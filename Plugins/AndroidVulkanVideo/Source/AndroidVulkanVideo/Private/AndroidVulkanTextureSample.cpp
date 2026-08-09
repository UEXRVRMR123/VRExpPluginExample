// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// Vulkan texture sample implementation.
//
// Actual texture handling is handled in
// FRenderOnRHIThreadCommand::Execute via calls to
// IVulkanImpl
// ------------------------------------------------

#include "AndroidVulkanTextureSample.h"

#include "UnrealLogging.h"

#include "Android/AndroidApplication.h"
#include "AndroidVulkanMediaPlayer.h"
#include "IVulkanDynamicRHI.h"
#include "IVulkanImpl.h"
#include "SceneUtils.h"

#include "RenderingThread.h"

void AndroidVulkanTextureSample::FRenderOnRHIThreadCommand(FRHICommandList &CurCommandList,
                                                           IVulkanImpl *CopyImpl, void *HWImageCopy,
                                                           const FTextureRHIRef &DstTexture)
{
    //  FScopeLock Lock(&AccessLock);
    if (CopyImpl == NULL || DstTexture.IsValid() == false)
    {
        return;
    }
    IVulkanDynamicRHI *RHI = static_cast<struct IVulkanDynamicRHI *>(GDynamicRHI);
    FVulkanRHIImageViewInfo TexInfo = RHI->RHIGetImageViewInfo(DstTexture.GetReference());
    VkCommandBuffer cmdBuffer = RHI->RHIGetActiveVkCommandBuffer();
    // 2) impl class
    // 3) VkImageView etc. from InDstTexture
    int w = TexInfo.Width;
    int h = TexInfo.Height;
    int Samples = DstTexture->GetDesc().NumSamples;
    CopyImpl->renderFrameToImageView(cmdBuffer, HWImageCopy, TexInfo.ImageView, TexInfo.Image,
                                     TexInfo.Format, w, h, Samples);
}

void AndroidVulkanTextureSample::FRenderMeshOnRHIThreadCommand(FRHICommandList &CurCommandList,
                                                               IVulkanImpl *CopyImpl,
                                                               void *HWImageCopy,
                                                               const FTextureRHIRef &DstTexture,
                                                               void *MeshID, int Samples)
{
    //    FScopeLock Lock(&AccessLock);
    //    UE_LOG(LogDirectVideo, VeryVerbose, TEXT("Writing texture mesh  %p (RHI thread)"),
    //    HWImageCopy);

    if (CopyImpl == NULL)
    {
        UE_LOGFMT(LogDirectVideo, Error, "Impl is invalid");
        return;
    }
    if (DstTexture.IsValid() == false)
    {
        UE_LOGFMT(LogDirectVideo, Error, "DstTexture is invalid");
        return;
    }
    if (CopyImpl->isReadyToRender(HWImageCopy) == false)
    {
        UE_LOG(LogDirectVideo, Error,
               TEXT("Impl is not ready to render - shouldn't be calling draw %p"), HWImageCopy);
        return;
    }
    IVulkanDynamicRHI *RHI = static_cast<struct IVulkanDynamicRHI *>(GDynamicRHI);
    FVulkanRHIImageViewInfo TexInfo = RHI->RHIGetImageViewInfo(DstTexture.GetReference());
    VkCommandBuffer cmdBuffer = RHI->RHIGetActiveVkCommandBuffer();
    int w = TexInfo.Width;
    int h = TexInfo.Height;
    int DrawViews = DstTexture->GetDesc().ArraySize;
    // UE_LOG(LogDirectVideo, VeryVerbose,
    //        TEXT("Render in existing pass: Samples: %d views: %d image:%p"), Samples, DrawViews,
    //        HWImageCopy);
    // // render to upside down viewport for consistency with unreal rendering
    CopyImpl->renderFrameToMeshInExistingPass(cmdBuffer, HWImageCopy, TexInfo.Image, 0, h, w, -h,
                                              Samples, DrawViews, MeshID);
}

