// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// The main IMediaPlayer implementation. This is a
// shim which provides Unreal interfaces, and then
// defers everything vulkan related to the main
// AndroidVulkanVideoImpl class.
// ------------------------------------------------

#include "AndroidVulkanMediaPlayer.h"
#include "AndroidVulkanTextureSample.h"

#include "IVulkanImpl.h"

#include "VideoMediaSampleHolder.h"

#include "UnrealArchiveFileSource.h"
#include "UnrealAudioOut.h"

#include "Android/AndroidJavaEnv.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "IPlatformFilePak.h"

#include "Misc/MessageDialog.h"

#include "Internationalization/Text.h"

#include "UnrealLogging.h"

#define LOCTEXT_NAMESPACE "FAndroidVulkanVideoModule"

#include <dlfcn.h>

#include "Async/Async.h"

extern JavaVM *GJavaVM;

FAndroidVulkanMediaPlayer::FAndroidVulkanMediaPlayer(IMediaEventSink &InEventSink)
    : SampleQueue(MakeShared<FVideoMediaSampleHolder, ESPMode::ThreadSafe>()),
      EventSink(InEventSink), Suspended(false), Impl(NULL), AudioOut(NULL), CreateImpl(NULL),
      DestroyImpl(NULL), ImplDLL(NULL), LoopIndex(0), SeekIndex(0),
      FallbackVideoPattern(TEXT("-fallback-{N}"))
{
    ImplDLL = dlopen("libVkLayer_DirectVideo_"
                     "38"
                     ".so",
                     RTLD_NOW | RTLD_LOCAL);
    if (ImplDLL == NULL)
    {
        char *ErrorText = dlerror();
        ;
        FString ErrorMsg;
        if (ErrorText)
        {
            ErrorMsg = ErrorText;
        }
        else
        {
            ErrorMsg = "Unknown error";
        }
        UE_LOGFMT(LogDirectVideo, Error,
                  "Failed to load libVkLayer_DirectVideo_"
                  "38"
                  ".so: {0}",
                  ErrorMsg);
        Logger.ForceErrorLog("Failed to load libVkLayer_DirectVideo_"
                             "38"
                             ".so:");
        return;
    }
    CreateImpl = (CreateImplType)(dlsym(ImplDLL, "createImpl"));
    DestroyImpl = (DestroyImplType)(dlsym(ImplDLL, "destroyImpl"));
    if (CreateImpl == NULL || DestroyImpl == NULL)
    {
        Logger.ForceErrorLog("Failed to load create or destroy from libVkLayer_DirectVideo.so");
        return;
    }
    Impl = CreateImpl();
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "Failed to create Impl");
        return;
    }
    Impl->setJVMPointer((void *)GJavaVM);
    // get log format bitmask from defaultEngine.ini
    int32 LogVisibility;
    if (GConfig->GetInt(TEXT("DirectVideo"), TEXT("LogBitmask"), LogVisibility, GEngineIni))
    {
        Logger.SetLogVisibilityBitmask((int64)LogVisibility);
    }
    else
    {
        Logger.SetLogVisibilityBitmask(ILogger::LogTypes::ALL_LOGS_BITMASK);
    }
    if (GConfig->GetString(TEXT("DirectVideo"), TEXT("FallbackVideoPattern"), FallbackVideoPattern,
                           GEngineIni))
    {
        UE_LOGFMT(LogDirectVideo, Log, "Using fallback video pattern: {0}", FallbackVideoPattern);
    }

    AudioTimestretchingEnabled = true;
    if (GConfig->GetBool(TEXT("DirectVideo"), TEXT("EnableAudioTimestretching"),
                         AudioTimestretchingEnabled, GEngineIni))
    {
        UE_LOGFMT(LogDirectVideo, Log, "Audio timestretching is {0}",
                  AudioTimestretchingEnabled ? TEXT("enabled") : TEXT("disabled"));
    }
    Impl->enableTimeStretching(AudioTimestretchingEnabled);

    Impl->setDataCallback(this);
    Impl->setLogger(&Logger);
    Impl->setLooping(Looping);
    // get output format from defaultEngine.ini
    FString OutFormat;
    EMediaTextureSampleFormat SampleFormat = EMediaTextureSampleFormat::CharBGR10A2;
    if (GConfig->GetString(TEXT("DirectVideo"), TEXT("OutputFormat"), OutFormat, GEngineIni))
    {
        if (OutFormat.Equals(TEXT("CharBGR10A2"), ESearchCase::IgnoreCase))
        {
            SampleFormat = EMediaTextureSampleFormat::CharBGR10A2;
        }
        else if (OutFormat.Equals(TEXT("CharBGRA"), ESearchCase::IgnoreCase))
        {
            SampleFormat = EMediaTextureSampleFormat::CharBGRA;
        }

        else if (OutFormat.Equals(TEXT("CharRGBA"), ESearchCase::IgnoreCase))
        {
            SampleFormat = EMediaTextureSampleFormat::CharRGBA;
        }
        else if (OutFormat.Equals(TEXT("RGBA16"), ESearchCase::IgnoreCase))
        {
            SampleFormat = EMediaTextureSampleFormat::RGBA16;
        }

        else if (OutFormat.Equals(TEXT("FloatRGB"), ESearchCase::IgnoreCase))
        {
            SampleFormat = EMediaTextureSampleFormat::FloatRGB;
        }
        else if (OutFormat.Equals(TEXT("FloatRGBA"), ESearchCase::IgnoreCase))
        {
            SampleFormat = EMediaTextureSampleFormat::FloatRGBA;
        }
        else
        {
            UE_LOGFMT(LogDirectVideo, Error, "Bad sample format in defaultEngine.ini");
        }
    }

    AndroidVulkanTextureSample::SetVideoFormat(SampleFormat);

    AudioOut = new UnrealAudioOut(this, AudioTimestretchingEnabled);
    Impl->setAudioOut(AudioOut);
    PlayState = EMediaState::Closed;
    CurInfo.Empty();
    DelegateEnterBackground = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(
        this, &FAndroidVulkanMediaPlayer::OnEnterBackground);
    DelegateEnterForeground = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(
        this, &FAndroidVulkanMediaPlayer::OnEnterForeground);

    bool bPauseWhenHeadsetRemoved = false;
    GConfig->GetBool(TEXT("DirectVideo"), TEXT("bPauseWhenHeadsetRemoved"),
                     bPauseWhenHeadsetRemoved, GEngineIni);
    if (bPauseWhenHeadsetRemoved)
    {
        DelegateHeadsetRemoved = FCoreDelegates::VRHeadsetRemovedFromHead.AddRaw(
            this, &FAndroidVulkanMediaPlayer::OnEnterBackground);
        DelegateHeadsetPutOn = FCoreDelegates::VRHeadsetPutOnHead.AddRaw(
            this, &FAndroidVulkanMediaPlayer::OnEnterForeground);
    }

    SeekIndex = 0;
    LoopIndex = 0;
    SentBlankFrame = false;
    Seeking = false;
}

