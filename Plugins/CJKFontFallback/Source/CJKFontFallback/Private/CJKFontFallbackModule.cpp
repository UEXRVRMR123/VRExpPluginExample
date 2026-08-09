// Copyright Zhang Shunlin. All Rights Reserved.

#include "CJKFontFallback.h"

#include "CJKFontFallbackUE55Bridge.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Engine/Font.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontFaceInterface.h"
#include "Fonts/FontProviderInterface.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
#include "IPlatformCrypto.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PlatformCryptoTypes.h"
#include "Rendering/SlateRenderer.h"
#include "UObject/ObjectKey.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

DEFINE_LOG_CATEGORY_STATIC(LogCJKFontFallback, Log, All);

namespace CJKFontFallback::Private
{
    static constexpr int32 MinUnicodeCodepoint = 0x000000;
    static constexpr int32 MaxUnicodeCodepoint = 0x10FFFF;
    static constexpr float FontScanIntervalSeconds = 1.0f;
    static const TCHAR *DefaultRobotoObjectPath = TEXT("/Engine/EngineFonts/Roboto.Roboto");

    enum class EFontAssetKind : uint8
    {
        SourceHanSans,
        PlangothicP1,
        PlangothicP2
    };

    struct FClosedCodepointRange
    {
        int32 First = 0;
        int32 Last = 0;

        bool operator==(const FClosedCodepointRange &Other) const { return First == Other.First && Last == Other.Last; }
    };

    struct FFontAssetDescriptor
    {
        EFontAssetKind Kind = EFontAssetKind::SourceHanSans;
        FString Filename;
        FString AbsolutePath;
        FString ExpectedSha256;
        FName TypefaceName;
        FName EditorName;
    };

    static bool TryConvertToClosedRange(const FInt32Range &Range, FClosedCodepointRange &OutRange)
    {
        if (Range.IsEmpty())
        {
            return false;
        }

        const FInt32Range::BoundsType &LowerBound = Range.GetLowerBound();
        const FInt32Range::BoundsType &UpperBound = Range.GetUpperBound();

        int64 First = MinUnicodeCodepoint;
        int64 Last = MaxUnicodeCodepoint;

        if (LowerBound.IsClosed())
        {
            First = LowerBound.GetValue();
            if (LowerBound.IsExclusive())
            {
                ++First;
            }
        }

        if (UpperBound.IsClosed())
        {
            Last = UpperBound.GetValue();
            if (UpperBound.IsExclusive())
            {
                --Last;
            }
        }

        First = FMath::Clamp<int64>(First, MinUnicodeCodepoint, MaxUnicodeCodepoint);
        Last = FMath::Clamp<int64>(Last, MinUnicodeCodepoint, MaxUnicodeCodepoint);
        if (First > Last)
        {
            return false;
        }

        OutRange.First = static_cast<int32>(First);
        OutRange.Last = static_cast<int32>(Last);
        return true;
    }

    static TArray<FClosedCodepointRange> NormalizeRanges(TArray<FClosedCodepointRange> Ranges)
    {
        Ranges.RemoveAll([](const FClosedCodepointRange &Range) { return Range.First > Range.Last; });

        Ranges.Sort([](const FClosedCodepointRange &Left, const FClosedCodepointRange &Right)
                    { return Left.First == Right.First ? Left.Last < Right.Last : Left.First < Right.First; });

        TArray<FClosedCodepointRange> Result;
        for (const FClosedCodepointRange &Range : Ranges)
        {
            if (Result.IsEmpty() || static_cast<int64>(Range.First) > static_cast<int64>(Result.Last().Last) + 1)
            {
                Result.Add(Range);
            }
            else
            {
                Result.Last().Last = FMath::Max(Result.Last().Last, Range.Last);
            }
        }

        return Result;
    }

    static TArray<FClosedCodepointRange> ConvertToClosedRanges(const TArray<FInt32Range> &Ranges)
    {
        TArray<FClosedCodepointRange> Result;
        Result.Reserve(Ranges.Num());
        for (const FInt32Range &Range : Ranges)
        {
            FClosedCodepointRange ClosedRange;
            if (TryConvertToClosedRange(Range, ClosedRange))
            {
                Result.Add(ClosedRange);
            }
        }
        return NormalizeRanges(MoveTemp(Result));
    }

