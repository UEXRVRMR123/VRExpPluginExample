// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// Callback used by implementation class to send
// buffers to media player.
// ------------------------------------------------
#pragma once
class IAndroidVulkanVideoAVCallback
{
  public:
    virtual ~IAndroidVulkanVideoAVCallback()
    {
    }
    // called with video frame hardware buffers
    virtual void onVideoFrame(void *frameHwBuffer, int w, int h, int64_t presTimeNs) = 0;
    // we need to get vulkan functions and device etc. via Unreal Engine for consistency
    virtual void *getVkDeviceProcAddr(const char *name) = 0;
    virtual VkDevice getVkDevice() = 0;
    virtual const VkAllocationCallbacks *getVkAllocationCallbacks() = 0;
    virtual VkPhysicalDevice getNativePhysicalDevice() = 0;

    virtual void onVideoError(const char *errorText) = 0;
    virtual void onAudioError(const char *errorText) = 0;

    virtual void onPlaybackEnd(bool looping) = 0;

    // if the video is too large we call this callback
    // if the callback returns false then we will fail out so
    // the player can do a fallback to lower resolution
    // otherwise we will just to a best effort attempt with
    // whatever codec is available, which works on some devices
    // e.g. quest will actually play slightly larger / higher fps videos
    // than it reportts
    virtual bool onVideoTooLarge() = 0;
};