FAndroidVulkanMediaPlayer::~FAndroidVulkanMediaPlayer()
{
    Close();
    SentBlankFrame = true;
    if (DelegateEnterBackground.IsValid())
    {
        FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(DelegateEnterBackground);
    }
    if (DelegateEnterForeground.IsValid())
    {
        FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(DelegateEnterForeground);
    }
    if (DelegateHeadsetPutOn.IsValid())
    {
        FCoreDelegates::VRHeadsetPutOnHead.Remove(DelegateHeadsetPutOn);
    }
    if (DelegateHeadsetRemoved.IsValid())
    {
        FCoreDelegates::VRHeadsetRemovedFromHead.Remove(DelegateHeadsetRemoved);
    }

    if (Impl != NULL && DestroyImpl != NULL)
    {
        DestroyImpl(Impl);
        Impl = NULL;
    }
    if (AudioOut != NULL)
    {
        delete AudioOut;
        AudioOut = NULL;
    }

    if (ImplDLL != NULL)
    {
        dlclose(ImplDLL);
        ImplDLL = NULL;
    }
}

// IMediaPlayer
// --------------------------------
void FAndroidVulkanMediaPlayer::Close()
{
    SentBlankFrame = false;

    if (PlayState != EMediaState::Closed)
    {
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "Media player close called");
        SampleQueue->FlushSamples();
        VideoSamplePool.ReleaseEverything();
        SeekIndex = 0;
        Seeking = false;
        LoopIndex = 0;
        EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
        EventSink.ReceiveMediaEvent(EMediaEvent::MediaClosed);
        PlayState = EMediaState::Closed;
        // close the Impl last because it will
        // kill all the texture images that we release in the pool above
        if (Impl != NULL)
        {
            Impl->close();
        }
    }
}
FString FAndroidVulkanMediaPlayer::GetInfo() const
{
    return CurInfo;
}
FGuid FAndroidVulkanMediaPlayer::GetPlayerPluginGUID() const
{
    static FGuid OurGUID(0x9bf2d7c6, 0xb2b84d26, 0xb6ae5a3a, 0xc9883569);
    return OurGUID;
}
FString FAndroidVulkanMediaPlayer::GetStats() const
{
    return TEXT("Not implemented");
}
FString FAndroidVulkanMediaPlayer::GetUrl() const
{
    return VideoURL;
}
bool FAndroidVulkanMediaPlayer::Open(const FString &Url, const IMediaOptions *Options)
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "Can't open video - Impl is NULL");
        return false;
    }
    Close();
    bool Started = false;
    FString CachedFallbackValue = GetFallbackCacheValue(Url);
    if (CachedFallbackValue.Len() != 0)
    {
        UE_LOG(LogDirectVideo, VeryVerbose, TEXT("Trying cached fallback video url: %s"),
               *CachedFallbackValue);
        Started = _OpenInternal(CachedFallbackValue);
        if (Started)
        {
            VideoURL = CachedFallbackValue;
        }
    }
    HasFallbackLeft = !FallbackVideoPattern.IsEmpty();
    // when we are trying URLs, if the last one we can try
    // is too big but exists, we retry with that after we
    // try everything else as some devices will play larger
    //  videos than they report (Quest 3 I am looking at you...)
    FString LastURLThatWasTooBig;
    for (int c = 0; c < 99 && Started == false; c++)
    {
        FString TryUrl = Url;
        if (c > 0 && !FallbackVideoPattern.IsEmpty())
        {
            FString Suffix = FallbackVideoPattern;
            Suffix = Suffix.Replace(TEXT("{N}"), *FString::FromInt(c));
            FString Base;
            FString Extension;
            Url.Split(TEXT("."), &Base, &Extension, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

            TryUrl = Base + Suffix + TEXT(".") + Extension;
            UE_LOGFMT(LogDirectVideo, Log, "Trying fallback video url: {0}", *TryUrl);
        }
        VideoTooLargeReported = false;
        Started = _OpenInternal(TryUrl);
        if (Started)
        {
            VideoURL = TryUrl;
            if (c > 0)
            {
                UpdateFallbackCacheValue(Url, TryUrl);
            }
            break;
        }
        if (!Started && VideoTooLargeReported)
        {
            LastURLThatWasTooBig = TryUrl;
        }
        if (!VideoTooLargeReported && !FallbackVideoPattern.IsEmpty())
        {
            // if video exists, but is too large,
            // try next fallback if it is there,
            // otherwise drop out
            break;
        }
    }
    if (!Started && !LastURLThatWasTooBig.IsEmpty() && HasFallbackLeft == true)
    {
        HasFallbackLeft = false;
        UE_LOGFMT(LogDirectVideo, Log, "Trying last url that was too big: {0}",
                  *LastURLThatWasTooBig);
        Started = _OpenInternal(LastURLThatWasTooBig);
        if (Started)
        {
            VideoURL = LastURLThatWasTooBig;
        }
    }
    if (Started)
    {
        PlayState = EMediaState::Stopped;
        EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
        EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpened);
        UE_LOGFMT(LogDirectVideo, Verbose, "Opening {0}", *Url);
        SeekIndex = 0;
        LoopIndex = 0;
        Seeking = false;
    }
    else
    {
        // couldn't open
        UE_LOGFMT(LogDirectVideo, Error, "Can't open video file {0}", *Url);
        EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
    }
    return Started;
}