void AndroidVulkanTextureSample::FUpdateMeshOnRHIThreadCommand(
    FRHICommandList &CurCommandList, IVulkanImpl *CopyImpl, const FTextureRHIRef &DstTexture,
    const TArray<VertexData> &PositionAndUV, const TArray<uint32> &Indices,
    const ShaderModelMatrix &Matrix, void *MeshID)
{
    //    FScopeLock Lock(&AccessLock);
    if (CopyImpl == NULL || DstTexture.IsValid() == false)
    {
        return;
    }

    IVulkanDynamicRHI *RHI = static_cast<struct IVulkanDynamicRHI *>(GDynamicRHI);
    VkCommandBuffer cmdBuffer = RHI->RHIGetActiveVkCommandBuffer();
    FVulkanRHIImageViewInfo TexInfo = RHI->RHIGetImageViewInfo(DstTexture.GetReference());
    CopyImpl->loadMesh(cmdBuffer, PositionAndUV.GetData(), PositionAndUV.Num(), Indices.GetData(),
                       Indices.Num(), MeshID);
    CopyImpl->updateModelMatrix(cmdBuffer, Matrix, TexInfo.Image, MeshID);
}

void AndroidVulkanTextureSample::FUpdateMatricesOnRHIThreadCommand(
    FRHICommandList &CurCommandList, IVulkanImpl *CopyImpl, const FTextureRHIRef &DstTexture,
    const ShaderViewMatrices &Matrices, const ShaderModelMatrix &ModelMatrix, void *MeshID)
{
    //    FScopeLock Lock(&AccessLock);
    if (CopyImpl == NULL || DstTexture.IsValid() == false)
    {
        return;
    }
    IVulkanDynamicRHI *RHI = static_cast<struct IVulkanDynamicRHI *>(GDynamicRHI);
    FVulkanRHIImageViewInfo TexInfo = RHI->RHIGetImageViewInfo(DstTexture.GetReference());
    VkCommandBuffer cmdBuffer = RHI->RHIGetActiveVkCommandBuffer();

    CopyImpl->updateViewMatrices(cmdBuffer, Matrices, TexInfo.Image);
    CopyImpl->updateModelMatrix(cmdBuffer, ModelMatrix, TexInfo.Image, MeshID);
}

void AndroidVulkanTextureSample::FInitMeshRenderingCommand(FRHICommandList &CurCommandList,
                                                           IVulkanImpl *CopyImpl, void *HWImageCopy,
                                                           const FTextureRHIRef &DstTexture,
                                                           void *MeshID, int Samples,
                                                           bool OutputSRGB, bool MultiViewEnabled)
{
    //    FScopeLock Lock(&AccessLock);
    if (CopyImpl == NULL || DstTexture.IsValid() == false)
    {
        return;
    }
    IVulkanDynamicRHI *RHI = static_cast<struct IVulkanDynamicRHI *>(GDynamicRHI);
    FVulkanRHIImageViewInfo TexInfo = RHI->RHIGetImageViewInfo(DstTexture.GetReference());
    VkCommandBuffer cmdBuffer = RHI->RHIGetActiveVkCommandBuffer();
    int w = TexInfo.Width;
    int h = TexInfo.Height;
    int DrawViews = DstTexture->GetDesc().ArraySize;
    CopyImpl->initializeFrameObjectsOutsidePass(cmdBuffer, HWImageCopy, TexInfo.Image, w, h, MeshID,
                                                Samples, DrawViews, OutputSRGB, MultiViewEnabled);
}

AndroidVulkanTextureSample::AndroidVulkanTextureSample()
{
    Impl = NULL;
    FrameHWImage = NULL;
    NumSamples = 1;

    MeshInitFence = RHICreateGPUFence(TEXT("VVRMeshInitFence"));
    ShowFence = RHICreateGPUFence(TEXT("VVRFrameFence"));
    for (int c = 0; c < MeshShowFences.size(); c++)
    {
        MeshShowFences[c] = RHICreateGPUFence(TEXT("VVRMeshFence"));
        UsedMeshShowFences[c] = false;
    }

    static const auto NumSamplesVar =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.MSAACount"));
    static const auto AAType =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.AntiAliasing"));

    if (AAType != NULL && NumSamplesVar != NULL)
    {
        EAntiAliasingMethod MobileAntiAliasing = EAntiAliasingMethod(AAType->GetInt());
        if (MobileAntiAliasing == EAntiAliasingMethod::AAM_MSAA)
        {
            NumSamples = NumSamplesVar->GetInt();
        }
    }
}