    static TArray<FInt32Range> ConvertToEngineRanges(const TArray<FClosedCodepointRange> &Ranges)
    {
        TArray<FInt32Range> Result;
        Result.Reserve(Ranges.Num());
        for (const FClosedCodepointRange &Range : Ranges)
        {
            Result.Add(FInt32Range::Inclusive(Range.First, Range.Last));
        }
        return Result;
    }

    static TArray<FClosedCodepointRange> SubtractRanges(const TArray<FClosedCodepointRange> &SourceRanges,
                                                        const TArray<FClosedCodepointRange> &ExclusionRanges)
    {
        TArray<FClosedCodepointRange> Result = NormalizeRanges(SourceRanges);
        const TArray<FClosedCodepointRange> NormalizedExclusions = NormalizeRanges(ExclusionRanges);

        for (const FClosedCodepointRange &Exclusion : NormalizedExclusions)
        {
            TArray<FClosedCodepointRange> NextResult;
            for (const FClosedCodepointRange &Source : Result)
            {
                if (Exclusion.Last < Source.First || Exclusion.First > Source.Last)
                {
                    NextResult.Add(Source);
                    continue;
                }

                if (Exclusion.First > Source.First)
                {
                    NextResult.Add({Source.First, Exclusion.First - 1});
                }
                if (Exclusion.Last < Source.Last)
                {
                    NextResult.Add({Exclusion.Last + 1, Source.Last});
                }
            }
            Result = MoveTemp(NextResult);
        }

        return NormalizeRanges(MoveTemp(Result));
    }

    static bool RangesOverlap(const TArray<FClosedCodepointRange> &LeftRanges,
                              const TArray<FClosedCodepointRange> &RightRanges)
    {
        for (const FClosedCodepointRange &Left : LeftRanges)
        {
            for (const FClosedCodepointRange &Right : RightRanges)
            {
                if (Left.First <= Right.Last && Right.First <= Left.Last)
                {
                    return true;
                }
            }
        }
        return false;
    }

    static TArray<FClosedCodepointRange> GetTargetRanges(const EFontAssetKind Kind)
    {
        switch (Kind)
        {
        case EFontAssetKind::SourceHanSans:
            return {{0x3400, 0x4DBF}, {0x4E00, 0x9FFF}, {0xF900, 0xFA6D}};

        case EFontAssetKind::PlangothicP1:
            return {{0xFA70, 0xFAD9},   {0x20000, 0x2A6DF}, {0x2A700, 0x2B73F}, {0x2B740, 0x2B81F},
                    {0x2B820, 0x2CEAF}, {0x2CEB0, 0x2EBEF}, {0x2EBF0, 0x2EE5F}, {0x2F800, 0x2FA1F}};

        case EFontAssetKind::PlangothicP2:
            return {{0x30000, 0x3134F}, {0x31350, 0x323AF}, {0x323B0, 0x3347F}};

        default:
            return {};
        }
    }

    static TArray<FClosedCodepointRange> GetAllTargetRanges()
    {
        TArray<FClosedCodepointRange> Result;
        for (const EFontAssetKind Kind :
             {EFontAssetKind::SourceHanSans, EFontAssetKind::PlangothicP1, EFontAssetKind::PlangothicP2})
        {
            Result.Append(GetTargetRanges(Kind));
        }
        return NormalizeRanges(MoveTemp(Result));
    }

    static bool AreSubFontsEquivalent(const FCompositeSubFont &Left, const FCompositeSubFont &Right)
    {
        if (!FMath::IsNearlyEqual(Left.ScalingFactor, Right.ScalingFactor) || Left.Cultures != Right.Cultures ||
            Left.CharacterRanges != Right.CharacterRanges || Left.Typeface.Fonts.Num() != Right.Typeface.Fonts.Num())
        {
            return false;
        }

#if WITH_EDITORONLY_DATA
        if (Left.EditorName != Right.EditorName)
        {
            return false;
        }
#endif

        for (int32 FontIndex = 0; FontIndex < Left.Typeface.Fonts.Num(); ++FontIndex)
        {
            const FTypefaceEntry &LeftEntry = Left.Typeface.Fonts[FontIndex];
            const FTypefaceEntry &RightEntry = Right.Typeface.Fonts[FontIndex];
            if (LeftEntry.Name != RightEntry.Name || LeftEntry.Font != RightEntry.Font)
            {
                return false;
            }
        }

        return true;
    }