bool FAndroidVulkanMediaPlayer::_OpenInternal(const FString &Url)
{
    UE_LOGFMT(LogDirectVideo, VeryVerbose, "Try open Url {0}", Url);
    SentBlankFrame = false;
    bool Started = false;
    FString fullPath = Url;
    if (fullPath.StartsWith("file://"))
    {
        fullPath = fullPath.RightChop(7);
    }
    if (fullPath.Contains("://"))
    {
        // a (non-file url)
        Started = Impl->startVideoURL(TCHAR_TO_UTF8(*fullPath), false);
    }
    else
    {
        // a file path - check if it is local or not
        if (fullPath.StartsWith("./"))
        {
            fullPath = FPaths::ProjectContentDir() + fullPath.RightChop(2);
        }
        FPaths::NormalizeFilename(fullPath);
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "Start video {0}", fullPath);
        IAndroidPlatformFile &PlatformFile = IAndroidPlatformFile::GetPlatformPhysical();
        if (PlatformFile.FileExists(*fullPath))
        {
            int64 FileOffset = PlatformFile.FileStartOffset(*fullPath);
            int64 FileSize = PlatformFile.FileSize(*fullPath);
            FString FileRootPath = PlatformFile.FileRootPath(*fullPath);
            UE_LOGFMT(LogDirectVideo, VeryVerbose, "File exists: {0} {1} {2} {3}", FileRootPath,
                      FileOffset, FileSize, fullPath);
            Started =
                Impl->startVideoFile(TCHAR_TO_UTF8(*FileRootPath), FileOffset, FileSize, false);
        }
        else
        {
            // check if the file exists in an archive / encrypted etc.
            FPakPlatformFile *PakPlatformFile =
                (FPakPlatformFile *)(FPlatformFileManager::Get().FindPlatformFile(
                    FPakPlatformFile::GetTypeName()));

            TRefCountPtr<FPakFile> PakFile;
            FPakEntry FileEntry;
            if (PakPlatformFile != nullptr &&
                PakPlatformFile->FindFileInPakFiles(*fullPath, &PakFile, &FileEntry))
            {
                // we have the file in a pak file
                // use a custom media data source to read it
                // n.b. if pak file is uncompressed and the file isn't encrypted we could skip this
                // step but this should work whatever
                TSharedRef<FArchive, ESPMode::ThreadSafe> Archive =
                    MakeShareable(IFileManager::Get().CreateFileReader(*fullPath));
                UnrealArchiveFileSource *fs = new UnrealArchiveFileSource(Archive);
                Started = Impl->startVideoCustomSource(fs, false);
            }
        }
    }
    return Started;
}