void AndroidVulkanTextureSample::InitNoVideo()
{
    FScopeLock Lock(&AccessLock);
    Clear();
    this->Dimension = FIntPoint(2, 2);
    this->FrameHWImage = NULL;
    this->SampleTime = FMediaTimeStamp(FTimespan::Zero());
    IsEmptyTexture = true;
}

void AndroidVulkanTextureSample::Init(IVulkanImpl *impl, void *hwImage, int w, int h,
                                      FMediaTimeStamp sampleTime)
{
    FScopeLock Lock(&AccessLock);
    Clear();
    this->Impl = impl;
    this->Dimension = FIntPoint(w, h);
    this->FrameHWImage = hwImage;
    this->SampleTime = sampleTime;
    UE_LOG(LogDirectVideo, VeryVerbose, TEXT("Create texture sample %d %d %p"), Dimension.X,
           Dimension.Y, hwImage);
    IsEmptyTexture = false;
}

AndroidVulkanTextureSample::~AndroidVulkanTextureSample()
{
}

FIntPoint AndroidVulkanTextureSample::GetDim() const
{
    return Dimension;
}

FIntPoint AndroidVulkanTextureSample::GetOutputDim() const
{
    return Dimension;
}
uint32 AndroidVulkanTextureSample::GetStride() const
{
    return Dimension.X;
}
FRHITexture *AndroidVulkanTextureSample::GetTexture() const
{
    return NULL;
}

IMediaTextureSampleConverter *AndroidVulkanTextureSample::GetMediaTextureSampleConverter()
{
    return this;
}

bool AndroidVulkanTextureSample::Convert(FRHICommandListImmediate &RHICmdList,
                                         FTextureRHIRef &InDstTexture,
                                         const FConversionHints &Hints)
{
    return ConvertInternal(RHICmdList, InDstTexture, Hints);
}

bool AndroidVulkanTextureSample::ConvertInternal(FRHICommandListImmediate &RHICmdList,
                                                 FTextureRHIRef &InDstTexture,
                                                 const FConversionHints &Hints)
{
    UE_LOGFMT(LogDirectVideo, VeryVerbose, "ConvertInternal");
    FScopeLock Lock(&AccessLock);
    if (IsEmptyTexture)
    {
        // just let texture clear itself to clear colour
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "Getting clear texture");
        return true;
    }
    else
    {
        if (Impl == NULL)
        {
            return false;
        }
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "doing texture conversion");
        State = RenderState::COPYING_TEXTURE;
        ShowFence->Clear();
        FTextureRHIRef CopyInDstTexture = InDstTexture;
        IVulkanImpl *CopyImpl = Impl;
        void *HWImageCopy = FrameHWImage;
        RHICmdList.EnqueueLambda(TEXT("ConvertTexture"),
                                 ([=, this](FRHICommandList &RHICommandList) {
                                     AndroidVulkanTextureSample::FRenderOnRHIThreadCommand(
                                         RHICommandList, CopyImpl, HWImageCopy, CopyInDstTexture);
                                 }));

        RHICmdList.WriteGPUFence(ShowFence);
        return true;
    }
}
bool AndroidVulkanTextureSample::InitFrameForMeshRendering(FTextureRHIRef InDstTexture,
                                                           FRHICommandList &RHICmdList,
                                                           void *MeshID, int ImplStereoMode,
                                                           bool OutputSRGB, bool MultiViewEnabled)
{
    FScopeLock Lock(&AccessLock);
    if (Impl == NULL)
    {
        return false;
    }
    if (IsEmptyTexture)
    {
        // TODO: no frame, do nothing?
        return true;
    }
    else
    {
        UE_LOG(LogDirectVideo, VeryVerbose, TEXT("init frame for rendering %p"), FrameHWImage);
        if (Impl->isReadyToRender(FrameHWImage) == true)
        {
            // we can render the frame without waiting for init
            State = RenderState::READY;
            return true;
        }

        MeshInitFence->Clear();
        State = RenderState::INIT;
        Impl->setStereoMode((IVulkanImpl::StereoMode)ImplStereoMode);
        IVulkanImpl *CopyImpl = Impl;
        void *FrameHWImageCopy = FrameHWImage;

        RHICmdList.EnqueueLambda(TEXT("InitFrame"), ([=, this](FRHICommandList &RHICommandList) {
                                     AndroidVulkanTextureSample::FInitMeshRenderingCommand(
                                         RHICommandList, CopyImpl, FrameHWImageCopy, InDstTexture,
                                         MeshID, NumSamples, OutputSRGB, MultiViewEnabled);
                                 }));

        RHICmdList.WriteGPUFence(MeshInitFence);
        return true;
    }
}