    static bool AreSubFontArraysEquivalent(const TArray<FCompositeSubFont> &Left,
                                           const TArray<FCompositeSubFont> &Right)
    {
        if (Left.Num() != Right.Num())
        {
            return false;
        }

        for (int32 Index = 0; Index < Left.Num(); ++Index)
        {
            if (!AreSubFontsEquivalent(Left[Index], Right[Index]))
            {
                return false;
            }
        }
        return true;
    }

    static bool ComputeFileSha256(const FString &Filename, FString &OutSha256)
    {
        IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        TUniquePtr<IFileHandle> FileHandle(PlatformFile.OpenRead(*Filename));
        if (!FileHandle)
        {
            return false;
        }

        TUniquePtr<FEncryptionContext> CryptoContext = IPlatformCrypto::Get().CreateContext();
        if (!CryptoContext)
        {
            return false;
        }

        FSHA256Hasher Hasher = CryptoContext->CreateSHA256Hasher();
        TArray<uint8> Buffer;
        Buffer.SetNumUninitialized(1024 * 1024);

        int64 RemainingBytes = FileHandle->Size();
        while (RemainingBytes > 0)
        {
            const int64 BytesToRead = FMath::Min<int64>(RemainingBytes, Buffer.Num());
            if (!FileHandle->Read(Buffer.GetData(), BytesToRead) ||
                Hasher.Update(MakeArrayView(Buffer.GetData(), static_cast<int32>(BytesToRead))) !=
                    EPlatformCryptoResult::Success)
            {
                return false;
            }
            RemainingBytes -= BytesToRead;
        }

        TArray<uint8> Hash;
        Hash.SetNumUninitialized(FSHA256Hasher::OutputByteLength);
        if (Hasher.Finalize(MakeArrayView(Hash)) != EPlatformCryptoResult::Success)
        {
            return false;
        }

        OutSha256 = BytesToHex(Hash.GetData(), Hash.Num());
        return true;
    }
} // namespace CJKFontFallback::Private

class FCJKFontFallbackModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static FCJKFontFallbackModule &Get()
    {
        return FModuleManager::LoadModuleChecked<FCJKFontFallbackModule>(TEXT("CJKFontFallback"));
    }

    bool ApplyToCompositeFontAndFlush(FCompositeFont &CompositeFont, ECJKFontFallbackMergePolicy MergePolicy);
    bool ApplyToFontInfoAndFlush(FSlateFontInfo &FontInfo, ECJKFontFallbackMergePolicy MergePolicy);
    void RefreshLoadedFontsFromPublicApi();

private:
    using EFontAssetKind = CJKFontFallback::Private::EFontAssetKind;
    using FClosedCodepointRange = CJKFontFallback::Private::FClosedCodepointRange;
    using FFontAssetDescriptor = CJKFontFallback::Private::FFontAssetDescriptor;

    void ConfigureFontAssets();
    bool ValidateFontAssets();
    bool IsPluginSubFont(const FCompositeSubFont &SubFont) const;
    const FFontAssetDescriptor &GetFontAsset(EFontAssetKind Kind) const;
    FCompositeSubFont BuildPluginSubFont(EFontAssetKind Kind, const TArray<FClosedCodepointRange> &Ranges) const;
    bool ApplyToCompositeFontInternal(FCompositeFont &CompositeFont, ECJKFontFallbackMergePolicy MergePolicy);
    bool ApplyToFontInfoInternal(FSlateFontInfo &FontInfo, ECJKFontFallbackMergePolicy MergePolicy);
    bool PatchCoreStyle(bool bForce);
    bool PatchFontObject(UFont &Font, ECJKFontFallbackMergePolicy MergePolicy);
    bool PatchDefaultRoboto(bool bForce);
    bool ScanLoadedFonts(bool bForce);
    void RefreshInternal(bool bForce, bool bRetryValidation, const TCHAR *Reason);
    void FlushFontCache(const TCHAR *Reason) const;
    bool HandleTicker(float DeltaTime);
    void HandlePostLoadMap(UWorld *LoadedWorld);
