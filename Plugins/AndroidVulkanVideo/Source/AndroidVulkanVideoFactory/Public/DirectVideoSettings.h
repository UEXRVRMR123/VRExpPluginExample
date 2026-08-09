// ------------------------------------------------
// Copyright Joe Marshall 2025- All Rights Reserved
// ------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Misc/ConfigCacheIni.h"

#include "DirectVideoSettings.generated.h"

UENUM(meta = (Bitflags))
enum class ELogBitmask : uint8
{
    Errors = 1 << 0,
    TextureWriting = 1 << 1,
    TextureWriting_VeryVerbose = 1 << 1,
    Decoding = 1 << 3,
    Decoding_VeryVerbose = 1 << 4,
    Vulkan = 1 << 5,
    Vulkan_VeryVerbose = 1 << 6,
};

ENUM_CLASS_FLAGS(ELogBitmask);

UCLASS(Config = Engine, defaultconfig, PerObjectConfig)
class ANDROIDVULKANVIDEOFACTORY_API UDirectVideoProjectLogSettings : public UObject
{
    GENERATED_BODY()

  public:
    UPROPERTY(config)
    FString LogDirectVideo = TEXT("VeryVerbose");

    UPROPERTY(config)
    FString LogDirectVideoMeshRenderer = TEXT("VeryVerbose");

    virtual void OverridePerObjectConfigSection(FString &SectionName) override
    {
        SectionName = TEXT("Core.Log");
    }
};

/**
 * DirectVideo Project Settings
 * These settings live in the Project Settings under Plugins -> DirectVideo
 * and enable you to add permissions to Android manifest for video access.
 */
UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "DirectVideo"))
class ANDROIDVULKANVIDEOFACTORY_API UDirectVideoProjectSettings : public UDeveloperSettings
{
    GENERATED_BODY()

  public:
    virtual void OverrideConfigSection(FString &ConfigName) override
    {
        ConfigName = TEXT("DirectVideo");
    }

    UDirectVideoProjectSettings(const FObjectInitializer &Initializer)
    {
        UnrealLogLevel = GConfig->GetStr(TEXT("Core.Log"), TEXT("LogDirectVideo"), GEngineIni);
    }

    // UDeveloperSettings interface
    virtual FName GetCategoryName() const override
    {
        return FName(TEXT("Plugins"));
    }

    /** If this is set to true, then you can set rates other than 0 or 1 on
     * videos which have sound, and it will time-stretch the audio.
     *
     * This takes slightly more CPU and adds a small amount of latency, so
     * you should set it off if you don't need time-stretching.
     * */
    UPROPERTY(config, EditAnywhere, Category = "Fallback Video",
              meta = (DisplayName = "Fallback video file name"))
    bool EnableAudioTimestretching = true;

    UPROPERTY(config, EditAnywhere, Category = "Timestretching",
              meta = (DisplayName = "Enable timestretch"))
    FString FallbackVideoPattern = TEXT("-fallback-{N}");

    /**
     * Output video format
     *
     * You probably don't want to change this unless you have very
     * specific requirements.
     *
     * CharBGR10A2 - 10 bit per channel BGR with 2 bit alpha (default)
     * CharBGRA - 8 bit per channel BGRA
     * CharRGBA - 8 bit per channel RGBA
     * RGBA16 - 16 bit per channel RGBA
     * FloatRGB - 32 bit float per channel RGB
     * FloatRGBA - 32 bit float per channel RGBA
     *
     * For most modern devices CharBGR10A2 has good performance and quality,
     * for very low spec devices CharBGRA may be more compatible but you will
     * get awful colour banding.
     *
     * FloatRGB and RGBA16 are not recommended for most current devices as
     * they are often not hardware accelerated.
     *
     *  */
    UPROPERTY(config, EditAnywhere, Category = "Display",
              meta = (DisplayName = "Output format", GetOptions = "GetOutputFormatOptions"))
    FString OutputFormat = TEXT("CharBGR10A2");