bool FAndroidVulkanMediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe> &Archive,
                                     const FString &OriginalUrl, const IMediaOptions *Options)
{
    UE_LOGFMT(LogDirectVideo, Error, "Opening archive {0} not supported", *OriginalUrl);
    return false;
}
void FAndroidVulkanMediaPlayer::SetGuid(const FGuid &Guid)
{
    PlayerGUID = Guid;
}

void FAndroidVulkanMediaPlayer::TickFetch(FTimespan DeltaTime, FTimespan Timecode)
{
}

void FAndroidVulkanMediaPlayer::TickInput(FTimespan DeltaTime, FTimespan Timecode)
{

    UE_LOGFMT(LogDirectVideo, VeryVerbose, "TickInput start");
    if (Impl == NULL)
    {
        return;
    }

    HasVideoThisFrame = false;
    VideoSamplePool.Tick();
    if (!SentBlankFrame && Impl->numVideoTracks() == 0)
    {
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "blank frame");
        SentBlankFrame = true;
        auto textureSample = VideoSamplePool.AcquireShared();
        textureSample->InitNoVideo();
        SampleQueue->AddVideo(textureSample);
    }
    UE_LOGFMT(LogDirectVideo, VeryVerbose, "TickInput dt:{0} tc {1} num {2}", DeltaTime.GetTicks(),
              Timecode.GetTicks(), SampleQueue->NumVideoSamples());
}

bool FAndroidVulkanMediaPlayer::GetPlayerFeatureFlag(EFeatureFlag flag) const
{

    switch (flag)
    {
    case EFeatureFlag::PlayerUsesInternalFlushOnSeek:
        return true;
    case EFeatureFlag::AlwaysPullNewestVideoFrame:
        return true;
    case EFeatureFlag::UsePlaybackTimingV2:
        return true;
    case EFeatureFlag::IsTrackSwitchSeamless:
        return true;
    case EFeatureFlag::UseRealtimeWithVideoOnly:
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "Feature flag query");
        return true;
    default:
        return false;
    }
}

// IMediaControl
// --------------------------------

bool FAndroidVulkanMediaPlayer::CanControl(EMediaControl Control) const
{
    switch (Control)
    {
    case EMediaControl::Pause:
        return PlayState == EMediaState::Playing;
    case EMediaControl::Resume:
        return PlayState == EMediaState::Stopped;
    case EMediaControl::Seek:
        // we can sometimes seek to recover from an error,
        // so we allow seeking in error state even though it may or
        // may not work depending on the error
        return PlayState != EMediaState::Closed;
    }
    return false;
}

FTimespan FAndroidVulkanMediaPlayer::GetDuration() const
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't get duration");
        return FTimespan::MaxValue();
    }
    int64_t timeNS = Impl->getDurationNS();
    if (timeNS >= 0 && PlayState != EMediaState::Closed)
    {
        //        UE_LOG(LogDirectVideo, VeryVerbose, TEXT("Get Duration %ld"), timeNS);
        return FTimespan(timeNS / 100LL);
    }
    else
    {
        //       UE_LOGFMT(LogDirectVideo, VeryVerbose, "Get Duration empty");
        return FTimespan::MaxValue();
    }
}