bool AndroidVulkanTextureSample::UpdateMesh(FTextureRHIRef InDstTexture,
                                            FRHICommandList &RHICmdList,
                                            TArray<VertexData> PositionAndUV,
                                            TArray<uint32> Indices, ShaderModelMatrix Matrix,
                                            void *MeshID)
{
    if (Impl != NULL && !Impl->hasMesh(MeshID))
    {
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "setting texture mesh");
        IVulkanImpl *CopyImpl = Impl;
        RHICmdList.EnqueueLambda(TEXT("SetMesh"), ([=, this](FRHICommandList &RHICommandList) {
                                     AndroidVulkanTextureSample::FUpdateMeshOnRHIThreadCommand(
                                         RHICommandList, CopyImpl, InDstTexture, PositionAndUV,
                                         Indices, Matrix, MeshID);
                                 }));

        return true;
    }
    else
    {
        return false;
    }
}

bool AndroidVulkanTextureSample::UpdateViewMatrices(FTextureRHIRef InDstTexture,
                                                    FRHICommandList &RHICmdList,
                                                    ShaderViewMatrices Matrices,
                                                    ShaderModelMatrix ModelMatrix, void *MeshID)
{
    // in impl, we need to
    // a) load data to mesh uniform buffers if not already loaded
    // then b) render to that mesh
    FScopeLock Lock(&AccessLock);
    if (Impl == NULL)
    {
        return false;
    }
    UE_LOGFMT(LogDirectVideo, VeryVerbose, "updating matrices");
    IVulkanImpl *CopyImpl = Impl;
    RHICmdList.EnqueueLambda(TEXT("UpdateMatrices"), ([=, this](FRHICommandList &RHICommandList) {
                                 AndroidVulkanTextureSample::FUpdateMatricesOnRHIThreadCommand(
                                     RHICommandList, CopyImpl, InDstTexture, Matrices, ModelMatrix,
                                     MeshID);
                             }));

    return true;
}