#if WITH_EDITOR
    void HandleAssetLoaded(UObject *Asset);
#endif

    TArray<FFontAssetDescriptor> FontAssets;
    TSet<FObjectKey> PatchedFontObjects;
    FTSTicker::FDelegateHandle TickerHandle;
    FDelegateHandle PostLoadMapHandle;
#if WITH_EDITOR
    FDelegateHandle AssetLoadedHandle;
#endif
    IConsoleCommand *RefreshConsoleCommand = nullptr;
    bool bFontAssetsConfigured = false;
    bool bFontAssetsValid = false;
    bool bCoreStyleProcessed = false;
    bool bDefaultRobotoProcessed = false;
};

void FCJKFontFallbackModule::StartupModule()
{
    ConfigureFontAssets();
    bFontAssetsValid = ValidateFontAssets();

    RefreshConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("CJK.FontFallback.Refresh"), TEXT("Revalidates and reapplies the CJK fallback ranges to loaded fonts."),
        FConsoleCommandDelegate::CreateStatic(&FCJKFontFallback::RefreshLoadedFonts), ECVF_Default);

    if (!FApp::CanEverRender())
    {
        return;
    }

    RefreshInternal(false, false, TEXT("Startup"));

    PostLoadMapHandle =
        FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FCJKFontFallbackModule::HandlePostLoadMap);

#if WITH_EDITOR
    AssetLoadedHandle = FCoreUObjectDelegates::OnAssetLoaded.AddRaw(this, &FCJKFontFallbackModule::HandleAssetLoaded);
#endif

    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        TEXT("CJKFontFallback.FontScan"), CJKFontFallback::Private::FontScanIntervalSeconds,
        [this](const float DeltaTime) { return HandleTicker(DeltaTime); });
}

void FCJKFontFallbackModule::ShutdownModule()
{
    if (TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle.Reset();
    }

    if (PostLoadMapHandle.IsValid())
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
        PostLoadMapHandle.Reset();
    }

#if WITH_EDITOR
    if (AssetLoadedHandle.IsValid())
    {
        FCoreUObjectDelegates::OnAssetLoaded.Remove(AssetLoadedHandle);
        AssetLoadedHandle.Reset();
    }
#endif

    if (RefreshConsoleCommand)
    {
        IConsoleManager::Get().UnregisterConsoleObject(RefreshConsoleCommand);
        RefreshConsoleCommand = nullptr;
    }

    PatchedFontObjects.Reset();
    FontAssets.Reset();
    bFontAssetsConfigured = false;
    bFontAssetsValid = false;
}

void FCJKFontFallbackModule::ConfigureFontAssets()
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CJKFontFallback"));
    if (!Plugin.IsValid())
    {
        UE_LOG(LogCJKFontFallback, Error, TEXT("CJKFontFallback plugin descriptor could not be located."));
        return;
    }

    const FString FontDirectory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/Fonts"));
    FontAssets = {{EFontAssetKind::SourceHanSans, TEXT("SourceHanSansSC-Regular.otf"),
                   FPaths::Combine(FontDirectory, TEXT("SourceHanSansSC-Regular.otf")),
                   TEXT("F1D8611151880C6C336AABEAC4640EF434FA13CBFBF1FFE82D0A71B2A5637256"),
                   FName(TEXT("CJKFontFallback_SourceHanSansSC")),
                   FName(TEXT("CJK Font Fallback - Source Han Sans SC"))},
                  {EFontAssetKind::PlangothicP1, TEXT("PlangothicP1-Regular.ttf"),
                   FPaths::Combine(FontDirectory, TEXT("PlangothicP1-Regular.ttf")),
                   TEXT("550B5D0775B15405946B18F4843DF439A51E69508D7E6778D94C1F7A53DC5AD6"),
                   FName(TEXT("CJKFontFallback_PlangothicP1")), FName(TEXT("CJK Font Fallback - Plangothic P1"))},
                  {EFontAssetKind::PlangothicP2, TEXT("PlangothicP2-Regular.ttf"),
                   FPaths::Combine(FontDirectory, TEXT("PlangothicP2-Regular.ttf")),
                   TEXT("681933370ADFE0FC7253F77735275A82FEA09FE4F8ADBA907BDEB46C110DAF8F"),
                   FName(TEXT("CJKFontFallback_PlangothicP2")), FName(TEXT("CJK Font Fallback - Plangothic P2"))}};

    for (FFontAssetDescriptor &FontAsset : FontAssets)
    {
        FPaths::NormalizeFilename(FontAsset.AbsolutePath);
    }
    bFontAssetsConfigured = true;
}

