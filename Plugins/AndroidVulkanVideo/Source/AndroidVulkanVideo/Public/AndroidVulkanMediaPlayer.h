// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// The main IMediaPlayer implementation. This is a
// shim which provides Unreal interfaces, and then
// defers everything vulkan related to the main
// AndroidVulkanVideoImpl class.
// ------------------------------------------------
#pragma once

#include "IMediaCache.h"
#include "IMediaControls.h"
#include "IMediaEventSink.h"
#include "IMediaPlayer.h"
#include "IMediaTracks.h"
#include "IMediaView.h"
#include "MediaSamples.h"

#include "Misc/ConfigCacheIni.h" // for fallback cache

// for vulkan types in the callback fn
#include "IVulkanDynamicRHI.h"

#include "IAndroidVulkanVideoAVCallback.h"

#include "AndroidVulkanTextureSample.h"
#include "VideoMediaSampleHolder.h"

#include "UnrealLogging.h"

class IAudioOut;
class IVulkanImpl;

class FAndroidVulkanMediaPlayer : public IMediaPlayer,
                                  protected IMediaCache,
                                  protected IMediaControls,
                                  protected IMediaTracks,
                                  protected IMediaView,
                                  protected IAndroidVulkanVideoAVCallback
{
  public:
    FAndroidVulkanMediaPlayer(IMediaEventSink &InEventSink);
    virtual ~FAndroidVulkanMediaPlayer();

    // IMediaPlayer
    virtual void Close() override;
    virtual IMediaCache &GetCache() override
    {
        return *this;
    }
    virtual IMediaControls &GetControls() override
    {
        return *this;
    }

    virtual bool FlushOnSeekCompleted() const
    {
        return false;
    }
    virtual FString GetInfo() const override;
    virtual FGuid GetPlayerPluginGUID() const override;
    virtual IMediaSamples &GetSamples() override;
    virtual FString GetStats() const override;
    virtual IMediaTracks &GetTracks() override
    {
        return *this;
    }
    virtual FString GetUrl() const override;
    virtual IMediaView &GetView() override
    {
        return *this;
    }
    virtual bool Open(const FString &Url, const IMediaOptions *Options) override;
    virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe> &Archive,
                      const FString &OriginalUrl, const IMediaOptions *Options) override;
    virtual void SetGuid(const FGuid &Guid) override;
    virtual void TickFetch(FTimespan DeltaTime, FTimespan Timecode) override;
    virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override;
    virtual bool GetPlayerFeatureFlag(EFeatureFlag flag) const override;
    virtual void SetLastAudioRenderedSampleTime(FTimespan SampleTime) override;

    FTimespan GetLastAudioRenderedSampleTime() const
    {
        return LastAudioSampleTime;
    }

    virtual TSharedPtr<FVideoMediaSampleHolder, ESPMode::ThreadSafe> GetSampleQueue()
    {
        return SampleQueue;
    }

    // IMediaCache
    // don't override these - we don't provide caching (yet?)
    // virtual bool QueryCacheState(EMediaCacheState State, TRangeSet<FTimespan>& OutTimeRanges)
    // const virtual int32 GetSampleCount(EMediaCacheState State) const

    // IMediaControl
    virtual bool CanControl(EMediaControl Control) const override;
    virtual FTimespan GetDuration() const override;
    virtual float GetRate() const override;
    virtual EMediaState GetState() const override;
    virtual EMediaStatus GetStatus() const override;
    virtual TRangeSet<float> GetSupportedRates(EMediaRateThinning Thinning) const override;
    virtual FTimespan GetTime() const override;
    virtual bool IsLooping() const override;
    virtual bool Seek(const FTimespan &Time) override;

    virtual bool SetLooping(bool Looping) override;
    virtual bool SetRate(float Rate) override;
    virtual bool SetNativeVolume(float Volume) override;

    // IMediaTracks
    virtual bool GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex,
                                     FMediaAudioTrackFormat &OutFormat) const override;
    virtual int32 GetNumTracks(EMediaTrackType TrackType) const override;
    virtual int32 GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const override;
    virtual int32 GetSelectedTrack(EMediaTrackType TrackType) const override;
    virtual FText GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const override;
    virtual int32 GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const override;
    virtual FString GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const override;
    virtual FString GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const override;
    virtual bool GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex,
                                     FMediaVideoTrackFormat &OutFormat) const override;
    virtual bool SelectTrack(EMediaTrackType TrackType, int32 TrackIndex) override;
    virtual bool SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex,
                                int32 FormatIndex) override;

    // IMediaView
    // don't provide these features, so we don't override any of it

    // IAndroidVulkanVideoAVCallback (Impl callback)
    virtual void onVideoFrame(void *frameHwBuffer, int w, int h, int64_t presTimeNs) override;
    virtual void *getVkDeviceProcAddr(const char *name) override;
    virtual VkDevice getVkDevice() override;
    virtual const VkAllocationCallbacks *getVkAllocationCallbacks() override;
    virtual VkPhysicalDevice getNativePhysicalDevice() override;
    virtual void onPlaybackEnd(bool looping) override;

    virtual void onVideoError(const char *errorText) override;
    virtual void onAudioError(const char *errorText) override;
    virtual bool onVideoTooLarge() override;

    virtual void ProcessVideoSamples() override {};

    int32 GetSeekIndex()
    {
        return SeekIndex;
    }

    void IncrementLoopIndex()
    {
        LoopIndex = LoopIndex + 1;
    }

    int32 GetLoopIndex()
    {
        return LoopIndex;
    }

    void UpdateSeekStatusIfNoVideoAndPaused()
    {
        // this is a special case - if we are paused and don't have
        // a video track showing, then we won't send seek completed
        // on receiving the post-seek video frame, so we update seek
        // status as soon as audio has been seeked
        if (GetSelectedTrack(EMediaTrackType::Video) == -1 && PlayState == EMediaState::Paused)
        {
            UE_LOGFMT(LogDirectVideo, Verbose,
                      "Updating seek status on audio seek as no video track and paused");
            UpdateSeekStatusOnData();
        }
    }

    void UpdateSeekStatusOnData()
    {
        if (Seeking)
        {
            SeekIndex = SeekTargetIndex;
            LoopIndex = 0;
            UE_LOGFMT(LogDirectVideo, Verbose, "Seeking-completed {0}", SeekIndex);
            Seeking = false;
            EventSink.ReceiveMediaEvent(EMediaEvent::SeekCompleted);
        }
    }

    IVulkanImpl *GetImpl()
    {
        return Impl;
    }

  private:
    // called for each open attempt (in case of auto-fallback to lower resolution)
    virtual bool _OpenInternal(const FString &Url);

    bool _SeekInternal(const FTimespan &Time, int32_t InSeekTargetIndex);

    // delegates for foreground -> background transitions which should pause
    // and resume playback
    void OnEnterBackground();
    void OnEnterForeground();

    FDelegateHandle DelegateEnterBackground;
    FDelegateHandle DelegateEnterForeground;
    FDelegateHandle DelegateHeadsetRemoved;
    FDelegateHandle DelegateHeadsetPutOn;

    bool Seeking;
    int32 SeekIndex;
    int32 LoopIndex;
    int SeekTargetIndex;

    // our output samples
    TSharedPtr<FVideoMediaSampleHolder, ESPMode::ThreadSafe> SampleQueue;
    IMediaEventSink &EventSink;
    IVulkanImpl *Impl;
    IAudioOut *AudioOut;
    FString CurInfo;
    FString VideoURL;
    FGuid PlayerGUID;
    EMediaState PlayState;
    bool Looping;
    FAndroidVulkanTextureSamplePool VideoSamplePool;
    bool HasVideoThisFrame;
    bool SentBlankFrame;

    bool Suspended;

    FTimespan LastAudioSampleTime;
    // logging object passed to Impl
    UnrealLogger Logger;

  private:
    void *ImplDLL;
    typedef IVulkanImpl *(*CreateImplType)();
    typedef void (*DestroyImplType)(IVulkanImpl *);
    CreateImplType CreateImpl;
    DestroyImplType DestroyImpl;

    bool HasError;
    FText ErrorMessage;

    /********************************* Video Fallback feature *************************
     * This feature allows fallback to lower resolution videos if the device doesn't
     * support the full resolution. e.g. some Honor devices have a max resolution of
     * 4096x4096 video decoding.
     *
     * FallbackVideoPattern is set by default to "_fallback_{N}". If the video
     * is too high resolution to be played, we will try playing a fallback video with
     * this suffix, with N starting at 1 and increasing until we find one that works
     * or we run out of videos to try.
     *
     * If we successfully find a fallback video, the value is stored in android application
     * cache folder in an ini file with the name .directvideo_fallback_cache.ini
     *
     ***********************************************************************************/
    FString FallbackVideoPattern;
    bool VideoTooLargeReported;
    bool HasFallbackLeft;

    bool AudioTimestretchingEnabled;
    // we persist video fallback values so that
    // we can load the fallback video first time next
    // time it is loaded
    FString FallbackCachePath;
    FConfigFile FallbackCacheFile;

    FString GetFallbackCacheValue(const FString &Url);
    void UpdateFallbackCacheValue(const FString &Url, const FString &Fallback);
    void FindCacheDir();
};