bool AndroidVulkanTextureSample::RenderToMesh(FTextureRHIRef InDstTexture,
                                              FRHICommandList &RHICmdList, void *MeshID)
{
    // in impl, we need to
    // a) load data to mesh uniform buffers if not already loaded
    // then b) render to that mesh
    FScopeLock Lock(&AccessLock);
    if (IsEmptyTexture)
    {
        // need to render to a clear colour...
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "Getting clear texture");
        return true;
    }
    else
    {
        if (Impl == NULL)
        {
            return false;
        }
        int ChosenFence = -1;
        auto &FenceToUse = MeshShowFences[0];

        if (State == RenderState::READY)
        {
            State = RenderState::SHOWN_MESH;
        }
        for (int c = 0; c < MeshShowFences.size(); c++)
        {
            if (UsedMeshShowFences[c] && MeshShowFences[c]->Poll())
            {
                MeshShowFences[c]->Clear();
                UsedMeshShowFences[c] = false;
            }
        }
        for (int c = 0; c < MeshShowFences.size(); c++)
        {
            if (!UsedMeshShowFences[c])
            {
                FenceToUse = MeshShowFences[c];
                UsedMeshShowFences[c] = true;
                ChosenFence = c;
                break;
            }
        }
        if (ChosenFence == -1)
        {
            UE_LOGFMT(LogDirectVideo, Error, "No free mesh fence found");
            return false;
        }
        else
        {
            UE_LOG(LogDirectVideo, VeryVerbose, TEXT("Using mesh fence %d"), ChosenFence);
        }

        FenceToUse->Clear();
        UE_LOG(LogDirectVideo, VeryVerbose, TEXT("Writing texture mesh  %p"), this->FrameHWImage);

        IVulkanImpl *CopyImpl = Impl;
        void *HWImageCopy = FrameHWImage;
        RHICmdList.EnqueueLambda(TEXT("RenderMesh"), ([=, this](FRHICommandList &RHICommandList) {
                                     AndroidVulkanTextureSample::FRenderMeshOnRHIThreadCommand(
                                         RHICommandList, CopyImpl, HWImageCopy, InDstTexture,
                                         MeshID, NumSamples);
                                 }));

        RHICmdList.WriteGPUFence(FenceToUse);
        return true;
    }
}

FMediaTimeStamp AndroidVulkanTextureSample::GetTime() const
{
    return SampleTime;
}

void AndroidVulkanTextureSample::InitializePoolable()
{
    Clear();
}

void AndroidVulkanTextureSample::ClearIfShown()
{
    // if mesh rendering, we don't clear texture sample
    // as it will be rendered next frame
    if (State == RenderState::COPYING_TEXTURE && IsReadyForReuse())
    {
        UE_LOG(LogDirectVideo, VeryVerbose, TEXT("Clearing texture sample as shown: %p"),
               this->FrameHWImage);
        Clear();
    }
}

bool AndroidVulkanTextureSample::IsReadyForReuse()
{
    FScopeLock Lock(&AccessLock);
    if (this->FrameHWImage != NULL)
    {
        switch (State)
        {
        case RenderState::INIT:
            if (!MeshInitFence->Poll())
            {
                UE_LOGFMT(LogDirectVideo, VeryVerbose,
                          "Reuse: Waiting for mesh init command to finish.");
                return false;
            }
            break;
        case RenderState::SHOWN_MESH:
            for (int c = 0; c < MeshShowFences.size(); c++)
            {
                if (UsedMeshShowFences[c] && !MeshShowFences[c]->Poll())
                {
                    UE_LOG(LogDirectVideo, VeryVerbose,
                           TEXT("Reuse: Waiting for mesh show command %d to finish."), c);
                    return false;
                }
            }
            break;
        case RenderState::COPYING_TEXTURE:
            if (!ShowFence->Poll())
            {
                UE_LOGFMT(LogDirectVideo, VeryVerbose,
                          "Reuse: Waiting for texture copy command to finish.");
                return false;
            }
            break;
        }
    }
    UE_LOG(LogDirectVideo, VeryVerbose, TEXT("Ready for reuse %p %d %d "), FrameHWImage, State,
           ShowFence->Poll());
    return true;
}

void AndroidVulkanTextureSample::Clear()
{
    FScopeLock Lock(&AccessLock);
    if (FrameHWImage != NULL)
    {
        UE_LOG(LogDirectVideo, VeryVerbose, TEXT("Clearing texture sample %p"), FrameHWImage);
    }
    if (Impl != NULL && FrameHWImage != NULL)
    {
        UE_LOG(LogDirectVideo, VeryVerbose, TEXT("Release texture sample  %p"), FrameHWImage);
        Impl->releaseFrame(FrameHWImage);
    }
    FrameHWImage = NULL;
    Impl = NULL;
    ShowFence->Clear();
    MeshInitFence->Clear();
    State = RenderState::NONE;
    UsedFence0 = false;
    UsedFence1 = false;
}

