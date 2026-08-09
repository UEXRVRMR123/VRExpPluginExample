// Copyright Zhang Shunlin. All Rights Reserved.

#include "CJKFontFallback.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Fonts/CompositeFont.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace CJKFontFallback::Tests
{
    static FString FindFontFilenameForCodepoint(const FCompositeFont &CompositeFont, const int32 Codepoint)
    {
        for (const FCompositeSubFont &SubFont : CompositeFont.SubTypefaces)
        {
            for (const FInt32Range &Range : SubFont.CharacterRanges)
            {
                if (Range.Contains(Codepoint) && !SubFont.Typeface.Fonts.IsEmpty())
                {
                    return FPaths::GetCleanFilename(SubFont.Typeface.Fonts[0].Font.GetFontFilename());
                }
            }
        }
        return FString();
    }
} // namespace CJKFontFallback::Tests

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCJKFontFallbackUnicodeRoutingTest, "CJKFontFallback.Fonts.Unicode17Routing",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCJKFontFallbackUnicodeRoutingTest::RunTest(const FString &Parameters)
{
    FCompositeFont CompositeFont;
    TestTrue(TEXT("The fallback ranges are added"),
             FCJKFontFallback::ApplyToCompositeFont(CompositeFont, ECJKFontFallbackMergePolicy::OverrideTargetRanges));

    struct FExpectedRoute
    {
        int32 Codepoint;
        const TCHAR *Filename;
    };

    const FExpectedRoute ExpectedRoutes[] = {
        {0x3400, TEXT("SourceHanSansSC-Regular.otf")}, {0x9F98, TEXT("SourceHanSansSC-Regular.otf")},
        {0xF900, TEXT("SourceHanSansSC-Regular.otf")}, {0xFA70, TEXT("PlangothicP1-Regular.ttf")},
        {0x20BB7, TEXT("PlangothicP1-Regular.ttf")},   {0x2A700, TEXT("PlangothicP1-Regular.ttf")},
        {0x2B740, TEXT("PlangothicP1-Regular.ttf")},   {0x2B820, TEXT("PlangothicP1-Regular.ttf")},
        {0x2CEB0, TEXT("PlangothicP1-Regular.ttf")},   {0x2EBF0, TEXT("PlangothicP1-Regular.ttf")},
        {0x2F800, TEXT("PlangothicP1-Regular.ttf")},   {0x30000, TEXT("PlangothicP2-Regular.ttf")},
        {0x31350, TEXT("PlangothicP2-Regular.ttf")},   {0x323B0, TEXT("PlangothicP2-Regular.ttf")}};

    for (const FExpectedRoute &Route : ExpectedRoutes)
    {
        TestEqual(FString::Printf(TEXT("U+%05X uses the expected font"), Route.Codepoint),
                  CJKFontFallback::Tests::FindFontFilenameForCodepoint(CompositeFont, Route.Codepoint),
                  FString(Route.Filename));
    }

    TestFalse(TEXT("Applying the same fallback ranges is idempotent"),
              FCJKFontFallback::ApplyToCompositeFont(CompositeFont, ECJKFontFallbackMergePolicy::OverrideTargetRanges));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCJKFontFallbackPreserveExistingRangesTest,
                                 "CJKFontFallback.Fonts.PreserveExistingRanges",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCJKFontFallbackPreserveExistingRangesTest::RunTest(const FString &Parameters)
{
    FCompositeFont CompositeFont;
    FCompositeSubFont ExistingSubFont;
    ExistingSubFont.Typeface.Fonts.Emplace(
        FName(TEXT("ExistingCJK")),
        FPaths::Combine(FPaths::EngineContentDir(), TEXT("Slate/Fonts/DroidSansFallback.ttf")), EFontHinting::Default,
        EFontLoadingPolicy::LazyLoad);
    ExistingSubFont.CharacterRanges.Add(FInt32Range::Inclusive(0x3400, 0x3400));
    CompositeFont.SubTypefaces.Add(MoveTemp(ExistingSubFont));

    TestTrue(
        TEXT("Fallback ranges are added around the existing range"),
        FCJKFontFallback::ApplyToCompositeFont(CompositeFont, ECJKFontFallbackMergePolicy::PreserveExistingRanges));

    TestEqual(TEXT("The existing U+3400 route is preserved"),
              CJKFontFallback::Tests::FindFontFilenameForCodepoint(CompositeFont, 0x3400),
              FString(TEXT("DroidSansFallback.ttf")));
    TestEqual(TEXT("The uncovered U+3401 route uses Source Han Sans"),
              CJKFontFallback::Tests::FindFontFilenameForCodepoint(CompositeFont, 0x3401),
              FString(TEXT("SourceHanSansSC-Regular.otf")));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