float FAndroidVulkanMediaPlayer::GetRate() const
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't get rate");
        return 0.0;
    }
    if (PlayState == EMediaState::Playing)
    {
        return Impl->getRate();
    }
    else
    {
        return 0.0;
    }
}

EMediaState FAndroidVulkanMediaPlayer::GetState() const
{
    return PlayState;
}

EMediaStatus FAndroidVulkanMediaPlayer::GetStatus() const
{
    // not supported yet
    return EMediaStatus::None;
}

TRangeSet<float> FAndroidVulkanMediaPlayer::GetSupportedRates(EMediaRateThinning Thinning) const
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't get supported rates");
        return TRangeSet<float>();
    }
    TRangeSet<float> Retval;
    if (Impl->numAudioTracks() > 0 && !AudioTimestretchingEnabled)
    {
        Retval.Add(TRange<float>(0.0f));
        Retval.Add(TRange<float>(1.0f));
    }
    else
    {
        Retval.Add(TRange<float>(TRange<float>::BoundsType::Inclusive(0.0f),
                                 TRange<float>::BoundsType::Inclusive(10.0f)));
    }
    return Retval;
}

FTimespan FAndroidVulkanMediaPlayer::GetTime() const
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't get time");
        return FTimespan::Zero();
    }
    bool hasTime = Impl->hasTimeNS();
    if (!hasTime)
    {
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "Get Time has no time");
        return FTimespan::Zero();
    }
    int64_t timeNS = Impl->getTimeNS();
    UE_LOGFMT(LogDirectVideo, VeryVerbose, "Get Time {0}", int64(timeNS));
    return FTimespan(timeNS / 100LL);
}

bool FAndroidVulkanMediaPlayer::IsLooping() const
{
    return Looping;
}

bool FAndroidVulkanMediaPlayer::_SeekInternal(const FTimespan &Time, int32 InSeekTargetIndex)
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't seek");
        return false;
    }
    UE_LOGFMT(LogDirectVideo, Verbose, "Seek time:{0} index:{1}", Time.GetTicks(), SeekIndex);
    int64_t ticks = Time.GetTicks();
    int64_t nanoseconds = ticks * 100LL;
    // impl->seek guarantees that it will not send any pre-seek data
    // after this call returns
    Impl->seek(nanoseconds);
    // so we set seeking = true after the call
    Seeking = true;
    SeekTargetIndex = InSeekTargetIndex;
    return true;
}

bool FAndroidVulkanMediaPlayer::Seek(const FTimespan &Time)
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't seek");
        return false;
    }
    // in pre-5.6 we don't get passed a seek index, so we have to seek to currentindex+1
    // whereas 5.6 passes in one which we must use
    return _SeekInternal(Time, SeekIndex + 1);
}

bool FAndroidVulkanMediaPlayer::SetLooping(bool Loop)
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't set looping");
        return false;
    }
    Looping = Loop;
    Impl->setLooping(Looping);
    return true;
}
bool FAndroidVulkanMediaPlayer::SetRate(float Rate)
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't set rate");
        return false;
    }
    bool retval = false;
    int iInitialState = (int)PlayState;
    switch (PlayState)
    {
    case EMediaState::Playing:
        if (Rate == 0.0)
        {
            Impl->setPlaying(false);
            retval = true;
            PlayState = EMediaState::Stopped;
            EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
        }
        else
        {
            retval = Impl->setRate(Rate);
        }
        break;
    case EMediaState::Stopped:
        if (Rate == 0.0)
        {
            retval = true;
        }
        else if (Rate > 0.0)
        {
            Impl->setPlaying(true);
            PlayState = EMediaState::Playing;
            retval = Impl->setRate(Rate);
            if (retval)
            {
                EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackResumed);
            }
        }
        break;
    default:
        retval = false;
        break;
    };
    int iFinalState = (int)PlayState;
    UE_LOGFMT(LogDirectVideo, Verbose, "Set rate {0} {1} state:{2}->{3}", Rate, retval,
              iInitialState, iFinalState);
    return retval;
}

bool FAndroidVulkanMediaPlayer::SetNativeVolume(float Volume)
{
    if (AudioOut == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No audio out, can't set volume");
        return false;
    }
    return AudioOut->setVolume(Volume);
}