void AndroidVulkanTextureSample::ShutdownPoolable()
{
    bool needsWait = false;
    {
        FScopeLock Lock(&AccessLock);
        switch (State)
        {
        case RenderState::INIT:
            if (!MeshInitFence->Poll())
            {
                needsWait = true;
            }
            break;
        case RenderState::SHOWN_MESH:
            for (int c = 0; c < MeshShowFences.size(); c++)
            {
                if (UsedMeshShowFences[c] && !MeshShowFences[c]->Poll())
                {
                    needsWait = true;
                    break;
                }
            }
            break;
        case RenderState::COPYING_TEXTURE:
            if (!ShowFence->Poll())
            {
                needsWait = true;
            }
            break;
        }
    }
    if (needsWait)
    {
        if (IsInGameThread())
        {
            UE_LOG(LogDirectVideo, VeryVerbose,
                   TEXT("Shutdown poolable - waiting for render finish %x"), FrameHWImage);
            FlushRenderingCommands();
        }
    }
    Clear();
}

void AndroidVulkanTextureSample::CaptureRenderPass(IMediaPlayer *Player,
                                                   FRHICommandList &RHICmdList)
{
    IVulkanImpl *Impl = static_cast<FAndroidVulkanMediaPlayer *>(Player)->GetImpl();
    RHICmdList.EnqueueLambda(
        TEXT("CaptureMainRenderPassID"), [Impl](FRHICommandList &CurCommandList) {
            IVulkanDynamicRHI *RHI = static_cast<struct IVulkanDynamicRHI *>(GDynamicRHI);
            VkCommandBuffer cmdBuffer = RHI->RHIGetActiveVkCommandBuffer();
            Impl->captureMainRenderPassID(cmdBuffer);
        });
}

void AndroidVulkanTextureSample::ImplDeleted()
{
    FScopeLock Lock(&AccessLock);
    this->Impl = NULL;
}

bool AndroidVulkanTextureSample::SetMeshCullMode(ERasterizerCullMode cullMode)
{
    VkCullModeFlagBits cullModeVK = VK_CULL_MODE_NONE;
    switch (cullMode)
    {
    case ERasterizerCullMode::CM_None:
        cullModeVK = VK_CULL_MODE_NONE;
        break;
    case ERasterizerCullMode::CM_CCW:
        cullModeVK = VK_CULL_MODE_BACK_BIT;
        break;
    case ERasterizerCullMode::CM_CW:
        cullModeVK = VK_CULL_MODE_FRONT_BIT;
        break;
    }
    if (Impl != NULL)
    {
        return Impl->setMeshCullmode(cullModeVK);
    }
    return false;
}

void AndroidVulkanTextureSample::MuteVideo(IMediaPlayer *Player)
{
    IVulkanImpl *Impl = static_cast<FAndroidVulkanMediaPlayer *>(Player)->GetImpl();
    if (Impl != NULL)
    {
        Impl->setVideoMuteMode(IVulkanImpl::VideoMuteMode::MUTE_DROP);
    }
}
void AndroidVulkanTextureSample::MuteAndPauseVideo(IMediaPlayer *Player)
{
    IVulkanImpl *Impl = static_cast<FAndroidVulkanMediaPlayer *>(Player)->GetImpl();
    if (Impl != NULL)
    {
        Impl->setVideoMuteMode(IVulkanImpl::VideoMuteMode::MUTE_PAUSE);
    }
}
void AndroidVulkanTextureSample::UnmuteVideo(IMediaPlayer *Player)
{
    IVulkanImpl *Impl = static_cast<FAndroidVulkanMediaPlayer *>(Player)->GetImpl();
    if (Impl != NULL)
    {
        Impl->setVideoMuteMode(IVulkanImpl::VideoMuteMode::MUTE_NONE);
    }
}

EMediaTextureSampleFormat AndroidVulkanTextureSample::VideoTextureFormat =
    EMediaTextureSampleFormat::CharBGR10A2;