    /**
     * Pause video playback while the headset is removed.
     *
     * Application background transitions still suspend playback regardless
     * of this setting.
     */
    UPROPERTY(config, EditAnywhere, Category = "Playback",
              meta = (DisplayName = "Pause when headset is removed"))
    bool bPauseWhenHeadsetRemoved = false;

    /**
     * Logging bitmask (development build only)
     *
     * This setting controls logs from the internal DirectVideo
     * implementation library.
     * - Logs will be output to Unreal Logs (and to Android logcat)
     * - If you turn everything on to very verbose you will get
     *   absolutely tons of logs. Logs are output at the
     *   correct Unreal log level, so if you want to see everything
     *   then set all these on and set Unreal log level to
     *   VeryVerbose.
     *
     */
    UPROPERTY(config, EditAnywhere, Category = "Logging",
              meta = (BitMask, BitmaskEnum = "/Script/AndroidVulkanVideoFactory.ELogBitmask",
                      DisplayName = "Logging (development build only)"))
    int32 LogBitmask = (int32)(ELogBitmask::Errors | ELogBitmask::Decoding);

    /* Unreal logging level (development build only)
     * - Sets unreal logging level
     * - VeryVerbose will generate a lot of logs!
     */
    UPROPERTY(EditAnywhere, Category = "Logging",
              meta = (DisplayName = "Unreal Log Level", GetOptions = "GetVerbosityOptions"))
    FString UnrealLogLevel = TEXT("VeryVerbose");

  private:
    UPROPERTY()
    UDirectVideoProjectLogSettings *LogSettingsInstance;

    UFUNCTION()
    TArray<FString> GetOutputFormatOptions() const
    {
        return {"CharBGR10A2", "CharBGRA", "CharRGBA", "RGBA16", "FloatRGB", "FloatRGBA"};
    }

    UFUNCTION()
    TArray<FString> GetVerbosityOptions() const
    {
        return {"VeryVerbose", "Verbose", "Warning", "Log", "Error", "NoLogging"};
    }

#if WITH_EDITOR
  public:
    virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override
    {

        static const FName NameUnrealLogLevel =
            GET_MEMBER_NAME_CHECKED(UDirectVideoProjectSettings, UnrealLogLevel);
        FString PropertyName;

        if (PropertyChangedEvent.Property != nullptr)
        {
            PropertyName = PropertyChangedEvent.Property->GetName();
        }
        if (PropertyChangedEvent.Property != nullptr &&
            PropertyChangedEvent.Property->GetFName() == NameUnrealLogLevel)
        {
            if (LogSettingsInstance == nullptr)
            {
                LogSettingsInstance = NewObject<UDirectVideoProjectLogSettings>(
                    GetTransientPackage(), UDirectVideoProjectLogSettings::StaticClass());
                LogSettingsInstance->AddToRoot();
            }

            // create a sandbox FConfigCache which can write
            // to defaultengine.ini for the log settings
            // n.b. we have to do this because GConfig can't write
            // to them on UE 5.4 or above
            FString SettingsName = GetDefaultConfigFilename();
            FConfigFile *DefaultConfigFile = GConfig->FindConfigFile(SettingsName);
            FConfigCacheIni Config(EConfigCacheType::Temporary);
            FConfigFile &NewFile = Config.Add(SettingsName, FConfigFile());
            NewFile.SetString(TEXT("Core.Log"), TEXT("LogDirectVideo"), *UnrealLogLevel);
            NewFile.SetString(TEXT("Core.Log"), TEXT("LogDirectVideoMeshRenderer"),
                              *UnrealLogLevel);
            NewFile.UpdateSinglePropertyInSection(*SettingsName, TEXT("LogDirectVideo"),
                                                  TEXT("Core.Log"));
            NewFile.UpdateSinglePropertyInSection(*SettingsName, TEXT("LogDirectVideoMeshRenderer"),
                                                  TEXT("Core.Log"));
        }
        else
        {
            Super::PostEditChangeProperty(PropertyChangedEvent);
        }
    }
#endif
};