bool FCJKFontFallbackModule::ValidateFontAssets()
{
    if (!bFontAssetsConfigured || FontAssets.Num() != 3)
    {
        return false;
    }

    bool bAllValid = true;
    for (const FFontAssetDescriptor &FontAsset : FontAssets)
    {
        FString ActualSha256;
        if (!CJKFontFallback::Private::ComputeFileSha256(FontAsset.AbsolutePath, ActualSha256))
        {
            UE_LOG(LogCJKFontFallback, Error, TEXT("Unable to read or hash font file: %s"), *FontAsset.AbsolutePath);
            bAllValid = false;
            continue;
        }

        if (!ActualSha256.Equals(FontAsset.ExpectedSha256, ESearchCase::IgnoreCase))
        {
            UE_LOG(LogCJKFontFallback, Error, TEXT("SHA-256 mismatch for %s. Expected %s, got %s."),
                   *FontAsset.AbsolutePath, *FontAsset.ExpectedSha256, *ActualSha256);
            bAllValid = false;
        }
    }

    if (bAllValid)
    {
        UE_LOG(LogCJKFontFallback, Log,
               TEXT("Validated Source Han Sans SC 2.005R and Plangothic V2.9.5795 font assets."));
    }
    else
    {
        UE_LOG(LogCJKFontFallback, Error, TEXT("Font validation failed. No font chain will be modified."));
    }

    return bAllValid;
}

const FCJKFontFallbackModule::FFontAssetDescriptor &
FCJKFontFallbackModule::GetFontAsset(const EFontAssetKind Kind) const
{
    const int32 Index = static_cast<int32>(Kind);
    check(FontAssets.IsValidIndex(Index));
    return FontAssets[Index];
}

bool FCJKFontFallbackModule::IsPluginSubFont(const FCompositeSubFont &SubFont) const
{
    if (SubFont.Typeface.Fonts.Num() != 1)
    {
        return false;
    }

    const FTypefaceEntry &Entry = SubFont.Typeface.Fonts[0];
    for (const FFontAssetDescriptor &FontAsset : FontAssets)
    {
        if (Entry.Name == FontAsset.TypefaceName &&
            FPaths::IsSamePath(Entry.Font.GetFontFilename(), FontAsset.AbsolutePath))
        {
            return true;
        }
    }
    return false;
}

FCompositeSubFont FCJKFontFallbackModule::BuildPluginSubFont(const EFontAssetKind Kind,
                                                             const TArray<FClosedCodepointRange> &Ranges) const
{
    const FFontAssetDescriptor &FontAsset = GetFontAsset(Kind);

    FCompositeSubFont SubFont;
    SubFont.Typeface.Fonts.Emplace(FontAsset.TypefaceName, FontAsset.AbsolutePath, EFontHinting::Default,
                                   EFontLoadingPolicy::LazyLoad);
    SubFont.ScalingFactor = 1.0f;
    SubFont.CharacterRanges = CJKFontFallback::Private::ConvertToEngineRanges(Ranges);
    SubFont.Cultures.Reset();
#if WITH_EDITORONLY_DATA
    SubFont.EditorName = FontAsset.EditorName;
#endif
    return SubFont;
}