// IMediaTracks
// --------------------------------
bool FAndroidVulkanMediaPlayer::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex,
                                                    FMediaAudioTrackFormat &OutFormat) const
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't get audio track format");
        return false;
    }
    if (FormatIndex != 0 || PlayState == EMediaState::Closed)
    {
        return false;
    }
    int32 bitsPerSample;
    int32 channels;
    int32 rate;
    if (!Impl->getAudioTrackFormat(TrackIndex, &bitsPerSample, &channels, &rate))
    {
        return false;
    }
    OutFormat.BitsPerSample = bitsPerSample;
    OutFormat.NumChannels = channels;
    OutFormat.SampleRate = rate;
    OutFormat.TypeName = TEXT("Native");
    UE_LOGFMT(LogDirectVideo, VeryVerbose, "Get audio trackformat {0} {1} {2} {3}", TrackIndex,
              bitsPerSample, channels, rate);
    return true;
}
int32 FAndroidVulkanMediaPlayer::GetNumTracks(EMediaTrackType TrackType) const
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't get num tracks");
        return 0;
    }
    int rv = 0;
    switch (TrackType)
    {
    case EMediaTrackType::Audio:
        rv = Impl->numAudioTracks();
        break;
    case EMediaTrackType::Video:
        rv = Impl->numVideoTracks();
        break;
    }
    UE_LOGFMT(LogDirectVideo, VeryVerbose, "Get num tracks  type: {0} {1}", int(TrackType), rv);
    return rv;
}

int32 FAndroidVulkanMediaPlayer::GetNumTrackFormats(EMediaTrackType TrackType,
                                                    int32 TrackIndex) const
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't get num track formats");
        return 0;
    }
    if (TrackIndex >= GetNumTracks(TrackType))
    {
        return 0;
    }
    switch (TrackType)
    {
    case EMediaTrackType::Audio:
        return 1;
    case EMediaTrackType::Video:
        return 1;
    default:
        return 0;
    }
}

int32 FAndroidVulkanMediaPlayer::GetSelectedTrack(EMediaTrackType TrackType) const
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't get selected track");
        return INDEX_NONE;
    }
    int32 retval = INDEX_NONE;
    switch (TrackType)
    {
    case EMediaTrackType::Audio:
        retval = Impl->getSelectedAudioTrack();
        break;
    case EMediaTrackType::Video:
        retval = Impl->getSelectedVideoTrack();
        break;
    }
    UE_LOGFMT(LogDirectVideo, VeryVerbose, "Returning selected track type {0} {1}", int(TrackType),
              retval);
    return retval;
}

FText FAndroidVulkanMediaPlayer::GetTrackDisplayName(EMediaTrackType TrackType,
                                                     int32 TrackIndex) const
{
    // TODO: pass through display names
    return FText::Format(LOCTEXT("TrackName", "Track {0} type  {1}"), (int)TrackType, TrackIndex);
}

int32 FAndroidVulkanMediaPlayer::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
    return 0;
}

FString FAndroidVulkanMediaPlayer::GetTrackLanguage(EMediaTrackType TrackType,
                                                    int32 TrackIndex) const
{
    // TODO - pass language through
    return TEXT("");
}

FString FAndroidVulkanMediaPlayer::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
    if (TrackIndex == 0 && PlayState != EMediaState::Closed)
    {
        return TEXT("TRACK");
    }
    else
    {
        return TEXT("");
    }
}

bool FAndroidVulkanMediaPlayer::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex,
                                                    FMediaVideoTrackFormat &OutFormat) const
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't get video track format");
        return false;
    }
    if (FormatIndex != 0 || PlayState == EMediaState::Closed)
    {
        return false;
    }
    int w = 0, h = 0;
    float frameRate = 0;
    if (!Impl->getVideoTrackFormat(TrackIndex, &w, &h, &frameRate))
    {
        return false;
    }
    OutFormat.Dim.X = w;
    OutFormat.Dim.Y = h;
    OutFormat.FrameRate = frameRate;
    OutFormat.FrameRates = TRange<float>(frameRate);
    OutFormat.TypeName = TEXT("Vulkan Video Frame");
    UE_LOGFMT(LogDirectVideo, VeryVerbose, "Got out video format");
    return true;
}

bool FAndroidVulkanMediaPlayer::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't select track");
        return false;
    }
    UE_LOGFMT(LogDirectVideo, VeryVerbose, "Selecting track type {0} = {1}", int(TrackType),
              TrackIndex);
    if (TrackType == EMediaTrackType::Audio)
    {
        return Impl->selectAudioTrack(TrackIndex);
    }
    else if (TrackType == EMediaTrackType::Video)
    {
        return Impl->selectVideoTrack(TrackIndex);
    }
    return false;
}

bool FAndroidVulkanMediaPlayer::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex,
                                               int32 FormatIndex)
{
    // todo: support multiple formats / track
    return TrackIndex == 0 && FormatIndex == 0 && PlayState != EMediaState::Closed;
}

IMediaSamples &FAndroidVulkanMediaPlayer::GetSamples()
{
    return *SampleQueue.Get();
}

void FAndroidVulkanMediaPlayer::onVideoFrame(void *frameHwBuffer, int w, int h, int64_t presTimeNs)
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't process video frame");
        return;
    }
    if (PlayState == EMediaState::Closed)
    {
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "Releasing frame as we are closed");
        Impl->releaseFrame(frameHwBuffer);
        return;
    }
    if (HasVideoThisFrame)
    {
        Impl->releaseFrame(frameHwBuffer);
        return;
    }
    HasVideoThisFrame = true;
    auto textureSample = VideoSamplePool.AcquireShared();
    textureSample->Init(
        Impl, frameHwBuffer, w, h,
        UnrealAudioOut::MakeTimeStamp(FTimespan(presTimeNs / 100LL), SeekIndex, LoopIndex));
    SampleQueue->AddVideo(textureSample);
    UE_LOGFMT(LogDirectVideo, VeryVerbose, "Add video sample");
    //    UE_LOGFMT(LogDirectVideo, VeryVerbose, "Video samples {0}",
    //    SampleQueue->NumVideoSamples());
    // NB: this needs to happen after the sample is in the queue, so that
    // the queue has something in when media player facade samples it for playback
    // time
    UpdateSeekStatusOnData();
}

void FAndroidVulkanMediaPlayer::OnEnterBackground()
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't enter background");
        return;
    }
    // going into backgroud - if playing, pause implementation player
    int i = (int)PlayState;
    UE_LOGFMT(LogDirectVideo, Verbose, "Enter Suspend {0}", i);
    if (!Suspended)
    {
        Suspended = true;
        if (PlayState == EMediaState::Playing)
        {
            Impl->setPlaying(false);
            EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
        }
    }
}
void FAndroidVulkanMediaPlayer::OnEnterForeground()
{
    if (Impl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "No Impl, can't enter foreground");
        return;
    }
    int i = (int)PlayState;
    UE_LOGFMT(LogDirectVideo, Verbose, "Enter foreground {0}", i);
    // back into foreground - if playing, start
    if (Suspended)
    {
        Suspended = false;
        if (PlayState == EMediaState::Playing)
        {
            Impl->setPlaying(true);
            EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackResumed);
        }
    }
}

void *FAndroidVulkanMediaPlayer::getVkDeviceProcAddr(const char *name)
{
    void *result = static_cast<void *>(
        (static_cast<struct IVulkanDynamicRHI *>(GDynamicRHI))->RHIGetVkDeviceProcAddr(name));

    if (result == NULL)
    {
        result = static_cast<void *>(
            (static_cast<struct IVulkanDynamicRHI *>(GDynamicRHI))->RHIGetVkInstanceProcAddr(name));
    }

    return result;
}

VkDevice FAndroidVulkanMediaPlayer::getVkDevice()
{
    IVulkanDynamicRHI *rhi = static_cast<struct IVulkanDynamicRHI *>(GDynamicRHI);
    if (rhi == NULL)
    {
        return VK_NULL_HANDLE;
    }
    return rhi->RHIGetVkDevice();
}

const VkAllocationCallbacks *FAndroidVulkanMediaPlayer::getVkAllocationCallbacks()
{
    IVulkanDynamicRHI *rhi = static_cast<struct IVulkanDynamicRHI *>(GDynamicRHI);
    if (rhi == NULL)
    {
        return NULL;
    }
    return rhi->RHIGetVkAllocationCallbacks();
}

VkPhysicalDevice FAndroidVulkanMediaPlayer::getNativePhysicalDevice()
{
    return static_cast<VkPhysicalDevice>(GDynamicRHI->RHIGetNativePhysicalDevice());
}

void FAndroidVulkanMediaPlayer::onPlaybackEnd(bool looping)
{
    EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackEndReached);
    if (!looping)
    {
        PlayState = EMediaState::Stopped;
        EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
    }
}

bool FAndroidVulkanMediaPlayer::onVideoTooLarge()
{
    if (HasFallbackLeft)
    {
        VideoTooLargeReported = true;
        UE_LOGFMT(LogDirectVideo, Warning,
                  "Video frame too large for Vulkan device, looking for fallback");
        return false;
    }
    else
    {
        UE_LOGFMT(LogDirectVideo, Verbose,
                  "Video frame too large for Vulkan device but no fallback left, trying to play");
        return true;
    }
}