bool FCJKFontFallbackModule::ApplyToCompositeFontInternal(FCompositeFont &CompositeFont,
                                                          const ECJKFontFallbackMergePolicy MergePolicy)
{
    if (!bFontAssetsValid)
    {
        return false;
    }

    using namespace CJKFontFallback::Private;

    TArray<FCompositeSubFont> NewSubFonts;
    NewSubFonts.Reserve(CompositeFont.SubTypefaces.Num() + 3);
    for (const FCompositeSubFont &ExistingSubFont : CompositeFont.SubTypefaces)
    {
        if (!IsPluginSubFont(ExistingSubFont))
        {
            NewSubFonts.Add(ExistingSubFont);
        }
    }

    const TArray<FClosedCodepointRange> AllTargetRanges = GetAllTargetRanges();
    TArray<FClosedCodepointRange> ExistingCultureIndependentRanges;

    if (MergePolicy == ECJKFontFallbackMergePolicy::OverrideTargetRanges)
    {
        for (int32 Index = NewSubFonts.Num() - 1; Index >= 0; --Index)
        {
            FCompositeSubFont &ExistingSubFont = NewSubFonts[Index];
            if (!ExistingSubFont.Cultures.IsEmpty())
            {
                continue;
            }

            const TArray<FClosedCodepointRange> ExistingRanges = ConvertToClosedRanges(ExistingSubFont.CharacterRanges);
            if (!RangesOverlap(ExistingRanges, AllTargetRanges))
            {
                continue;
            }

            const TArray<FClosedCodepointRange> TrimmedRanges = SubtractRanges(ExistingRanges, AllTargetRanges);
            if (TrimmedRanges.IsEmpty())
            {
                NewSubFonts.RemoveAt(Index);
            }
            else
            {
                ExistingSubFont.CharacterRanges = ConvertToEngineRanges(TrimmedRanges);
            }
        }
    }
    else
    {
        for (const FCompositeSubFont &ExistingSubFont : NewSubFonts)
        {
            if (ExistingSubFont.Cultures.IsEmpty())
            {
                ExistingCultureIndependentRanges.Append(ConvertToClosedRanges(ExistingSubFont.CharacterRanges));
            }
        }
        ExistingCultureIndependentRanges = NormalizeRanges(MoveTemp(ExistingCultureIndependentRanges));
    }

    for (const EFontAssetKind Kind :
         {EFontAssetKind::SourceHanSans, EFontAssetKind::PlangothicP1, EFontAssetKind::PlangothicP2})
    {
        TArray<FClosedCodepointRange> PluginRanges = GetTargetRanges(Kind);
        if (MergePolicy == ECJKFontFallbackMergePolicy::PreserveExistingRanges)
        {
            PluginRanges = SubtractRanges(PluginRanges, ExistingCultureIndependentRanges);
        }

        if (!PluginRanges.IsEmpty())
        {
            NewSubFonts.Add(BuildPluginSubFont(Kind, PluginRanges));
        }
    }

    if (AreSubFontArraysEquivalent(CompositeFont.SubTypefaces, NewSubFonts))
    {
        return false;
    }

    CompositeFont.SubTypefaces = MoveTemp(NewSubFonts);
#if WITH_EDITORONLY_DATA
    CompositeFont.MakeDirty();
#endif
    return true;
}

bool FCJKFontFallbackModule::ApplyToFontInfoInternal(FSlateFontInfo &FontInfo,
                                                     const ECJKFontFallbackMergePolicy MergePolicy)
{
    if (const UFont *Font = Cast<UFont>(FontInfo.FontObject.Get()))
    {
        UFont *MutableFont = const_cast<UFont *>(Font);
        return PatchFontObject(*MutableFont, MergePolicy);
    }

    const FCompositeFont *ExistingCompositeFont = FontInfo.CompositeFont.Get();
    if (!ExistingCompositeFont && FontInfo.FontObject)
    {
        if (const IFontProviderInterface *FontProvider = Cast<IFontProviderInterface>(FontInfo.FontObject.Get()))
        {
            ExistingCompositeFont = FontProvider->GetCompositeFont();
        }
    }

    TSharedRef<FStandaloneCompositeFont> WrappedFont = MakeShared<FStandaloneCompositeFont>();
    if (ExistingCompositeFont)
    {
        static_cast<FCompositeFont &>(*WrappedFont) = *ExistingCompositeFont;
    }
    else if (FontInfo.FontObject && Cast<IFontFaceInterface>(FontInfo.FontObject.Get()))
    {
        FTypefaceEntry &DefaultEntry = WrappedFont->DefaultTypeface.Fonts.AddDefaulted_GetRef();
        DefaultEntry.Name = FontInfo.TypefaceFontName.IsNone() ? FName(TEXT("Regular")) : FontInfo.TypefaceFontName;
        DefaultEntry.Font = FFontData(FontInfo.FontObject.Get());
    }
    else
    {
        UE_LOG(LogCJKFontFallback, Warning,
               TEXT("ApplyToFontInfo received font data that cannot provide a composite or font face."));
        return false;
    }

    if (!ApplyToCompositeFontInternal(*WrappedFont, MergePolicy))
    {
        return false;
    }

    FontInfo.FontObject = nullptr;
    FontInfo.CompositeFont = WrappedFont;
    return true;
}