void FAndroidVulkanMediaPlayer::SetLastAudioRenderedSampleTime(FTimespan SampleTime)
{
    LastAudioSampleTime = SampleTime;
};

void FAndroidVulkanMediaPlayer::onVideoError(const char *errorText)
{
    UE_LOGFMT(LogDirectVideo, Error, "Video error: {0}", errorText);
    SetRate(0.0f); // stop playback on video error
                   //    PlayState = EMediaState::Error;
    EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
}
void FAndroidVulkanMediaPlayer::onAudioError(const char *errorText)
{
    UE_LOGFMT(LogDirectVideo, Error, "Audio error: {0}", errorText);
    SetRate(0.0f); // stop playback on audio error
                   //    PlayState = EMediaState::Error;
    EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
}

static FString _CleanFallbackCacheURL(const FString &InVal)
{
    FString OutVal;
    for (int i = 0; i < InVal.Len(); i++)
    {
        TCHAR c = InVal[i];

        if (FChar::IsAlnum(c))
        {
            OutVal += c;
        }
        else
        {
            OutVal += '_';
        }
    }
    return OutVal;
}

FString FAndroidVulkanMediaPlayer::GetFallbackCacheValue(const FString &InUrl)
{
    FString Url = _CleanFallbackCacheURL(InUrl);
    FindCacheDir();
    FString FallbackVid;
    if (FallbackCacheFile.GetString(TEXT("FallbackCache"), *Url, FallbackVid))
    {
        UE_LOG(LogDirectVideo, VeryVerbose, TEXT("Using cached fallback video url: %s"),
               *FallbackVid);
        return FallbackVid;
    }
    else
    {
        UE_LOG(LogDirectVideo, VeryVerbose, TEXT("No cached fallback video url for %s"), *Url);
        return "";
    }
}

void FAndroidVulkanMediaPlayer::UpdateFallbackCacheValue(const FString &InUrl,
                                                         const FString &Fallback)
{
    FString Url = _CleanFallbackCacheURL(InUrl);
    FindCacheDir();
    FString OldFallback = GetFallbackCacheValue(Url);
    if (OldFallback.Len() == 0 || (OldFallback != Fallback))
    {
        UE_LOG(LogDirectVideo, VeryVerbose, TEXT("Updating cached fallback video url: %s -> %s"),
               *Url, *Fallback);
        // update cache
        FallbackCacheFile.SetString(TEXT("FallbackCache"), *Url, *Fallback);
        FallbackCacheFile.Write(FallbackCachePath);
    }
}

void FAndroidVulkanMediaPlayer::FindCacheDir()
{
    // this is some disgusting JNI code to get app cache directory
    // because some versions of Unreal make it hard to find
    if (FallbackCachePath.Len() != 0)
    {
        // already found
        return;
    }

    // get the android app cache dir for storing the fallback cache
    jobject jactivity = AndroidJavaEnv::GetGameActivityThis();
    JNIEnv *env = AndroidJavaEnv::GetJavaEnv();
    jclass contextClass = env->FindClass("android/content/Context");
    jmethodID getCacheDirMethod = env->GetMethodID(contextClass, "getCacheDir", "()Ljava/io/File;");
    jobject cacheDir = env->CallObjectMethod(jactivity, getCacheDirMethod);
    jclass fileClass = env->FindClass("java/io/File");
    jmethodID getAbsolutePathMethod =
        env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");
    jstring cacheDirPath = (jstring)env->CallObjectMethod(cacheDir, getAbsolutePathMethod);
    const char *cacheDirPathCStr = env->GetStringUTFChars(cacheDirPath, nullptr);
    FString CacheDirString = FString(UTF8_TO_TCHAR(cacheDirPathCStr));
    FallbackCachePath = FPaths::Combine(CacheDirString, TEXT("fallback_cache.ini"));
    env->ReleaseStringUTFChars(cacheDirPath, cacheDirPathCStr);
    env->DeleteLocalRef(cacheDirPath);
    env->DeleteLocalRef(cacheDir);
    env->DeleteLocalRef(fileClass);
    env->DeleteLocalRef(contextClass);

    FallbackCacheFile.Read(FallbackCachePath);
    UE_LOG(LogDirectVideo, VeryVerbose, TEXT("Fallback cache file: %s"), *FallbackCachePath);
}

#undef LOCTEXT_NAMESPACE