bool FCJKFontFallbackModule::PatchCoreStyle(const bool bForce)
{
    if ((bCoreStyleProcessed && !bForce) || !FSlateApplication::IsInitialized())
    {
        return false;
    }

    FCompositeFont *CoreStyleFont = CJKFontFallback::UE55Bridge::GetMutableCoreStyleDefaultFont();
    if (!CoreStyleFont)
    {
        return false;
    }

    const bool bChanged =
        ApplyToCompositeFontInternal(*CoreStyleFont, ECJKFontFallbackMergePolicy::OverrideTargetRanges);
    bCoreStyleProcessed = true;
    return bChanged;
}

bool FCJKFontFallbackModule::PatchFontObject(UFont &Font, const ECJKFontFallbackMergePolicy MergePolicy)
{
    if (Font.GetCompositeFont() == nullptr)
    {
        return false;
    }

    return ApplyToCompositeFontInternal(Font.CompositeFont, MergePolicy);
}

bool FCJKFontFallbackModule::PatchDefaultRoboto(const bool bForce)
{
    if (bDefaultRobotoProcessed && !bForce)
    {
        return false;
    }

    UFont *DefaultRoboto = LoadObject<UFont>(nullptr, CJKFontFallback::Private::DefaultRobotoObjectPath);
    if (!DefaultRoboto)
    {
        UE_LOG(LogCJKFontFallback, Warning, TEXT("Unable to load %s."),
               CJKFontFallback::Private::DefaultRobotoObjectPath);
        return false;
    }

    const bool bChanged = PatchFontObject(*DefaultRoboto, ECJKFontFallbackMergePolicy::OverrideTargetRanges);
    PatchedFontObjects.Add(FObjectKey(DefaultRoboto));
    bDefaultRobotoProcessed = true;
    return bChanged;
}

bool FCJKFontFallbackModule::ScanLoadedFonts(const bool bForce)
{
    bool bChanged = false;
    ForEachObjectOfClass(UFont::StaticClass(),
                         [this, bForce, &bChanged](UObject *Object)
                         {
                             UFont *Font = Cast<UFont>(Object);
                             if (!IsValid(Font) || Font->GetCompositeFont() == nullptr)
                             {
                                 return;
                             }

                             const FObjectKey ObjectKey(Font);
                             if (!bForce && PatchedFontObjects.Contains(ObjectKey))
                             {
                                 return;
                             }

                             const ECJKFontFallbackMergePolicy MergePolicy =
                                 Font->GetPathName().Equals(CJKFontFallback::Private::DefaultRobotoObjectPath,
                                                            ESearchCase::CaseSensitive)
                                     ? ECJKFontFallbackMergePolicy::OverrideTargetRanges
                                     : ECJKFontFallbackMergePolicy::PreserveExistingRanges;

                             bChanged |= PatchFontObject(*Font, MergePolicy);
                             PatchedFontObjects.Add(ObjectKey);
                         });
    return bChanged;
}

void FCJKFontFallbackModule::RefreshInternal(const bool bForce, const bool bRetryValidation, const TCHAR *Reason)
{
    if (!IsInGameThread() || !FApp::CanEverRender())
    {
        return;
    }

    if (!bFontAssetsValid)
    {
        if (!bRetryValidation)
        {
            return;
        }
        bFontAssetsValid = ValidateFontAssets();
        if (!bFontAssetsValid)
        {
            return;
        }
    }

    bool bChanged = PatchCoreStyle(bForce);
    bChanged |= PatchDefaultRoboto(bForce);
    bChanged |= ScanLoadedFonts(bForce);

    if (bChanged)
    {
        FlushFontCache(Reason);
        UE_LOG(LogCJKFontFallback, Log, TEXT("Applied Unicode 17 CJK fallback fonts (%s)."), Reason);
    }
}

void FCJKFontFallbackModule::FlushFontCache(const TCHAR *Reason) const
{
    if (!FSlateApplication::IsInitialized())
    {
        return;
    }

    if (FSlateRenderer *Renderer = FSlateApplication::Get().GetRenderer())
    {
        Renderer->FlushFontCache(FString::Printf(TEXT("CJKFontFallback: %s"), Reason));
    }
}

bool FCJKFontFallbackModule::HandleTicker(const float DeltaTime)
{
    RefreshInternal(false, false, TEXT("Periodic font scan"));
    return true;
}

void FCJKFontFallbackModule::HandlePostLoadMap(UWorld *LoadedWorld)
{
    RefreshInternal(false, false, TEXT("PostLoadMapWithWorld"));
}

#if WITH_EDITOR
void FCJKFontFallbackModule::HandleAssetLoaded(UObject *Asset)
{
    if (!IsInGameThread() || !bFontAssetsValid)
    {
        return;
    }

    UFont *Font = Cast<UFont>(Asset);
    if (!Font || Font->GetCompositeFont() == nullptr)
    {
        return;
    }

    const ECJKFontFallbackMergePolicy MergePolicy =
        Font->GetPathName().Equals(CJKFontFallback::Private::DefaultRobotoObjectPath, ESearchCase::CaseSensitive)
            ? ECJKFontFallbackMergePolicy::OverrideTargetRanges
            : ECJKFontFallbackMergePolicy::PreserveExistingRanges;

    if (PatchFontObject(*Font, MergePolicy))
    {
        FlushFontCache(TEXT("OnAssetLoaded"));
    }
    PatchedFontObjects.Add(FObjectKey(Font));
}
#endif

bool FCJKFontFallbackModule::ApplyToCompositeFontAndFlush(FCompositeFont &CompositeFont,
                                                          const ECJKFontFallbackMergePolicy MergePolicy)
{
    if (!bFontAssetsValid)
    {
        bFontAssetsValid = ValidateFontAssets();
    }

    const bool bChanged = ApplyToCompositeFontInternal(CompositeFont, MergePolicy);
    if (bChanged)
    {
        FlushFontCache(TEXT("ApplyToCompositeFont"));
    }
    return bChanged;
}

bool FCJKFontFallbackModule::ApplyToFontInfoAndFlush(FSlateFontInfo &FontInfo,
                                                     const ECJKFontFallbackMergePolicy MergePolicy)
{
    if (!bFontAssetsValid)
    {
        bFontAssetsValid = ValidateFontAssets();
    }

    const bool bChanged = ApplyToFontInfoInternal(FontInfo, MergePolicy);
    if (bChanged)
    {
        FlushFontCache(TEXT("ApplyToFontInfo"));
    }
    return bChanged;
}

void FCJKFontFallbackModule::RefreshLoadedFontsFromPublicApi() { RefreshInternal(true, true, TEXT("Manual refresh")); }

bool FCJKFontFallback::ApplyToCompositeFont(FCompositeFont &CompositeFont,
                                            const ECJKFontFallbackMergePolicy MergePolicy)
{
    if (!IsInGameThread())
    {
        UE_LOG(LogCJKFontFallback, Error, TEXT("ApplyToCompositeFont must be called on the game thread."));
        return false;
    }

    return FCJKFontFallbackModule::Get().ApplyToCompositeFontAndFlush(CompositeFont, MergePolicy);
}

bool FCJKFontFallback::ApplyToFontInfo(FSlateFontInfo &FontInfo, const ECJKFontFallbackMergePolicy MergePolicy)
{
    if (!IsInGameThread())
    {
        UE_LOG(LogCJKFontFallback, Error, TEXT("ApplyToFontInfo must be called on the game thread."));
        return false;
    }

    return FCJKFontFallbackModule::Get().ApplyToFontInfoAndFlush(FontInfo, MergePolicy);
}

void FCJKFontFallback::RefreshLoadedFonts()
{
    if (!IsInGameThread())
    {
        AsyncTask(ENamedThreads::GameThread, [] { FCJKFontFallback::RefreshLoadedFonts(); });
        return;
    }

    FCJKFontFallbackModule::Get().RefreshLoadedFontsFromPublicApi();
}

IMPLEMENT_MODULE(FCJKFontFallbackModule, CJKFontFallback)
