#include "UI/VRExpDetectableUIInfoTriggerLogic.h"

#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Curves/CurveFloat.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GripMotionControllerComponent.h"
#include "ReplicatedVRCameraComponent.h"
#include "UObject/UnrealType.h"
#include "Detection/VRExpDetectableComponent.h"
#include "Detection/VRExpHeadDetectionComponent.h"
#include "Interaction/VRExpGrabbableMotionComponent.h"
#include "UI/VRExpUIInfoActorInterface.h"
#include "UI/VRExpUIInfoPlacementResolver.h"
#include "UI/VRExpUIInfoPresentationSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogVRExpDetectableUIInfoTriggerLogic, Log, All);

namespace
{
template <typename TEnum>
FString GetUIInfoEnumDisplayName(TEnum Value)
{
    const UEnum *Enum = StaticEnum<TEnum>();
    return Enum ? Enum->GetDisplayNameTextByValue(static_cast<int64>(Value)).ToString()
                : TEXT("Unknown");
}

FString GetMotionChangeFlagsText(
    EVRExpGrabbableMotionChangeFlags ChangeFlags)
{
    if (ChangeFlags == EVRExpGrabbableMotionChangeFlags::None)
    {
        return TEXT("None");
    }

    FString Result;
    const auto AppendFlag =
        [&Result, ChangeFlags](
            EVRExpGrabbableMotionChangeFlags Flag,
            const TCHAR *Name)
    {
        if (!EnumHasAnyFlags(ChangeFlags, Flag))
        {
            return;
        }
        if (!Result.IsEmpty())
        {
            Result += TEXT("|");
        }
        Result += Name;
    };

    AppendFlag(EVRExpGrabbableMotionChangeFlags::Grip, TEXT("Grip"));
    AppendFlag(
        EVRExpGrabbableMotionChangeFlags::MotionState,
        TEXT("MotionState"));
    AppendFlag(
        EVRExpGrabbableMotionChangeFlags::NormalMotionMode,
        TEXT("NormalMode"));
    AppendFlag(
        EVRExpGrabbableMotionChangeFlags::ReleaseMotionMode,
        TEXT("ReleaseMode"));
    AppendFlag(
        EVRExpGrabbableMotionChangeFlags::UpdatedComponent,
        TEXT("UpdatedComponent"));
    AppendFlag(
        EVRExpGrabbableMotionChangeFlags::RegistrationRebuilt,
        TEXT("RegistrationRebuilt"));
    return Result;
}
}

UVRExpDetectableUIInfoTriggerLogic::UVRExpDetectableUIInfoTriggerLogic()
    : bActivateOnHeadDetection(true),
      bHeadOnlyInNormalPhase(true),
      HeadPresentationMode(EVRExpUIInfoHeadPresentationMode::WhileDetected),
      HeadVisibleDurationSeconds(3.0f),
      HeadDetectionLossBehavior(
          EVRExpUIInfoHeadDetectionLossBehavior::KeepVisibleUntilDurationExpires),
      HeadCooldownSeconds(10.0f),
      HeadCooldownCompletionBehavior(
          EVRExpUIInfoHeadCooldownCompletionBehavior::ReactivateIfStillDetected),
      bActivateWhileGripped(true),
      ReleaseBehavior(EVRExpUIInfoReleaseBehavior::DeferToCurrentInteraction),
      bEnableMotionOnlyPresentation(false),
      MotionLocalPlayerIndex(0),
      HideDelaySeconds(3.0f),
      DestroyDelaySeconds(20.0f),
      bRestoreAfterPriorityLoss(true),
      bKeepExclusiveClaimDuringGracePeriod(true),
      GracePeriodPriority(0),
      bDrawRuntimeDebug(false),
      bDrawDebugWorldGeometry(true),
      bDrawDebugScreenSummary(true),
      DebugAxisLength(20.0f),
      DebugScreenTextScale(1.0f),
#if WITH_EDITORONLY_DATA
      EditorPreviewMode(EVRExpUIInfoEditorPreviewMode::Disabled),
      EditorPreviewSource(EVRExpUIInfoEditorPreviewSource::Head),
      EditorPreviewStatus(EVRExpUIInfoEditorPreviewStatus::Disabled),
#endif
      CurrentInteractionSource(EVRExpUIInfoInteractionSource::None),
      CachedResolvedOnceBaseTransform(FTransform::Identity),
      LastValidFacingRotation(FQuat::Identity),
      ExitStartWorldTransform(FTransform::Identity),
      ExitTargetWorldTransform(FTransform::Identity),
      ExitVisibilityReason(EVRExpUIInfoVisibilityReason::None),
      ExitTransitionLinearAlpha(0.0f),
      HeadActivationSequence(0),
      GripActivationSequence(0),
      ReleaseActivationSequence(0),
      MotionActivationSequence(0),
      LastInteractionSequence(0),
      bHeadClaimEligible(false),
      bGripClaimEligible(false),
      bReleaseClaimEligible(false),
      bMotionClaimEligible(false),
      bGraceClaimEligible(false),
      bHasLocalInteraction(false),
      bUIActorVisible(false),
      bExclusivePresentationGranted(false),
      bPresentationTargetAvailable(false),
      bWasPriorityDisplaced(false),
      bHasSpawnedUIActor(false),
      bHasResolvedOnceTarget(false),
      bHasLastValidFacingRotation(false),
      bHasValidPresentationTransform(false),
      bTransformSettled(true),
      bAwaitingPresentationTarget(false),
      bHeadTimedPresentationActive(false),
      bHeadCooldownActive(false),
      bAwaitingFreshHeadDetection(false),
      bExitTransitionActive(false),
      bExitTransitionReversing(false),
      bDestroyAfterExitTransition(false),
      bTickRegistered(false),
      bDestroyingUIActorInternally(false),
      bHasWarnedMissingClass(false),
      bHasWarnedInvalidDestroyDelay(false),
      bHasWarnedMissingLocalPlayer(false),
      bHasWarnedInvalidBinding(false),
      bHasWarnedInvalidPositionBinding(false),
      bHasWarnedInvalidFacingAxes(false)
{
    HeadPresentationSettings.Priority = 1;
    GripPresentationSettings.Priority = 2;
    ReleasePresentationSettings.Priority = 2;

    MotionPresentationSettings.AnchorType =
        EVRExpUIInfoAnchorType::MotionUpdatedComponent;
    MotionPresentationSettings.BindingMode =
        EVRExpUIInfoBindingMode::Follow;
    MotionPresentationSettings.PositionMode =
        EVRExpUIInfoPositionMode::BetweenAnchorAndCamera;
    MotionPresentationSettings.BetweenRatio = 0.25f;
    MotionPresentationSettings.MinDistanceFromAnchor = 10.0f;
    MotionPresentationSettings.MinDistanceFromCamera = 30.0f;
    MotionPresentationSettings.BetweenVerticalOffset = 10.0f;
    MotionPresentationSettings.OrientationMode =
        EVRExpUIInfoOrientationMode::FaceCamera;
    MotionPresentationSettings.LocalFacingAxis =
        EVRExpUIInfoAxisDirection::PositiveX;
    MotionPresentationSettings.LocalUpAxis =
        EVRExpUIInfoAxisDirection::PositiveZ;
    MotionPresentationSettings.ScaleMode =
        EVRExpUIInfoScaleMode::KeepWorldScale;
    MotionPresentationSettings.MissingAnchorPolicy =
        EVRExpUIInfoMissingAnchorPolicy::HideAndRetry;
    MotionPresentationSettings.PresentationPolicy =
        EVRExpUIInfoPresentationPolicy::ExclusivePerLocalPlayer;
    MotionPresentationSettings.Priority = 0;
}

#if WITH_EDITOR
void UVRExpDetectableUIInfoTriggerLogic::PostEditChangeProperty(
    FPropertyChangedEvent &PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    FVRExpUIInfoPlacementResolver::NormalizeSettings(
        HeadPresentationSettings);
    FVRExpUIInfoPlacementResolver::NormalizeSettings(
        GripPresentationSettings);
    FVRExpUIInfoPlacementResolver::NormalizeSettings(
        ReleasePresentationSettings);
    FVRExpUIInfoPlacementResolver::NormalizeSettings(
        MotionPresentationSettings);
    HeadVisibleDurationSeconds =
        FMath::Max(0.1f, HeadVisibleDurationSeconds);
    HeadCooldownSeconds =
        FMath::Max(0.1f, HeadCooldownSeconds);
    ExitSettings.DurationSeconds =
        FMath::Max(0.0f, ExitSettings.DurationSeconds);
    ExitSettings.EaseExponent =
        FMath::Max(0.01f, ExitSettings.EaseExponent);

    if (!IsTimedHeadPresentationEnabled())
    {
        CancelHeadTimingTimers();
    }

    if (CurrentInteractionSource == EVRExpUIInfoInteractionSource::HeadDetection)
    {
        CurrentPresentationSettings = HeadPresentationSettings;
        ResetResolvedOnceTarget();
        bTransformSettled = false;
        TickPresentation(0.0f);
    }
    else if (CurrentInteractionSource == EVRExpUIInfoInteractionSource::Grip)
    {
        CurrentPresentationSettings = GripPresentationSettings;
        ResetResolvedOnceTarget();
        bTransformSettled = false;
        TickPresentation(0.0f);
    }
    else if (CurrentInteractionSource == EVRExpUIInfoInteractionSource::Release)
    {
        CurrentPresentationSettings =
            GetPresentationSettings(EVRExpUIInfoInteractionSource::Release);
        ResetResolvedOnceTarget();
        bTransformSettled = false;
        if (ReleaseBehavior ==
            EVRExpUIInfoReleaseBehavior::HideDuringRelease)
        {
            ApplyHiddenReleasePresentation(LastContext);
        }
        else
        {
            TickPresentation(0.0f);
        }
    }
    else if (CurrentInteractionSource == EVRExpUIInfoInteractionSource::Motion)
    {
        CurrentPresentationSettings = MotionPresentationSettings;
        ResetResolvedOnceTarget();
        bTransformSettled = false;
        TickPresentation(0.0f);
    }

    BroadcastPresentationChanged();
    UpdateRuntimeDebugState();
    DrawRuntimeDebug();
    RefreshTickRegistration();
}

void UVRExpDetectableUIInfoTriggerLogic::SetEditorPreviewStatus(
    EVRExpUIInfoEditorPreviewStatus NewStatus,
    const FString &FailureReason)
{
#if WITH_EDITORONLY_DATA
    EditorPreviewStatus = NewStatus;
    EditorPreviewFailureReason =
        NewStatus == EVRExpUIInfoEditorPreviewStatus::Active ||
                NewStatus == EVRExpUIInfoEditorPreviewStatus::Disabled
            ? FString()
            : FailureReason;
#endif
}
#endif

AActor *UVRExpDetectableUIInfoTriggerLogic::GetSpawnedUIActor() const
{
    return SpawnedUIActor.Get();
}

bool UVRExpDetectableUIInfoTriggerLogic::IsUIActorVisible() const
{
    return bUIActorVisible && SpawnedUIActor.IsValid();
}

FVRExpUIInfoRuntimeDebugState
UVRExpDetectableUIInfoTriggerLogic::GetRuntimeDebugState() const
{
    return RuntimeDebugState;
}

void UVRExpDetectableUIInfoTriggerLogic::OnInitialized(
    const FVRExpDetectableInteractionContext &InitialContext)
{
    CancelHeadTimingTimers();
    ResetExitTransitionState(true);
    LastContext = InitialContext;
    SpawnedUIActor.Reset();
    LocalPlayerController.Reset();
    LastReleasedLocalPlayerController.Reset();
    CurrentInteractionSource = EVRExpUIInfoInteractionSource::None;
    CurrentPresentationSettings = HeadPresentationSettings;
    CachedResolvedOnceBaseTransform = FTransform::Identity;
    CachedResolvedOnceAnchor = FResolvedAnchor();
    LastValidFacingRotation = FQuat::Identity;
    ExitStartWorldTransform = FTransform::Identity;
    ExitTargetWorldTransform = FTransform::Identity;
    ExitVisibilityReason = EVRExpUIInfoVisibilityReason::None;
    ExitTransitionLinearAlpha = 0.0f;
    HeadActivationSequence = 0;
    GripActivationSequence = 0;
    ReleaseActivationSequence = 0;
    MotionActivationSequence = 0;
    LastInteractionSequence = 0;
    bHeadClaimEligible = false;
    bGripClaimEligible = false;
    bReleaseClaimEligible = false;
    bMotionClaimEligible = false;
    bGraceClaimEligible = false;
    bHasLocalInteraction = false;
    bUIActorVisible = false;
    bExclusivePresentationGranted = false;
    bPresentationTargetAvailable = false;
    bWasPriorityDisplaced = false;
    bHasSpawnedUIActor = false;
    bHasResolvedOnceTarget = false;
    bHasLastValidFacingRotation = false;
    bHasValidPresentationTransform = false;
    bTransformSettled = true;
    bAwaitingPresentationTarget = false;
    bHeadTimedPresentationActive = false;
    bHeadCooldownActive = false;
    bAwaitingFreshHeadDetection = false;
    bExitTransitionActive = false;
    bExitTransitionReversing = false;
    bDestroyAfterExitTransition = false;
    bTickRegistered = false;
    bDestroyingUIActorInternally = false;
    bHasWarnedMissingClass = false;
    bHasWarnedInvalidDestroyDelay = false;
    bHasWarnedMissingLocalPlayer = false;
    bHasWarnedInvalidBinding = false;
    bHasWarnedInvalidPositionBinding = false;
    bHasWarnedInvalidFacingAxes = false;
    LastPlacementFailureReason.Reset();
    RuntimeDebugState = FVRExpUIInfoRuntimeDebugState();

    if (DetectableComponent.IsValid())
    {
        DetectableComponent->OnHeadDetectionStarted.AddUniqueDynamic(
            this,
            &UVRExpDetectableUIInfoTriggerLogic::HandleHeadDetectionStarted);
    }

    RefreshTickRegistration();
}

void UVRExpDetectableUIInfoTriggerLogic::OnActivated(
    const FVRExpDetectableInteractionContext &Context)
{
    HandleActiveContext(Context);
}

void UVRExpDetectableUIInfoTriggerLogic::OnActiveContextUpdated(
    const FVRExpDetectableInteractionContext &Context)
{
    HandleActiveContext(Context);
}

void UVRExpDetectableUIInfoTriggerLogic::HandleHeadDetectionStarted(
    const FVRExpDetectableInteractionContext &Context)
{
    if (!bAwaitingFreshHeadDetection || bHeadCooldownActive)
    {
        return;
    }

    bAwaitingFreshHeadDetection = false;
    UpdateRuntimeDebugState();
    DrawRuntimeDebug();
}

bool UVRExpDetectableUIInfoTriggerLogic::EvaluateSourceSwitches(
    const FVRExpDetectableInteractionContext &Context,
    EVRExpDetectableActivationMatchSource &OutMatchedSource,
    FString &OutFailureReason) const
{
    OutMatchedSource =
        EVRExpDetectableActivationMatchSource::None;
    OutFailureReason.Reset();

    if (IsGripSourceEligible(Context))
    {
        OutMatchedSource =
            EVRExpDetectableActivationMatchSource::Grip;
        return true;
    }
    if (IsReleaseSourceEligible(Context))
    {
        OutMatchedSource =
            EVRExpDetectableActivationMatchSource::Release;
        return true;
    }
    if (IsHeadSourceEligible(Context))
    {
        OutMatchedSource =
            EVRExpDetectableActivationMatchSource::HeadDetection;
        return true;
    }
    if (IsMotionSourceEligible(Context))
    {
        OutMatchedSource =
            EVRExpDetectableActivationMatchSource::Motion;
        return true;
    }

    const bool bAnySourceEnabled =
        bActivateOnHeadDetection ||
        bActivateWhileGripped ||
        ReleaseBehavior !=
            EVRExpUIInfoReleaseBehavior::DeferToCurrentInteraction ||
        bEnableMotionOnlyPresentation;
    if (!bAnySourceEnabled)
    {
        OutFailureReason =
            TEXT("No presentation sources are enabled.");
    }
    else if (Context.bIsGripped &&
             !bActivateWhileGripped)
    {
        OutFailureReason =
            TEXT("Grip is active, but Activate While Gripped is disabled.");
    }
    else if (
        Context.MotionPhase ==
            EVRExpGrabbableMotionPhase::Releasing &&
        ReleaseBehavior ==
            EVRExpUIInfoReleaseBehavior::
                DeferToCurrentInteraction)
    {
        OutFailureReason =
            TEXT("The Motion Phase is Releasing, but Release Behavior does not create an independent Release source.");
    }
    else if (Context.bIsHeadDetected &&
             !bActivateOnHeadDetection)
    {
        OutFailureReason =
            TEXT("Head Detection is active, but Activate On Head Detection is disabled.");
    }
    else if (Context.bIsHeadDetected &&
             IsTimedHeadPresentationEnabled() &&
             bHeadCooldownActive)
    {
        OutFailureReason =
            TEXT("Head Detection is active, but the timed Head presentation is cooling down.");
    }
    else if (Context.bIsHeadDetected &&
             IsTimedHeadPresentationEnabled() &&
             bAwaitingFreshHeadDetection)
    {
        OutFailureReason =
            TEXT("Head Detection is active, but a new detection begin is required after cooldown.");
    }
    else if (bActivateOnHeadDetection &&
             Context.bIsHeadDetected &&
             bHeadOnlyInNormalPhase &&
             IsValid(Context.GrabbableMotionComponent.Get()) &&
             Context.MotionPhase !=
                 EVRExpGrabbableMotionPhase::Normal)
    {
        OutFailureReason =
            TEXT("Head Detection is active, but the Motion Phase is not Normal.");
    }
    else if (
        bEnableMotionOnlyPresentation &&
        !IsValid(Context.GrabbableMotionComponent.Get()))
    {
        OutFailureReason =
            TEXT("Motion-Only Presentation is enabled, but no authoritative Motion Component is available.");
    }
    else
    {
        OutFailureReason =
            TEXT("No enabled presentation source matches the current interaction context.");
    }
    return false;
}

void UVRExpDetectableUIInfoTriggerLogic::OnDeactivated(
    const FVRExpDetectableInteractionContext &Context)
{
    if (ShouldEndTimedHeadPresentationOnDetectionLoss(Context))
    {
        EndTimedHeadPresentationState();
        bGraceClaimEligible = false;
        if (CurrentInteractionSource ==
                EVRExpUIInfoInteractionSource::HeadDetection &&
            SpawnedUIActor.IsValid())
        {
            RemoveExclusiveClaim();
            SetUIActorVisible(
                false,
                EVRExpUIInfoVisibilityReason::HeadDetectionLost);
        }
    }

    BeginGracePeriod(Context, true);
}

void UVRExpDetectableUIInfoTriggerLogic::BeginGracePeriod(
    const FVRExpDetectableInteractionContext &Context,
    bool bRecordContextTransition)
{
    LastContext = Context;
    if (bRecordContextTransition)
    {
        RecordContextTransition(Context);
    }

    bHasLocalInteraction = false;
    CancelInactivityTimers();

    if (!SpawnedUIActor.IsValid())
    {
        bGraceClaimEligible = false;
        bAwaitingPresentationTarget = false;
        CurrentInteractionSource =
            EVRExpUIInfoInteractionSource::GracePeriod;
        ResetResolvedOnceTarget();
        HandleExternalUIActorDestruction(nullptr);
        UpdateRuntimeDebugState();
        DrawRuntimeDebug();
        RefreshTickRegistration();
        return;
    }

    const bool bKeepExclusiveGraceClaim =
        IsExclusivePresentation() &&
        bKeepExclusiveClaimDuringGracePeriod &&
        bExclusivePresentationGranted &&
        bUIActorVisible;
    bGraceClaimEligible = bKeepExclusiveGraceClaim;

    CurrentInteractionSource = EVRExpUIInfoInteractionSource::GracePeriod;
    BroadcastPresentationChanged();

    if (bKeepExclusiveGraceClaim)
    {
        UpdateExclusiveClaim(true);
    }
    else
    {
        RemoveExclusiveClaim();
        if (IsExclusivePresentation() && bUIActorVisible &&
            !bExitTransitionActive)
        {
            SetUIActorVisible(
                false,
                EVRExpUIInfoVisibilityReason::InactivityTimeout);
        }
    }

    StartInactivityTimers();
    UpdateRuntimeDebugState();
    DrawRuntimeDebug();
    RefreshTickRegistration();
}

void UVRExpDetectableUIInfoTriggerLogic::OnDeinitialized(
    const FVRExpDetectableInteractionContext &FinalContext)
{
    LastContext = FinalContext;
    if (DetectableComponent.IsValid())
    {
        DetectableComponent->OnHeadDetectionStarted.RemoveDynamic(
            this,
            &UVRExpDetectableUIInfoTriggerLogic::HandleHeadDetectionStarted);
    }
    CancelHeadTimingTimers();
    CancelInactivityTimers();
    RemoveExclusiveClaim();

    if (SpawnedUIActor.IsValid())
    {
        SetUIActorVisible(false, EVRExpUIInfoVisibilityReason::Deinitialized);
        DestroyUIActor(FinalContext);
    }
    else
    {
        HandleExternalUIActorDestruction(nullptr);
    }

    CurrentInteractionSource = EVRExpUIInfoInteractionSource::None;
    HeadActivationSequence = 0;
    GripActivationSequence = 0;
    ReleaseActivationSequence = 0;
    MotionActivationSequence = 0;
    LastInteractionSequence = 0;
    bHeadClaimEligible = false;
    bGripClaimEligible = false;
    bReleaseClaimEligible = false;
    bMotionClaimEligible = false;
    bGraceClaimEligible = false;
    bHasLocalInteraction = false;
    bAwaitingPresentationTarget = false;
    bHeadTimedPresentationActive = false;
    bHeadCooldownActive = false;
    bAwaitingFreshHeadDetection = false;
    ResetResolvedOnceTarget();
    LastPlacementFailureReason.Reset();
    RuntimeDebugState.LastVisibilityReason =
        EVRExpUIInfoVisibilityReason::Deinitialized;
    UpdateRuntimeDebugState();
    ClearDebugScreenMessage();
    RefreshTickRegistration();
}

void UVRExpDetectableUIInfoTriggerLogic::HandleActiveContext(
    const FVRExpDetectableInteractionContext &Context)
{
    const bool bMotionAnchorChanged =
        LastContext.GrabbableMotionComponent !=
            Context.GrabbableMotionComponent ||
        LastContext.MotionUpdatedComponent !=
            Context.MotionUpdatedComponent;
    LastContext = Context;

    if (bMotionAnchorChanged &&
        CurrentPresentationSettings.AnchorType ==
            EVRExpUIInfoAnchorType::MotionUpdatedComponent)
    {
        ResetResolvedOnceTarget();
        bTransformSettled = false;
    }

    if (ShouldEndTimedHeadPresentationOnDetectionLoss(Context))
    {
        EndTimedHeadPresentationState();
    }

    RecordContextTransition(Context);

    EVRExpUIInfoInteractionSource NewSource = DetermineInteractionSource(Context);
    if (NewSource == EVRExpUIInfoInteractionSource::None)
    {
        return;
    }

    APlayerController *ResolvedPlayerController =
        ResolveLocalPlayerController(Context, NewSource);
    if (NewSource == EVRExpUIInfoInteractionSource::Release &&
        Context.ChangeSource == EVRExpDetectableChangeSource::Grip &&
        Context.ChangePhase == EVRExpDetectableChangePhase::Ended &&
        IsValid(ResolvedPlayerController))
    {
        LastReleasedLocalPlayerController = ResolvedPlayerController;
    }
    if (NewSource == EVRExpUIInfoInteractionSource::Release &&
        ReleaseBehavior == EVRExpUIInfoReleaseBehavior::HideDuringRelease)
    {
        if (IsValid(ResolvedPlayerController))
        {
            LocalPlayerController = ResolvedPlayerController;
        }
        ApplyHiddenReleasePresentation(Context);
        return;
    }
    if (!IsValid(ResolvedPlayerController) &&
        NewSource == EVRExpUIInfoInteractionSource::Grip &&
        IsHeadSourceEligible(Context))
    {
        NewSource = EVRExpUIInfoInteractionSource::HeadDetection;
        ResolvedPlayerController = ResolveLocalPlayerController(Context, NewSource);
    }
    if (!IsValid(ResolvedPlayerController) &&
        NewSource != EVRExpUIInfoInteractionSource::Motion &&
        NewSource != EVRExpUIInfoInteractionSource::Release &&
        bEnableMotionOnlyPresentation &&
        IsValid(Context.GrabbableMotionComponent.Get()))
    {
        NewSource = EVRExpUIInfoInteractionSource::Motion;
        ResolvedPlayerController =
            ResolveLocalPlayerController(Context, NewSource);
    }

    if (!IsValid(ResolvedPlayerController))
    {
        if (NewSource != EVRExpUIInfoInteractionSource::Motion &&
            NewSource != EVRExpUIInfoInteractionSource::Release)
        {
            if (bHasLocalInteraction)
            {
                BeginGracePeriod(Context, false);
            }
            return;
        }

        const bool bSourceChanged =
            NewSource != CurrentInteractionSource;
        CurrentInteractionSource = NewSource;
        CurrentPresentationSettings =
            GetPresentationSettings(NewSource);
        LocalPlayerController.Reset();
        bHasLocalInteraction = true;
        bPresentationTargetAvailable = false;
        bAwaitingPresentationTarget = true;
        LastPlacementFailureReason =
            TEXT("No matching local player was found.");
        if (!bHasWarnedMissingLocalPlayer)
        {
            bHasWarnedMissingLocalPlayer = true;
            if (NewSource ==
                EVRExpUIInfoInteractionSource::Release)
            {
                UE_LOG(
                    LogVRExpDetectableUIInfoTriggerLogic,
                    Warning,
                    TEXT("%s cannot resolve a local player for Release presentation; the presentation will remain hidden and retry."),
                    *GetPathName());
            }
            else
            {
                UE_LOG(
                    LogVRExpDetectableUIInfoTriggerLogic,
                    Warning,
                    TEXT("%s cannot resolve Motion Local Player Index %d; the presentation will remain hidden and retry."),
                    *GetPathName(),
                    MotionLocalPlayerIndex);
            }
        }
        RemoveExclusiveClaim();
        if (SpawnedUIActor.IsValid())
        {
            SetUIActorVisible(
                false,
                EVRExpUIInfoVisibilityReason::PlacementTargetUnavailable);
        }
        if (bSourceChanged && SpawnedUIActor.IsValid())
        {
            BroadcastPresentationChanged();
        }
        UpdateRuntimeDebugState();
        DrawRuntimeDebug();
        RefreshTickRegistration();
        return;
    }

    LocalPlayerController = ResolvedPlayerController;
    bGraceClaimEligible = false;
    bHasLocalInteraction = true;
    CancelInactivityTimers();
    ApplyPresentation(NewSource, Context);
}

void UVRExpDetectableUIInfoTriggerLogic::RecordContextTransition(
    const FVRExpDetectableInteractionContext &Context)
{
    UVRExpUIInfoPresentationSubsystem *Subsystem = GetPresentationSubsystem();
    const auto AllocateSequence = [this, Subsystem]()
    {
        const uint64 NewSequence =
            Subsystem
                ? Subsystem->AllocateActivationSequence()
                : FMath::Max(LastInteractionSequence + 1, static_cast<uint64>(1));
        LastInteractionSequence = NewSequence;
        return NewSequence;
    };

    if (!IsActive())
    {
        HeadActivationSequence = 0;
        GripActivationSequence = 0;
        ReleaseActivationSequence = 0;
        MotionActivationSequence = 0;
        bHeadClaimEligible = false;
        bGripClaimEligible = false;
        bReleaseClaimEligible = false;
        bMotionClaimEligible = false;
        return;
    }

    if (Context.ChangeSource == EVRExpDetectableChangeSource::HeadDetection)
    {
        if (Context.ChangePhase == EVRExpDetectableChangePhase::Began &&
            IsHeadSourceEligible(Context))
        {
            HeadActivationSequence = AllocateSequence();
            bHeadClaimEligible = true;
        }
        else if (Context.ChangePhase == EVRExpDetectableChangePhase::Ended &&
                 !IsHeadSourceEligible(Context))
        {
            HeadActivationSequence = 0;
            bHeadClaimEligible = false;
        }
    }
    else if (Context.ChangeSource == EVRExpDetectableChangeSource::Grip)
    {
        if (Context.ChangePhase == EVRExpDetectableChangePhase::Began &&
            IsGripSourceEligible(Context))
        {
            GripActivationSequence = AllocateSequence();
            bGripClaimEligible = true;
        }
        else if (Context.ChangePhase == EVRExpDetectableChangePhase::Ended)
        {
            GripActivationSequence = 0;
            bGripClaimEligible = false;
        }
    }

    const bool bHeadIsPresentationCandidate =
        IsHeadSourceEligible(Context);
    if (bHeadIsPresentationCandidate &&
        HeadActivationSequence == 0)
    {
        HeadActivationSequence = AllocateSequence();
        bHeadClaimEligible = true;
    }
    else if (!bHeadIsPresentationCandidate)
    {
        HeadActivationSequence = 0;
        bHeadClaimEligible = false;
    }

    const bool bGripIsPresentationCandidate =
        IsGripSourceEligible(Context);
    if (bGripIsPresentationCandidate &&
        GripActivationSequence == 0)
    {
        GripActivationSequence = AllocateSequence();
        bGripClaimEligible = true;
    }
    else if (!bGripIsPresentationCandidate)
    {
        GripActivationSequence = 0;
        bGripClaimEligible = false;
    }

    const bool bReleaseIsPresentationCandidate =
        IsReleaseSourceEligible(Context);
    if (bReleaseIsPresentationCandidate &&
        ReleaseActivationSequence == 0)
    {
        ReleaseActivationSequence = AllocateSequence();
        bReleaseClaimEligible =
            ReleaseBehavior !=
            EVRExpUIInfoReleaseBehavior::HideDuringRelease;
    }
    else if (!bReleaseIsPresentationCandidate)
    {
        ReleaseActivationSequence = 0;
        bReleaseClaimEligible = false;
    }

    const bool bHasResolvableLocalGrip =
        bGripIsPresentationCandidate &&
        IsValid(ResolveLocalPlayerController(
            Context,
            EVRExpUIInfoInteractionSource::Grip));
    const bool bHasResolvableLocalHead =
        bHeadIsPresentationCandidate &&
        IsValid(ResolveLocalPlayerController(
            Context,
            EVRExpUIInfoInteractionSource::HeadDetection));
    const bool bMotionIsPresentationCandidate =
        IsMotionSourceEligible(Context) &&
        !bHasResolvableLocalGrip &&
        !bHasResolvableLocalHead &&
        !bReleaseIsPresentationCandidate;
    if (bMotionIsPresentationCandidate &&
        MotionActivationSequence == 0)
    {
        MotionActivationSequence = AllocateSequence();
        bMotionClaimEligible = true;
    }
    else if (!bMotionIsPresentationCandidate)
    {
        MotionActivationSequence = 0;
        bMotionClaimEligible = false;
    }
}

EVRExpUIInfoInteractionSource
UVRExpDetectableUIInfoTriggerLogic::DetermineInteractionSource(
    const FVRExpDetectableInteractionContext &Context) const
{
    return ResolveVRExpUIInfoInteractionSource(
        IsGripSourceEligible(Context),
        Context.MotionPhase,
        ReleaseBehavior,
        IsHeadSourceEligible(Context),
        IsMotionSourceEligible(Context),
        true);
}

bool UVRExpDetectableUIInfoTriggerLogic::IsHeadSourceEligible(
    const FVRExpDetectableInteractionContext &Context) const
{
    if (!bActivateOnHeadDetection)
    {
        return false;
    }

    if (HeadPresentationMode ==
        EVRExpUIInfoHeadPresentationMode::TimedWithCooldown)
    {
        if (bHeadCooldownActive || bAwaitingFreshHeadDetection)
        {
            return false;
        }

        if (bHeadTimedPresentationActive)
        {
            return Context.bIsHeadDetected ||
                   HeadDetectionLossBehavior ==
                       EVRExpUIInfoHeadDetectionLossBehavior::
                           KeepVisibleUntilDurationExpires;
        }
    }

    if (!Context.bIsHeadDetected)
    {
        return false;
    }
    if (!bHeadOnlyInNormalPhase ||
        !IsValid(Context.GrabbableMotionComponent.Get()))
    {
        return true;
    }
    return Context.MotionPhase ==
           EVRExpGrabbableMotionPhase::Normal;
}

bool UVRExpDetectableUIInfoTriggerLogic::IsGripSourceEligible(
    const FVRExpDetectableInteractionContext &Context) const
{
    return Context.bIsGripped &&
           bActivateWhileGripped;
}

bool UVRExpDetectableUIInfoTriggerLogic::IsReleaseSourceEligible(
    const FVRExpDetectableInteractionContext &Context) const
{
    return Context.MotionPhase ==
               EVRExpGrabbableMotionPhase::Releasing &&
           ReleaseBehavior !=
               EVRExpUIInfoReleaseBehavior::DeferToCurrentInteraction;
}

bool UVRExpDetectableUIInfoTriggerLogic::IsMotionSourceEligible(
    const FVRExpDetectableInteractionContext &Context) const
{
    return bEnableMotionOnlyPresentation &&
           IsValid(Context.GrabbableMotionComponent.Get());
}

uint64 UVRExpDetectableUIInfoTriggerLogic::GetActivationSequenceForSource(
    EVRExpUIInfoInteractionSource InteractionSource) const
{
    switch (InteractionSource)
    {
    case EVRExpUIInfoInteractionSource::HeadDetection:
        return HeadActivationSequence;

    case EVRExpUIInfoInteractionSource::Grip:
        return GripActivationSequence;

    case EVRExpUIInfoInteractionSource::Release:
        return ReleaseActivationSequence;

    case EVRExpUIInfoInteractionSource::Motion:
        return MotionActivationSequence;

    case EVRExpUIInfoInteractionSource::GracePeriod:
        return LastInteractionSequence;

    default:
        return 0;
    }
}

bool UVRExpDetectableUIInfoTriggerLogic::IsSourceClaimEligible(
    EVRExpUIInfoInteractionSource InteractionSource) const
{
    switch (InteractionSource)
    {
    case EVRExpUIInfoInteractionSource::HeadDetection:
        return bHeadClaimEligible;

    case EVRExpUIInfoInteractionSource::Grip:
        return bGripClaimEligible;

    case EVRExpUIInfoInteractionSource::Release:
        return bReleaseClaimEligible;

    case EVRExpUIInfoInteractionSource::Motion:
        return bMotionClaimEligible;

    case EVRExpUIInfoInteractionSource::GracePeriod:
        return bGraceClaimEligible;

    default:
        return false;
    }
}

void UVRExpDetectableUIInfoTriggerLogic::SetSourceClaimEligible(
    EVRExpUIInfoInteractionSource InteractionSource,
    bool bEligible)
{
    if (InteractionSource == EVRExpUIInfoInteractionSource::HeadDetection)
    {
        bHeadClaimEligible = bEligible;
    }
    else if (InteractionSource == EVRExpUIInfoInteractionSource::Grip)
    {
        bGripClaimEligible = bEligible;
    }
    else if (InteractionSource == EVRExpUIInfoInteractionSource::Release)
    {
        bReleaseClaimEligible = bEligible;
    }
    else if (InteractionSource == EVRExpUIInfoInteractionSource::Motion)
    {
        bMotionClaimEligible = bEligible;
    }
    else if (InteractionSource == EVRExpUIInfoInteractionSource::GracePeriod)
    {
        bGraceClaimEligible = bEligible;
    }
}

bool UVRExpDetectableUIInfoTriggerLogic::EnsureUIActor(
    const FVRExpDetectableInteractionContext &Context)
{
    if (bHasSpawnedUIActor && !SpawnedUIActor.IsValid())
    {
        HandleExternalUIActorDestruction(nullptr);
    }

    if (SpawnedUIActor.IsValid())
    {
        return true;
    }

    if (!UIActorClass)
    {
        LastPlacementFailureReason = TEXT("UIActorClass is not configured.");
        bAwaitingPresentationTarget = false;
        if (!bHasWarnedMissingClass)
        {
            bHasWarnedMissingClass = true;
            UE_LOG(LogVRExpDetectableUIInfoTriggerLogic, Warning,
                   TEXT("%s cannot create a UI Info Actor because UIActorClass is not configured."),
                   *GetPathName());
        }
        return false;
    }

    APlayerController *ResolvedPlayerController =
        LocalPlayerController.IsValid()
            ? LocalPlayerController.Get()
            : ResolveLocalPlayerController(Context, CurrentInteractionSource);
    if (!IsValid(ResolvedPlayerController))
    {
        LastPlacementFailureReason = TEXT("No matching local player was found.");
        bAwaitingPresentationTarget = bHasLocalInteraction;
        if (!bHasWarnedMissingLocalPlayer)
        {
            bHasWarnedMissingLocalPlayer = true;
            UE_LOG(LogVRExpDetectableUIInfoTriggerLogic, Warning,
                   TEXT("%s cannot create a UI Info Actor because no matching local player was found."),
                   *GetPathName());
        }
        return false;
    }

    UWorld *World = GetOwningWorld();
    AActor *DetectableOwner =
        DetectableComponent.IsValid() ? DetectableComponent->GetOwner() : nullptr;
    if (!World || !IsValid(DetectableOwner))
    {
        LastPlacementFailureReason = TEXT("The detectable owner or world is unavailable.");
        bAwaitingPresentationTarget = false;
        return false;
    }

    LocalPlayerController = ResolvedPlayerController;

    FTransform EntryWorldTransform = FTransform::Identity;
    FString EntryFailureReason;
    if (!ResolveEntryWorldTransform(EntryWorldTransform, EntryFailureReason))
    {
        LastPlacementFailureReason =
            EntryFailureReason.IsEmpty()
                ? TEXT("The configured entry transform could not be resolved.")
                : EntryFailureReason;
        bAwaitingPresentationTarget = true;
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = DetectableOwner;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor *NewUIActor =
        World->SpawnActor<AActor>(UIActorClass, EntryWorldTransform, SpawnParameters);
    if (!IsValid(NewUIActor))
    {
        LastPlacementFailureReason = TEXT("SpawnActor failed for UIActorClass.");
        UE_LOG(LogVRExpDetectableUIInfoTriggerLogic, Warning,
               TEXT("%s failed to spawn UI Info Actor class %s."),
               *GetPathName(), *GetNameSafe(UIActorClass.Get()));
        return false;
    }

    NewUIActor->SetReplicates(false);
    NewUIActor->SetReplicateMovement(false);
    NewUIActor->SetActorHiddenInGame(true);
    NewUIActor->OnDestroyed.AddUniqueDynamic(
        this, &UVRExpDetectableUIInfoTriggerLogic::HandleSpawnedUIActorDestroyed);

    SpawnedUIActor = NewUIActor;
    bHasSpawnedUIActor = true;
    bUIActorVisible = false;
    ResetExitTransitionState(false);
    ExitVisibilityReason = EVRExpUIInfoVisibilityReason::None;
    bExclusivePresentationGranted = false;
    bTransformSettled = EntrySettings.EntryMode == EVRExpUIInfoEntryMode::AtTarget;
    bHasValidPresentationTransform =
        EntrySettings.EntryMode == EVRExpUIInfoEntryMode::AtTarget;
    bAwaitingPresentationTarget = false;
    LastPlacementFailureReason.Reset();

    if (NewUIActor->GetClass()->ImplementsInterface(
            UVRExpUIInfoActorInterface::StaticClass()))
    {
        IVRExpUIInfoActorInterface::Execute_OnUIInfoInitialized(
            NewUIActor, DetectableComponent.Get(), Context);
    }
    OnUIActorSpawned.Broadcast(NewUIActor, Context);

    RefreshTickRegistration();
    return true;
}

bool UVRExpDetectableUIInfoTriggerLogic::ResolveEntryWorldTransform(
    FTransform &OutTransform,
    FString &OutFailureReason)
{
    AActor *DetectableOwner =
        DetectableComponent.IsValid() ? DetectableComponent->GetOwner() : nullptr;
    if (!IsValid(DetectableOwner))
    {
        OutFailureReason = TEXT("The detectable owner is unavailable.");
        return false;
    }

    switch (EntrySettings.EntryMode)
    {
    case EVRExpUIInfoEntryMode::AtTarget:
    {
        FResolvedAnchor Anchor;
        return ResolvePresentationTarget(OutTransform, Anchor, OutFailureReason);
    }

    case EVRExpUIInfoEntryMode::AtOwner:
        OutTransform =
            EntrySettings.RelativeOffset * DetectableOwner->GetActorTransform();
        return true;

    case EVRExpUIInfoEntryMode::AtComponent:
    {
        FResolvedAnchor Anchor;
        if (!ResolveComponentAnchor(
                EntrySettings.EntryComponent,
                EntrySettings.EntrySocketName,
                Anchor,
                OutFailureReason))
        {
            return false;
        }
        OutTransform = EntrySettings.RelativeOffset * Anchor.WorldTransform;
        return true;
    }

    case EVRExpUIInfoEntryMode::AtWorldTransform:
        OutTransform = EntrySettings.WorldTransform;
        return true;

    default:
        OutFailureReason = TEXT("EntryMode is invalid.");
        return false;
    }
}

void UVRExpDetectableUIInfoTriggerLogic::HandleSpawnedUIActorDestroyed(
    AActor *DestroyedActor)
{
    if (bDestroyingUIActorInternally)
    {
        return;
    }
    if (SpawnedUIActor.IsValid() &&
        SpawnedUIActor.Get() != DestroyedActor)
    {
        return;
    }
    HandleExternalUIActorDestruction(DestroyedActor);
}

void UVRExpDetectableUIInfoTriggerLogic::HandleExternalUIActorDestruction(
    AActor *DestroyedActor)
{
    if (!bHasSpawnedUIActor)
    {
        return;
    }

    RemoveExclusiveClaim();
    if (UVRExpUIInfoPresentationSubsystem *Subsystem = GetPresentationSubsystem())
    {
        Subsystem->UnregisterTickLogic(this);
    }
    bTickRegistered = false;

    CancelInactivityTimers();
    ResetExitTransitionState(false);
    SpawnedUIActor.Reset();
    bHasSpawnedUIActor = false;
    bUIActorVisible = false;
    bExclusivePresentationGranted = false;
    bPresentationTargetAvailable = false;
    bHasValidPresentationTransform = false;
    bGraceClaimEligible = false;
    bTransformSettled = true;
    bAwaitingPresentationTarget = bHasLocalInteraction;
    ResetResolvedOnceTarget();
    LastValidFacingRotation = FQuat::Identity;
    bHasLastValidFacingRotation = false;

    if (DestroyedActor)
    {
        OnUIActorDestroyed.Broadcast(DestroyedActor, LastContext);
    }

    if (bHasLocalInteraction)
    {
        LastPlacementFailureReason =
            TEXT("The UI Actor was destroyed externally; waiting to respawn.");
    }

    UpdateRuntimeDebugState();
    RefreshTickRegistration();
}

void UVRExpDetectableUIInfoTriggerLogic::DestroyUIActor(
    const FVRExpDetectableInteractionContext &Context)
{
    AActor *ActorToDestroy = SpawnedUIActor.Get();
    if (!IsValid(ActorToDestroy))
    {
        HandleExternalUIActorDestruction(nullptr);
        return;
    }

    CancelInactivityTimers();
    RemoveExclusiveClaim();
    if (UVRExpUIInfoPresentationSubsystem *Subsystem = GetPresentationSubsystem())
    {
        Subsystem->UnregisterTickLogic(this);
    }
    bTickRegistered = false;

    if (bUIActorVisible)
    {
        SetUIActorVisibleImmediately(
            false, EVRExpUIInfoVisibilityReason::InactivityTimeout);
    }

    bDestroyingUIActorInternally = true;
    if (ActorToDestroy->GetClass()->ImplementsInterface(
            UVRExpUIInfoActorInterface::StaticClass()))
    {
        IVRExpUIInfoActorInterface::Execute_OnUIInfoAboutToDestroy(
            ActorToDestroy, Context);
    }

    if (IsValid(ActorToDestroy))
    {
        ActorToDestroy->OnDestroyed.RemoveDynamic(
            this,
            &UVRExpDetectableUIInfoTriggerLogic::HandleSpawnedUIActorDestroyed);
        ActorToDestroy->Destroy();
    }
    bDestroyingUIActorInternally = false;

    SpawnedUIActor.Reset();
    bHasSpawnedUIActor = false;
    bUIActorVisible = false;
    bExclusivePresentationGranted = false;
    bPresentationTargetAvailable = false;
    bHasValidPresentationTransform = false;
    bGraceClaimEligible = false;
    bTransformSettled = true;
    bAwaitingPresentationTarget = false;
    OnUIActorDestroyed.Broadcast(ActorToDestroy, Context);
    UpdateRuntimeDebugState();
    RefreshTickRegistration();
}

void UVRExpDetectableUIInfoTriggerLogic::ApplyPresentation(
    EVRExpUIInfoInteractionSource NewSource,
    const FVRExpDetectableInteractionContext &Context)
{
    const bool bSourceChanged = NewSource != CurrentInteractionSource;
    CurrentInteractionSource = NewSource;
    CurrentPresentationSettings = GetPresentationSettings(NewSource);
    if (CurrentPresentationSettings.AnchorType ==
            EVRExpUIInfoAnchorType::World &&
        CurrentPresentationSettings.BindingMode !=
            EVRExpUIInfoBindingMode::ResolveOnce)
    {
        if (!bHasWarnedInvalidBinding)
        {
            bHasWarnedInvalidBinding = true;
            UE_LOG(
                LogVRExpDetectableUIInfoTriggerLogic,
                Warning,
                TEXT("%s uses a World anchor with a non-ResolveOnce binding; ResolveOnce is enforced."),
                *GetPathName());
        }
        CurrentPresentationSettings.BindingMode =
            EVRExpUIInfoBindingMode::ResolveOnce;
    }
    if (CurrentPresentationSettings.PositionMode ==
            EVRExpUIInfoPositionMode::BetweenAnchorAndCamera &&
        CurrentPresentationSettings.BindingMode ==
            EVRExpUIInfoBindingMode::Attach)
    {
        if (!bHasWarnedInvalidPositionBinding)
        {
            bHasWarnedInvalidPositionBinding = true;
            UE_LOG(
                LogVRExpDetectableUIInfoTriggerLogic,
                Warning,
                TEXT("%s uses BetweenAnchorAndCamera with Attach; Follow is enforced because the target depends on both anchor and camera."),
                *GetPathName());
        }
        CurrentPresentationSettings.BindingMode =
            EVRExpUIInfoBindingMode::Follow;
    }

    if (bSourceChanged)
    {
        bWasPriorityDisplaced = false;
        bHasLastValidFacingRotation = false;
        ResetResolvedOnceTarget();
    }

    const bool bAlreadyHadActor = SpawnedUIActor.IsValid();
    if (!EnsureUIActor(Context))
    {
        UpdateRuntimeDebugState();
        DrawRuntimeDebug();
        RefreshTickRegistration();
        return;
    }

    if (bSourceChanged || !bAlreadyHadActor)
    {
        BroadcastPresentationChanged();
    }

    if (bExitTransitionActive)
    {
        ReverseExitTransition();
        StartHeadVisibleTimerIfNeeded();
        RefreshHeadVisibleTimerCountingState();
    }

    TickPresentation(0.0f);
}

void UVRExpDetectableUIInfoTriggerLogic::ApplyHiddenReleasePresentation(
    const FVRExpDetectableInteractionContext &Context)
{
    const bool bSourceChanged =
        CurrentInteractionSource != EVRExpUIInfoInteractionSource::Release;
    LastContext = Context;
    CurrentInteractionSource = EVRExpUIInfoInteractionSource::Release;
    CurrentPresentationSettings =
        GetPresentationSettings(EVRExpUIInfoInteractionSource::Release);
    bGraceClaimEligible = false;
    bHasLocalInteraction = true;
    bPresentationTargetAvailable = false;
    bAwaitingPresentationTarget = false;
    bTransformSettled = true;
    LastPlacementFailureReason.Reset();
    CancelInactivityTimers();
    RemoveExclusiveClaim();
    ResetResolvedOnceTarget();

    if (bSourceChanged && SpawnedUIActor.IsValid())
    {
        BroadcastPresentationChanged();
    }
    if (SpawnedUIActor.IsValid())
    {
        SetUIActorVisible(
            false,
            EVRExpUIInfoVisibilityReason::HiddenDuringRelease);
    }

    UpdateRuntimeDebugState();
    DrawRuntimeDebug();
    RefreshTickRegistration();
}

const FVRExpUIInfoPresentationSettings &
UVRExpDetectableUIInfoTriggerLogic::GetPresentationSettings(
    EVRExpUIInfoInteractionSource InteractionSource) const
{
    switch (InteractionSource)
    {
    case EVRExpUIInfoInteractionSource::Grip:
        return GripPresentationSettings;

    case EVRExpUIInfoInteractionSource::Release:
        return ReleaseBehavior ==
                       EVRExpUIInfoReleaseBehavior::KeepGripPresentation
                   ? GripPresentationSettings
                   : ReleasePresentationSettings;

    case EVRExpUIInfoInteractionSource::Motion:
        return MotionPresentationSettings;

    default:
        return HeadPresentationSettings;
    }
}

EVRExpUIInfoBindingMode
UVRExpDetectableUIInfoTriggerLogic::GetEffectiveBindingMode() const
{
    return FVRExpUIInfoPlacementResolver::GetEffectiveBindingMode(
        CurrentPresentationSettings);
}

bool UVRExpDetectableUIInfoTriggerLogic::IsExclusivePresentation() const
{
    return CurrentPresentationSettings.PresentationPolicy ==
           EVRExpUIInfoPresentationPolicy::ExclusivePerLocalPlayer;
}

int32 UVRExpDetectableUIInfoTriggerLogic::GetExclusivePriority() const
{
    if (CurrentInteractionSource ==
        EVRExpUIInfoInteractionSource::GracePeriod)
    {
        return GracePeriodPriority;
    }
    return CurrentInteractionSource == EVRExpUIInfoInteractionSource::None
               ? 0
               : CurrentPresentationSettings.Priority;
}

void UVRExpDetectableUIInfoTriggerLogic::ResetResolvedOnceTarget()
{
    bHasResolvedOnceTarget = false;
    CachedResolvedOnceBaseTransform = FTransform::Identity;
    CachedResolvedOnceAnchor = FResolvedAnchor();
    RuntimeDebugState.bAnchorResolved = false;
    RuntimeDebugState.bTargetResolved = false;
    RuntimeDebugState.AnchorActor = nullptr;
    RuntimeDebugState.AnchorComponent = nullptr;
    RuntimeDebugState.AnchorSocketName = NAME_None;
    RuntimeDebugState.AnchorWorldTransform = FTransform::Identity;
    RuntimeDebugState.TargetWorldTransform = FTransform::Identity;
}

bool UVRExpDetectableUIInfoTriggerLogic::ResolvePresentationTarget(
    FTransform &OutTargetWorldTransform,
    FResolvedAnchor &OutAnchor,
    FString &OutFailureReason)
{
    const EVRExpUIInfoBindingMode EffectiveBindingMode =
        GetEffectiveBindingMode();

    FVRExpUIInfoPlacementRequest PlacementRequest;
    PlacementRequest.Settings = &CurrentPresentationSettings;
    PlacementRequest.DetectableOwner =
        DetectableComponent.IsValid()
            ? DetectableComponent->GetOwner()
            : nullptr;
    PlacementRequest.MotionUpdatedComponent =
        DetectableComponent.IsValid()
            ? DetectableComponent->GetResolvedMotionUpdatedComponent()
            : nullptr;
    PlacementRequest.CameraComponent = ResolveLocalCameraComponent();
    PlacementRequest.bHasViewTransform =
        ResolveLocalViewTransform(PlacementRequest.ViewTransform);
    PlacementRequest.bHasFallbackFacingRotation =
        bHasLastValidFacingRotation;
    PlacementRequest.FallbackFacingRotation =
        LastValidFacingRotation;

    FTransform BaseTargetWorldTransform = FTransform::Identity;
    if (EffectiveBindingMode == EVRExpUIInfoBindingMode::ResolveOnce &&
        bHasResolvedOnceTarget)
    {
        BaseTargetWorldTransform = CachedResolvedOnceBaseTransform;
        OutAnchor = CachedResolvedOnceAnchor;
    }
    else
    {
        FVRExpUIInfoPlacementResult BaseResult;
        if (!FVRExpUIInfoPlacementResolver::ResolveBaseTarget(
                PlacementRequest,
                BaseResult,
                OutFailureReason))
        {
            RuntimeDebugState.bAnchorResolved = false;
            RuntimeDebugState.bTargetResolved = false;
            RuntimeDebugState.AnchorActor.Reset();
            RuntimeDebugState.AnchorComponent.Reset();
            RuntimeDebugState.AnchorSocketName = NAME_None;
            RuntimeDebugState.AnchorWorldTransform = FTransform::Identity;
            return false;
        }
        OutAnchor = BaseResult.Anchor;
        BaseTargetWorldTransform = BaseResult.TargetWorldTransform;

        RuntimeDebugState.bAnchorResolved = true;
        RuntimeDebugState.AnchorActor = OutAnchor.Actor;
        RuntimeDebugState.AnchorComponent = OutAnchor.Component;
        RuntimeDebugState.AnchorSocketName = OutAnchor.SocketName;
        RuntimeDebugState.AnchorWorldTransform =
            OutAnchor.WorldTransform;

        if (EffectiveBindingMode == EVRExpUIInfoBindingMode::ResolveOnce)
        {
            CachedResolvedOnceBaseTransform = BaseTargetWorldTransform;
            CachedResolvedOnceAnchor = OutAnchor;
            bHasResolvedOnceTarget = true;
        }
    }

    RuntimeDebugState.bAnchorResolved = true;
    RuntimeDebugState.AnchorActor = OutAnchor.Actor;
    RuntimeDebugState.AnchorComponent = OutAnchor.Component;
    RuntimeDebugState.AnchorSocketName = OutAnchor.SocketName;
    RuntimeDebugState.AnchorWorldTransform = OutAnchor.WorldTransform;

    OutTargetWorldTransform = BaseTargetWorldTransform;
    FVRExpUIInfoPlacementResult OrientationResult;
    OrientationResult.Anchor = OutAnchor;
    OrientationResult.TargetWorldTransform =
        OutTargetWorldTransform;
    if (!FVRExpUIInfoPlacementResolver::ApplyOrientation(
            PlacementRequest,
            OutAnchor,
            OutTargetWorldTransform,
            OrientationResult,
            OutFailureReason))
    {
        RuntimeDebugState.bTargetResolved = false;
        return false;
    }
    if (OrientationResult.bHasFacingRotation)
    {
        LastValidFacingRotation =
            OrientationResult.FacingRotation;
        bHasLastValidFacingRotation = true;
    }
    if (OrientationResult.bUsedDefaultFacingAxes &&
        !bHasWarnedInvalidFacingAxes)
    {
        bHasWarnedInvalidFacingAxes = true;
        UE_LOG(
            LogVRExpDetectableUIInfoTriggerLogic,
            Warning,
            TEXT("%s has parallel LocalFacingAxis and LocalUpAxis; +X/+Z is used."),
            *GetPathName());
    }

    RuntimeDebugState.bTargetResolved = true;
    RuntimeDebugState.TargetWorldTransform = OutTargetWorldTransform;
    return true;
}

bool UVRExpDetectableUIInfoTriggerLogic::ResolveAnchor(
    FResolvedAnchor &OutAnchor,
    FString &OutFailureReason) const
{
    AActor *DetectableOwner =
        DetectableComponent.IsValid() ? DetectableComponent->GetOwner() : nullptr;

    switch (CurrentPresentationSettings.AnchorType)
    {
    case EVRExpUIInfoAnchorType::Camera:
    {
        if (UCameraComponent *CameraComponent = ResolveLocalCameraComponent())
        {
            OutAnchor.Component = CameraComponent;
            OutAnchor.Actor = CameraComponent->GetOwner();
            OutAnchor.WorldTransform = CameraComponent->GetComponentTransform();
            return true;
        }

        if (GetEffectiveBindingMode() == EVRExpUIInfoBindingMode::Attach)
        {
            OutFailureReason =
                TEXT("Attach binding requires a valid local camera component.");
            return false;
        }

        if (!ResolveLocalViewTransform(OutAnchor.WorldTransform))
        {
            OutFailureReason = TEXT("The local player view is unavailable.");
            return false;
        }

        OutAnchor.bUsedPlayerViewPoint = true;
        if (APlayerController *PlayerController = LocalPlayerController.Get())
        {
            OutAnchor.Actor = PlayerController->GetPawn();
        }
        return true;
    }

    case EVRExpUIInfoAnchorType::DetectableOwner:
        if (!IsValid(DetectableOwner))
        {
            OutFailureReason = TEXT("The detectable owner is unavailable.");
            return false;
        }
        OutAnchor.Actor = DetectableOwner;
        OutAnchor.Component = DetectableOwner->GetRootComponent();
        OutAnchor.WorldTransform = DetectableOwner->GetActorTransform();
        if (GetEffectiveBindingMode() == EVRExpUIInfoBindingMode::Attach &&
            !OutAnchor.Component.IsValid())
        {
            OutFailureReason =
                TEXT("Attach binding requires the detectable owner to have a root component.");
            return false;
        }
        return true;

    case EVRExpUIInfoAnchorType::MotionUpdatedComponent:
    {
        if (!DetectableComponent.IsValid())
        {
            OutFailureReason =
                TEXT("The detectable component is unavailable.");
            return false;
        }

        USceneComponent *MotionUpdatedComponent =
            DetectableComponent->GetResolvedMotionUpdatedComponent();
        if (!IsValid(MotionUpdatedComponent))
        {
            OutFailureReason =
                TEXT("The authoritative Grabbable Motion Component or its UpdatedComponent is unavailable.");
            return false;
        }

        OutAnchor.Actor = MotionUpdatedComponent->GetOwner();
        OutAnchor.Component = MotionUpdatedComponent;
        OutAnchor.WorldTransform =
            MotionUpdatedComponent->GetComponentTransform();
        return true;
    }

    case EVRExpUIInfoAnchorType::Component:
        return ResolveComponentAnchor(
            CurrentPresentationSettings.AttachComponent,
            CurrentPresentationSettings.AttachSocketName,
            OutAnchor,
            OutFailureReason);

    case EVRExpUIInfoAnchorType::World:
        OutAnchor.WorldTransform = CurrentPresentationSettings.WorldTransform;
        return true;

    default:
        OutFailureReason = TEXT("AnchorType is invalid.");
        return false;
    }
}

bool UVRExpDetectableUIInfoTriggerLogic::ResolveComponentAnchor(
    const FComponentReference &ComponentReference,
    FName SocketName,
    FResolvedAnchor &OutAnchor,
    FString &OutFailureReason) const
{
    AActor *DetectableOwner =
        DetectableComponent.IsValid() ? DetectableComponent->GetOwner() : nullptr;
    return FVRExpUIInfoPlacementResolver::ResolveComponentAnchor(
        DetectableOwner,
        ComponentReference,
        SocketName,
        OutAnchor,
        OutFailureReason);
}

bool UVRExpDetectableUIInfoTriggerLogic::ApplyPositionMode(
    FTransform &InOutTargetWorldTransform,
    const FResolvedAnchor &Anchor,
    FString &OutFailureReason) const
{
    if (CurrentPresentationSettings.PositionMode ==
        EVRExpUIInfoPositionMode::AnchorRelative)
    {
        return true;
    }

    if (CurrentPresentationSettings.PositionMode !=
        EVRExpUIInfoPositionMode::BetweenAnchorAndCamera)
    {
        OutFailureReason = TEXT("PositionMode is invalid.");
        return false;
    }

    if (CurrentPresentationSettings.AnchorType ==
        EVRExpUIInfoAnchorType::Camera)
    {
        OutFailureReason =
            TEXT("BetweenAnchorAndCamera requires a non-Camera anchor.");
        return false;
    }

    FTransform ViewTransform;
    if (!ResolveLocalViewTransform(ViewTransform))
    {
        OutFailureReason =
            TEXT("BetweenAnchorAndCamera requires a valid local player view.");
        return false;
    }

    const FVector AnchorPoint =
        Anchor.WorldTransform.GetLocation();
    const FVector CameraPoint = ViewTransform.GetLocation();
    const float SegmentLength =
        FVector::Distance(AnchorPoint, CameraPoint);
    const float MinAnchorDistance =
        FMath::Max(0.0f, CurrentPresentationSettings.MinDistanceFromAnchor);
    const float MinCameraDistance =
        FMath::Max(0.0f, CurrentPresentationSettings.MinDistanceFromCamera);

    if (SegmentLength <= KINDA_SMALL_NUMBER ||
        SegmentLength + KINDA_SMALL_NUMBER <
            MinAnchorDistance + MinCameraDistance)
    {
        OutFailureReason = FString::Printf(
            TEXT("Anchor-camera distance %.2f cm cannot satisfy minimum anchor/camera clearances %.2f/%.2f cm."),
            SegmentLength,
            MinAnchorDistance,
            MinCameraDistance);
        return false;
    }

    const float MinimumRatio =
        MinAnchorDistance / SegmentLength;
    const float MaximumRatio =
        1.0f - MinCameraDistance / SegmentLength;
    const float SafeRatio = FMath::Clamp(
        CurrentPresentationSettings.BetweenRatio,
        MinimumRatio,
        MaximumRatio);

    FVector PositionUp;
    if (!ResolvePositionUpVector(
            Anchor,
            PositionUp,
            OutFailureReason))
    {
        return false;
    }

    InOutTargetWorldTransform.SetLocation(
        FMath::Lerp(AnchorPoint, CameraPoint, SafeRatio) +
        PositionUp.GetSafeNormal(KINDA_SMALL_NUMBER, FVector::UpVector) *
            CurrentPresentationSettings.BetweenVerticalOffset);
    return true;
}

bool UVRExpDetectableUIInfoTriggerLogic::ResolvePositionUpVector(
    const FResolvedAnchor &Anchor,
    FVector &OutUpVector,
    FString &OutFailureReason) const
{
    switch (CurrentPresentationSettings.PositionUpReference)
    {
    case EVRExpUIInfoUpReference::WorldUp:
        OutUpVector = FVector::UpVector;
        return true;

    case EVRExpUIInfoUpReference::CameraUp:
    {
        FTransform ViewTransform;
        if (!ResolveLocalViewTransform(ViewTransform))
        {
            OutFailureReason =
                TEXT("Camera Up position reference requires a valid local player view.");
            return false;
        }
        OutUpVector = ViewTransform.GetUnitAxis(EAxis::Z);
        return true;
    }

    case EVRExpUIInfoUpReference::AnchorUp:
        OutUpVector =
            Anchor.WorldTransform.GetUnitAxis(EAxis::Z);
        return true;

    default:
        OutFailureReason = TEXT("PositionUpReference is invalid.");
        return false;
    }
}

bool UVRExpDetectableUIInfoTriggerLogic::ApplyOrientation(
    FTransform &InOutTargetWorldTransform,
    const FResolvedAnchor &Anchor,
    FString &OutFailureReason)
{
    switch (CurrentPresentationSettings.OrientationMode)
    {
    case EVRExpUIInfoOrientationMode::InheritAnchor:
        return true;

    case EVRExpUIInfoOrientationMode::FixedWorld:
        InOutTargetWorldTransform.SetRotation(
            CurrentPresentationSettings.FixedWorldRotation.Quaternion());
        return true;

    case EVRExpUIInfoOrientationMode::FaceCamera:
    case EVRExpUIInfoOrientationMode::FaceCameraYawOnly:
    {
        FQuat FacingRotation = InOutTargetWorldTransform.GetRotation();
        const bool bPlanar =
            CurrentPresentationSettings.OrientationMode ==
            EVRExpUIInfoOrientationMode::FaceCameraYawOnly;
        if (!BuildCameraFacingRotation(
                InOutTargetWorldTransform.GetLocation(),
                Anchor,
                bPlanar,
                FacingRotation,
                OutFailureReason))
        {
            return false;
        }
        InOutTargetWorldTransform.SetRotation(FacingRotation);
        return true;
    }

    default:
        OutFailureReason = TEXT("OrientationMode is invalid.");
        return false;
    }
}

bool UVRExpDetectableUIInfoTriggerLogic::ResolveFacingUpVector(
    const FResolvedAnchor &Anchor,
    FVector &OutUpVector,
    FString &OutFailureReason) const
{
    switch (CurrentPresentationSettings.UpReference)
    {
    case EVRExpUIInfoUpReference::WorldUp:
        OutUpVector = FVector::UpVector;
        return true;

    case EVRExpUIInfoUpReference::CameraUp:
    {
        FTransform ViewTransform;
        if (!ResolveLocalViewTransform(ViewTransform))
        {
            OutFailureReason =
                TEXT("Camera Up requires a valid local player view.");
            return false;
        }
        OutUpVector = ViewTransform.GetUnitAxis(EAxis::Z);
        return true;
    }

    case EVRExpUIInfoUpReference::AnchorUp:
        OutUpVector = Anchor.WorldTransform.GetUnitAxis(EAxis::Z);
        return true;

    default:
        OutFailureReason = TEXT("UpReference is invalid.");
        return false;
    }
}

bool UVRExpDetectableUIInfoTriggerLogic::BuildCameraFacingRotation(
    const FVector &TargetLocation,
    const FResolvedAnchor &Anchor,
    bool bPlanar,
    FQuat &OutRotation,
    FString &OutFailureReason)
{
    FTransform ViewTransform;
    if (!ResolveLocalViewTransform(ViewTransform))
    {
        OutFailureReason =
            TEXT("Camera-facing orientation requires a valid local player view.");
        return false;
    }

    FVector DesiredUp;
    if (!ResolveFacingUpVector(Anchor, DesiredUp, OutFailureReason))
    {
        return false;
    }
    DesiredUp = DesiredUp.GetSafeNormal();
    if (DesiredUp.IsNearlyZero())
    {
        DesiredUp = FVector::UpVector;
    }

    FVector DesiredFacing = ViewTransform.GetLocation() - TargetLocation;
    if (bPlanar)
    {
        DesiredFacing = FVector::VectorPlaneProject(DesiredFacing, DesiredUp);
    }
    DesiredFacing = DesiredFacing.GetSafeNormal();

    if (DesiredFacing.IsNearlyZero())
    {
        OutFailureReason =
            TEXT("Camera-facing direction is degenerate; the last valid rotation is retained.");
        if (bHasLastValidFacingRotation)
        {
            OutRotation = LastValidFacingRotation;
        }
        return true;
    }

    FVector LocalFacing =
        GetAxisVector(CurrentPresentationSettings.LocalFacingAxis);
    FVector LocalUp =
        GetAxisVector(CurrentPresentationSettings.LocalUpAxis);
    if (FMath::Abs(FVector::DotProduct(LocalFacing, LocalUp)) >
        1.0f - KINDA_SMALL_NUMBER)
    {
        LocalFacing = FVector::ForwardVector;
        LocalUp = FVector::UpVector;
        if (!bHasWarnedInvalidFacingAxes)
        {
            bHasWarnedInvalidFacingAxes = true;
            UE_LOG(LogVRExpDetectableUIInfoTriggerLogic, Warning,
                   TEXT("%s has parallel LocalFacingAxis and LocalUpAxis; +X/+Z is used."),
                   *GetPathName());
        }
    }

    FVector DesiredProjectedUp =
        FVector::VectorPlaneProject(DesiredUp, DesiredFacing).GetSafeNormal();
    if (DesiredProjectedUp.IsNearlyZero())
    {
        DesiredProjectedUp =
            FVector::VectorPlaneProject(FVector::UpVector, DesiredFacing)
                .GetSafeNormal();
    }
    if (DesiredProjectedUp.IsNearlyZero())
    {
        DesiredProjectedUp =
            FVector::VectorPlaneProject(
                ViewTransform.GetUnitAxis(EAxis::Y), DesiredFacing)
                .GetSafeNormal();
    }
    if (DesiredProjectedUp.IsNearlyZero())
    {
        OutFailureReason =
            TEXT("A stable camera-facing up direction could not be constructed.");
        return false;
    }

    const FQuat FacingAlignment =
        FQuat::FindBetweenNormals(LocalFacing, DesiredFacing);
    const FVector AlignedLocalUp =
        FVector::VectorPlaneProject(
            FacingAlignment.RotateVector(LocalUp), DesiredFacing)
            .GetSafeNormal();
    if (AlignedLocalUp.IsNearlyZero())
    {
        OutFailureReason =
            TEXT("The configured local UI axes cannot construct a valid facing basis.");
        return false;
    }

    const float TwistAngle = FMath::Atan2(
        FVector::DotProduct(
            DesiredFacing,
            FVector::CrossProduct(AlignedLocalUp, DesiredProjectedUp)),
        FVector::DotProduct(AlignedLocalUp, DesiredProjectedUp));
    const FQuat TwistRotation(DesiredFacing, TwistAngle);
    const FQuat FacingRotation =
        (TwistRotation * FacingAlignment).GetNormalized();
    OutRotation =
        (FacingRotation *
         CurrentPresentationSettings.FacingRotationOffset.Quaternion())
            .GetNormalized();

    LastValidFacingRotation = OutRotation;
    bHasLastValidFacingRotation = true;
    return true;
}

FVector UVRExpDetectableUIInfoTriggerLogic::GetAxisVector(
    EVRExpUIInfoAxisDirection Axis)
{
    switch (Axis)
    {
    case EVRExpUIInfoAxisDirection::PositiveX:
        return FVector::ForwardVector;
    case EVRExpUIInfoAxisDirection::NegativeX:
        return -FVector::ForwardVector;
    case EVRExpUIInfoAxisDirection::PositiveY:
        return FVector::RightVector;
    case EVRExpUIInfoAxisDirection::NegativeY:
        return -FVector::RightVector;
    case EVRExpUIInfoAxisDirection::PositiveZ:
        return FVector::UpVector;
    case EVRExpUIInfoAxisDirection::NegativeZ:
        return -FVector::UpVector;
    default:
        return FVector::ForwardVector;
    }
}

void UVRExpDetectableUIInfoTriggerLogic::ConfigureAttachment(
    const FResolvedAnchor &Anchor,
    EVRExpUIInfoBindingMode BindingMode)
{
    AActor *UIActor = SpawnedUIActor.Get();
    USceneComponent *RootComponent =
        IsValid(UIActor) ? UIActor->GetRootComponent() : nullptr;
    if (!IsValid(UIActor) || !IsValid(RootComponent))
    {
        return;
    }

    if (BindingMode == EVRExpUIInfoBindingMode::Attach &&
        Anchor.Component.IsValid())
    {
        USceneComponent *AttachParent = Anchor.Component.Get();
        const FTransform PreservedWorldTransform =
            RootComponent->GetComponentTransform();
        if (RootComponent->GetAttachParent() != AttachParent ||
            RootComponent->GetAttachSocketName() != Anchor.SocketName)
        {
            UIActor->AttachToComponent(
                AttachParent,
                FAttachmentTransformRules::KeepWorldTransform,
                Anchor.SocketName);
        }

        const bool bAbsoluteRotation =
            CurrentPresentationSettings.OrientationMode !=
            EVRExpUIInfoOrientationMode::InheritAnchor;
        const bool bAbsoluteScale =
            CurrentPresentationSettings.ScaleMode ==
            EVRExpUIInfoScaleMode::KeepWorldScale;
        RootComponent->SetAbsolute(false, bAbsoluteRotation, bAbsoluteScale);
        RootComponent->SetWorldTransform(PreservedWorldTransform);
        return;
    }

    if (RootComponent->GetAttachParent())
    {
        UIActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    }
    RootComponent->SetAbsolute(false, false, false);
}

void UVRExpDetectableUIInfoTriggerLogic::ApplySmoothedWorldTransform(
    const FTransform &TargetWorldTransform,
    float DeltaTime)
{
    AActor *UIActor = SpawnedUIActor.Get();
    USceneComponent *RootComponent =
        IsValid(UIActor) ? UIActor->GetRootComponent() : nullptr;
    if (!IsValid(UIActor) || !IsValid(RootComponent))
    {
        bTransformSettled = true;
        return;
    }

    const FTransform CurrentTransform = RootComponent->GetComponentTransform();

    const FVector NewLocation =
        CurrentPresentationSettings.bSmoothLocation &&
                CurrentPresentationSettings.LocationInterpSpeed > 0.0f
            ? FMath::VInterpTo(
                  CurrentTransform.GetLocation(),
                  TargetWorldTransform.GetLocation(),
                  DeltaTime,
                  CurrentPresentationSettings.LocationInterpSpeed)
            : TargetWorldTransform.GetLocation();

    const FQuat NewRotation =
        CurrentPresentationSettings.bSmoothRotation &&
                CurrentPresentationSettings.RotationInterpSpeed > 0.0f
            ? FMath::QInterpTo(
                  CurrentTransform.GetRotation(),
                  TargetWorldTransform.GetRotation(),
                  DeltaTime,
                  CurrentPresentationSettings.RotationInterpSpeed)
            : TargetWorldTransform.GetRotation();

    const FVector NewScale =
        CurrentPresentationSettings.bSmoothScale &&
                CurrentPresentationSettings.ScaleInterpSpeed > 0.0f
            ? FMath::VInterpTo(
                  CurrentTransform.GetScale3D(),
                  TargetWorldTransform.GetScale3D(),
                  DeltaTime,
                  CurrentPresentationSettings.ScaleInterpSpeed)
            : TargetWorldTransform.GetScale3D();

    RootComponent->SetWorldLocation(NewLocation);
    RootComponent->SetWorldRotation(NewRotation.GetNormalized());
    RootComponent->SetWorldScale3D(NewScale);

    const FTransform AppliedTransform(
        NewRotation.GetNormalized(), NewLocation, NewScale);
    bTransformSettled =
        IsTransformSettled(AppliedTransform, TargetWorldTransform);
}

bool UVRExpDetectableUIInfoTriggerLogic::IsTransformSettled(
    const FTransform &CurrentTransform,
    const FTransform &TargetTransform) const
{
    const bool bLocationSettled =
        FVector::DistSquared(
            CurrentTransform.GetLocation(), TargetTransform.GetLocation()) <=
        FMath::Square(0.1f);
    const bool bRotationSettled =
        CurrentTransform.GetRotation()
            .GetNormalized()
            .AngularDistance(
                TargetTransform.GetRotation().GetNormalized()) <=
        FMath::DegreesToRadians(0.1);
    const bool bScaleSettled =
        CurrentTransform.GetScale3D().Equals(
            TargetTransform.GetScale3D(), 0.001f);
    return bLocationSettled && bRotationSettled && bScaleSettled;
}

UCameraComponent *
UVRExpDetectableUIInfoTriggerLogic::ResolveLocalCameraComponent() const
{
    APlayerController *PlayerController = LocalPlayerController.Get();
    APawn *LocalPawn =
        IsValid(PlayerController) ? PlayerController->GetPawn() : nullptr;
    if (!IsValid(LocalPawn))
    {
        return nullptr;
    }

    if (UReplicatedVRCameraComponent *VRCamera =
            LocalPawn->FindComponentByClass<UReplicatedVRCameraComponent>())
    {
        return VRCamera;
    }

    return LocalPawn->FindComponentByClass<UCameraComponent>();
}

bool UVRExpDetectableUIInfoTriggerLogic::ResolveLocalViewTransform(
    FTransform &OutViewTransform) const
{
    if (UCameraComponent *CameraComponent = ResolveLocalCameraComponent())
    {
        OutViewTransform = CameraComponent->GetComponentTransform();
        return true;
    }

    APlayerController *PlayerController = LocalPlayerController.Get();
    if (!IsValid(PlayerController))
    {
        return false;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
    OutViewTransform =
        FTransform(ViewRotation, ViewLocation, FVector::OneVector);
    return true;
}

APlayerController *
UVRExpDetectableUIInfoTriggerLogic::ResolveLocalPlayerController(
    const FVRExpDetectableInteractionContext &Context,
    EVRExpUIInfoInteractionSource InteractionSource) const
{
    const auto ResolveMotionLocalPlayer =
        [this]() -> APlayerController *
    {
        UWorld *World = GetOwningWorld();
        UGameInstance *GameInstance =
            IsValid(World) ? World->GetGameInstance() : nullptr;
        if (!IsValid(GameInstance))
        {
            return nullptr;
        }

        const TArray<ULocalPlayer *> &LocalPlayers =
            GameInstance->GetLocalPlayers();
        if (!LocalPlayers.IsValidIndex(MotionLocalPlayerIndex) ||
            !IsValid(LocalPlayers[MotionLocalPlayerIndex]))
        {
            return nullptr;
        }

        APlayerController *PlayerController =
            LocalPlayers[MotionLocalPlayerIndex]
                ->GetPlayerController(World);
        return IsValid(PlayerController) &&
                       PlayerController->IsLocalController()
                   ? PlayerController
                   : nullptr;
    };

    const auto ResolveControllerOwner =
        [](const UGripMotionControllerComponent *GripController)
            -> APlayerController *
    {
        const APawn *SourcePawn =
            IsValid(GripController)
                ? Cast<APawn>(GripController->GetOwner())
                : nullptr;
        if (!IsValid(SourcePawn) || !SourcePawn->IsLocallyControlled())
        {
            return nullptr;
        }

        return Cast<APlayerController>(SourcePawn->GetController());
    };

    if (InteractionSource == EVRExpUIInfoInteractionSource::Motion)
    {
        return ResolveMotionLocalPlayer();
    }

    if (InteractionSource == EVRExpUIInfoInteractionSource::Grip)
    {
        for (int32 Index = Context.ActiveGrips.Num() - 1;
             Index >= 0;
             --Index)
        {
            if (APlayerController *PlayerController =
                    ResolveControllerOwner(
                        Context.ActiveGrips[Index].GripController.Get()))
            {
                return PlayerController;
            }
        }

        return nullptr;
    }

    if (InteractionSource == EVRExpUIInfoInteractionSource::Release)
    {
        if (APlayerController *CachedController =
                LocalPlayerController.Get();
            IsValid(CachedController) &&
            CachedController->IsLocalController())
        {
            return CachedController;
        }

        if (APlayerController *ReleasedController =
                LastReleasedLocalPlayerController.Get();
            IsValid(ReleasedController) &&
            ReleasedController->IsLocalController())
        {
            return ReleasedController;
        }

        if (APlayerController *ReleasedGripController =
                ResolveControllerOwner(Context.GripController.Get()))
        {
            return ReleasedGripController;
        }

        if (IsValid(Context.HeadDetector.Get()))
        {
            if (APawn *HeadPawn =
                    Cast<APawn>(Context.HeadDetector->GetOwner()))
            {
                if (HeadPawn->IsLocallyControlled())
                {
                    return Cast<APlayerController>(
                        HeadPawn->GetController());
                }
            }
        }

        return ResolveMotionLocalPlayer();
    }

    AActor *SourceOwner = nullptr;
    if (InteractionSource == EVRExpUIInfoInteractionSource::HeadDetection &&
        IsValid(Context.HeadDetector.Get()))
    {
        SourceOwner = Context.HeadDetector->GetOwner();
    }

    if (APawn *SourcePawn = Cast<APawn>(SourceOwner))
    {
        return SourcePawn->IsLocallyControlled()
                   ? Cast<APlayerController>(SourcePawn->GetController())
                   : nullptr;
    }

    if (UWorld *World = GetOwningWorld())
    {
        for (FConstPlayerControllerIterator Iterator =
                 World->GetPlayerControllerIterator();
             Iterator;
             ++Iterator)
        {
            APlayerController *Candidate = Iterator->Get();
            if (IsValid(Candidate) && Candidate->IsLocalController())
            {
                return Candidate;
            }
        }
    }

    return nullptr;
}

void UVRExpDetectableUIInfoTriggerLogic::UpdateExclusiveClaim(
    bool bEligible)
{
    if (!SpawnedUIActor.IsValid() || !IsExclusivePresentation())
    {
        RemoveExclusiveClaim();
        return;
    }

    if (!LocalPlayerController.IsValid())
    {
        LocalPlayerController =
            ResolveLocalPlayerController(LastContext, CurrentInteractionSource);
    }

    UVRExpUIInfoPresentationSubsystem *Subsystem =
        GetPresentationSubsystem();
    if (!Subsystem || !LocalPlayerController.IsValid())
    {
        RemoveExclusiveClaim();
        SetUIActorVisible(
            false,
            EVRExpUIInfoVisibilityReason::PlacementTargetUnavailable);
        return;
    }

    Subsystem->UpdateExclusiveClaim(
        this,
        LocalPlayerController.Get(),
        GetExclusivePriority(),
        GetActivationSequenceForSource(CurrentInteractionSource),
        bEligible);
}

void UVRExpDetectableUIInfoTriggerLogic::RemoveExclusiveClaim()
{
    if (UVRExpUIInfoPresentationSubsystem *Subsystem =
            GetPresentationSubsystem())
    {
        Subsystem->RemoveExclusiveClaim(this);
    }
    bExclusivePresentationGranted = false;
}

void UVRExpDetectableUIInfoTriggerLogic::HandleExclusivePresentationGranted(
    bool bGranted)
{
    bExclusivePresentationGranted = bGranted;

    if (!IsExclusivePresentation())
    {
        return;
    }

    if (bGranted)
    {
        const bool bCanDisplay =
            bPresentationTargetAvailable ||
            (CurrentPresentationSettings.MissingAnchorPolicy ==
                 EVRExpUIInfoMissingAnchorPolicy::KeepLastAndRetry &&
             bHasValidPresentationTransform);
        if (bCanDisplay)
        {
            const EVRExpUIInfoVisibilityReason VisibilityReason =
                bWasPriorityDisplaced
                    ? EVRExpUIInfoVisibilityReason::RestoredAfterPriorityLoss
                    : EVRExpUIInfoVisibilityReason::Activated;
            SetUIActorVisible(true, VisibilityReason);
            bWasPriorityDisplaced = false;
        }
    }
    else
    {
        bWasPriorityDisplaced = true;
        SetUIActorVisible(
            false,
            EVRExpUIInfoVisibilityReason::PriorityDisplaced);
    }

    UpdateRuntimeDebugState();
    RefreshTickRegistration();
}

void UVRExpDetectableUIInfoTriggerLogic::HandleExclusiveClaimMadeIneligible()
{
    SetSourceClaimEligible(CurrentInteractionSource, false);
    UpdateRuntimeDebugState();
    RefreshTickRegistration();
}

bool UVRExpDetectableUIInfoTriggerLogic::ShouldRestoreAfterPriorityLoss() const
{
    return bRestoreAfterPriorityLoss;
}

void UVRExpDetectableUIInfoTriggerLogic::TickPresentation(float DeltaTime)
{
    if (bHasSpawnedUIActor && !SpawnedUIActor.IsValid())
    {
        HandleExternalUIActorDestruction(nullptr);
    }

    if (bExitTransitionActive)
    {
        TickExitTransition(DeltaTime);
        UpdateRuntimeDebugState();
        DrawRuntimeDebug();
        RefreshTickRegistration();
        return;
    }

    if (CurrentInteractionSource ==
            EVRExpUIInfoInteractionSource::Release &&
        ReleaseBehavior ==
            EVRExpUIInfoReleaseBehavior::HideDuringRelease)
    {
        RemoveExclusiveClaim();
        if (SpawnedUIActor.IsValid())
        {
            SetUIActorVisible(
                false,
                EVRExpUIInfoVisibilityReason::HiddenDuringRelease);
        }
        UpdateRuntimeDebugState();
        DrawRuntimeDebug();
        RefreshTickRegistration();
        return;
    }

    if (bHasLocalInteraction && !LocalPlayerController.IsValid())
    {
        LocalPlayerController =
            ResolveLocalPlayerController(
                LastContext,
                CurrentInteractionSource);
        if (!LocalPlayerController.IsValid())
        {
            if (CurrentInteractionSource !=
                    EVRExpUIInfoInteractionSource::Motion &&
                CurrentInteractionSource !=
                    EVRExpUIInfoInteractionSource::Release)
            {
                BeginGracePeriod(LastContext, false);
                return;
            }

            bPresentationTargetAvailable = false;
            bAwaitingPresentationTarget = true;
            LastPlacementFailureReason =
                TEXT("No matching local player was found.");
            RemoveExclusiveClaim();
            if (SpawnedUIActor.IsValid())
            {
                SetUIActorVisible(
                    false,
                    EVRExpUIInfoVisibilityReason::PlacementTargetUnavailable);
            }
            UpdateRuntimeDebugState();
            DrawRuntimeDebug();
            RefreshTickRegistration();
            return;
        }
    }

    const bool bHadUIActor = SpawnedUIActor.IsValid();
    if (!bHadUIActor && bHasLocalInteraction)
    {
        EnsureUIActor(LastContext);
    }
    if (!bHadUIActor && SpawnedUIActor.IsValid())
    {
        BroadcastPresentationChanged();
    }

    AActor *UIActor = SpawnedUIActor.Get();
    if (!IsValid(UIActor))
    {
        UpdateRuntimeDebugState();
        DrawRuntimeDebug();
        RefreshTickRegistration();
        return;
    }

    USceneComponent *RootComponent = UIActor->GetRootComponent();
    if (!IsValid(RootComponent))
    {
        bPresentationTargetAvailable = false;
        bAwaitingPresentationTarget = bHasLocalInteraction;
        LastPlacementFailureReason =
            TEXT("The UI Actor has no root component.");
        RemoveExclusiveClaim();
        SetUIActorVisible(
            false,
            EVRExpUIInfoVisibilityReason::PlacementTargetUnavailable);
        UpdateRuntimeDebugState();
        DrawRuntimeDebug();
        RefreshTickRegistration();
        return;
    }

    FTransform TargetWorldTransform;
    FResolvedAnchor Anchor;
    FString ResolveFailureReason;
    if (!ResolvePresentationTarget(
            TargetWorldTransform, Anchor, ResolveFailureReason))
    {
        bPresentationTargetAvailable = false;
        bAwaitingPresentationTarget =
            bHasLocalInteraction ||
            (CurrentInteractionSource ==
                 EVRExpUIInfoInteractionSource::GracePeriod &&
             bUIActorVisible);
        LastPlacementFailureReason =
            ResolveFailureReason.IsEmpty()
                ? TEXT("The presentation target could not be resolved.")
                : ResolveFailureReason;

        if (CurrentPresentationSettings.MissingAnchorPolicy ==
            EVRExpUIInfoMissingAnchorPolicy::HideAndRetry)
        {
            RemoveExclusiveClaim();
            SetUIActorVisible(
                false,
                EVRExpUIInfoVisibilityReason::PlacementTargetUnavailable);
        }
        else if (IsExclusivePresentation() &&
                 bHasValidPresentationTransform)
        {
            UpdateExclusiveClaim(
                IsSourceClaimEligible(CurrentInteractionSource));
        }
        else if (!IsExclusivePresentation() &&
                 bHasValidPresentationTransform)
        {
            RemoveExclusiveClaim();
            SetUIActorVisible(
                true, EVRExpUIInfoVisibilityReason::Activated);
        }
        else
        {
            RemoveExclusiveClaim();
        }

        UpdateRuntimeDebugState();
        DrawRuntimeDebug();
        RefreshTickRegistration();
        return;
    }

    bPresentationTargetAvailable = true;
    bHasValidPresentationTransform = true;
    bAwaitingPresentationTarget = false;
    LastPlacementFailureReason = ResolveFailureReason;

    const EVRExpUIInfoBindingMode EffectiveBindingMode =
        GetEffectiveBindingMode();
    ConfigureAttachment(Anchor, EffectiveBindingMode);
    ApplySmoothedWorldTransform(TargetWorldTransform, DeltaTime);

    if (IsExclusivePresentation())
    {
        UpdateExclusiveClaim(
            IsSourceClaimEligible(CurrentInteractionSource));
    }
    else
    {
        RemoveExclusiveClaim();
        SetUIActorVisible(
            true, EVRExpUIInfoVisibilityReason::Activated);
    }

    UpdateRuntimeDebugState();
    DrawRuntimeDebug();
    RefreshTickRegistration();
}

bool UVRExpDetectableUIInfoTriggerLogic::RequiresPresentationTick() const
{
    if (bExitTransitionActive)
    {
        return true;
    }

    if (bDrawRuntimeDebug)
    {
        return true;
    }

    if (CurrentInteractionSource ==
            EVRExpUIInfoInteractionSource::Release &&
        ReleaseBehavior ==
            EVRExpUIInfoReleaseBehavior::HideDuringRelease)
    {
        return false;
    }

    if (bAwaitingPresentationTarget)
    {
        return true;
    }

    if (!SpawnedUIActor.IsValid())
    {
        return bHasLocalInteraction && UIActorClass.Get() != nullptr;
    }

    if (!bUIActorVisible &&
        CurrentInteractionSource ==
            EVRExpUIInfoInteractionSource::GracePeriod)
    {
        return false;
    }

    if (!bTransformSettled)
    {
        return true;
    }

    if (GetEffectiveBindingMode() == EVRExpUIInfoBindingMode::Follow)
    {
        return true;
    }

    return CurrentPresentationSettings.OrientationMode ==
               EVRExpUIInfoOrientationMode::FaceCamera ||
           CurrentPresentationSettings.OrientationMode ==
               EVRExpUIInfoOrientationMode::FaceCameraYawOnly;
}

void UVRExpDetectableUIInfoTriggerLogic::RefreshTickRegistration()
{
    UVRExpUIInfoPresentationSubsystem *Subsystem =
        GetPresentationSubsystem();
    if (!Subsystem)
    {
        bTickRegistered = false;
        return;
    }

    const bool bShouldTick = RequiresPresentationTick();
    if (bShouldTick && !bTickRegistered)
    {
        Subsystem->RegisterTickLogic(this);
        bTickRegistered = true;
    }
    else if (!bShouldTick && bTickRegistered)
    {
        Subsystem->UnregisterTickLogic(this);
        bTickRegistered = false;
    }

    if (!bDrawRuntimeDebug)
    {
        ClearDebugScreenMessage();
    }
}

bool UVRExpDetectableUIInfoTriggerLogic::IsTimedHeadPresentationEnabled() const
{
    return bActivateOnHeadDetection &&
           HeadPresentationMode ==
               EVRExpUIInfoHeadPresentationMode::TimedWithCooldown;
}

bool UVRExpDetectableUIInfoTriggerLogic::ShouldEndTimedHeadPresentationOnDetectionLoss(
    const FVRExpDetectableInteractionContext &Context) const
{
    return IsTimedHeadPresentationEnabled() &&
           bHeadTimedPresentationActive &&
           HeadDetectionLossBehavior ==
               EVRExpUIInfoHeadDetectionLossBehavior::HideAndStartCooldown &&
           Context.ChangeSource ==
               EVRExpDetectableChangeSource::HeadDetection &&
           Context.ChangePhase ==
               EVRExpDetectableChangePhase::Ended &&
           !Context.bIsHeadDetected;
}

bool UVRExpDetectableUIInfoTriggerLogic::ShouldAwaitFreshHeadDetectionAfterCooldown(
    bool bIsHeadDetected) const
{
    return IsTimedHeadPresentationEnabled() &&
           HeadCooldownCompletionBehavior ==
               EVRExpUIInfoHeadCooldownCompletionBehavior::RequireNewDetection &&
           bIsHeadDetected;
}

FVRExpDetectableInteractionContext
UVRExpDetectableUIInfoTriggerLogic::BuildRefreshedHeadContext(
    EVRExpDetectableChangePhase ChangePhase) const
{
    FVRExpDetectableInteractionContext Context = LastContext;
    Context.ChangeSource =
        EVRExpDetectableChangeSource::HeadDetection;
    Context.ChangePhase = ChangePhase;

    if (UVRExpDetectableComponent *Component =
            DetectableComponent.Get())
    {
        Context.DetectableComponent = Component;
        Context.bIsHeadDetected = Component->IsHeadDetected();
        Context.bIsGripped = Component->IsGripped();
        Context.bIsInteracting = Component->IsInteracting();
        Context.GripSource = Component->GetResolvedGripSource();
        Context.GrabbableMotionComponent =
            Component->GetResolvedGrabbableMotionComponent();
        Context.MotionUpdatedComponent =
            Component->GetResolvedMotionUpdatedComponent();
    }

    return Context;
}

bool UVRExpDetectableUIInfoTriggerLogic::ShouldCountHeadVisibleDuration(
    bool bHasValidUIActor) const
{
    return bHeadTimedPresentationActive &&
           bHasValidUIActor &&
           bUIActorVisible &&
           bPresentationTargetAvailable &&
           (!IsExclusivePresentation() ||
            bExclusivePresentationGranted);
}

void UVRExpDetectableUIInfoTriggerLogic::StartHeadVisibleTimerIfNeeded()
{
    if (!IsTimedHeadPresentationEnabled() ||
        CurrentInteractionSource !=
            EVRExpUIInfoInteractionSource::HeadDetection ||
        bHeadTimedPresentationActive ||
        bHeadCooldownActive ||
        bAwaitingFreshHeadDetection ||
        !SpawnedUIActor.IsValid() ||
        !bUIActorVisible ||
        !bPresentationTargetAvailable ||
        (IsExclusivePresentation() &&
         !bExclusivePresentationGranted))
    {
        return;
    }

    UWorld *World = GetOwningWorld();
    if (!World)
    {
        return;
    }

    CancelHeadVisibleTimer();
    bHeadTimedPresentationActive = true;
    World->GetTimerManager().SetTimer(
        HeadVisibleTimerHandle,
        this,
        &UVRExpDetectableUIInfoTriggerLogic::HandleHeadVisibleTimerExpired,
        FMath::Max(0.1f, HeadVisibleDurationSeconds),
        false);
}

void UVRExpDetectableUIInfoTriggerLogic::RefreshHeadVisibleTimerCountingState()
{
    if (!bHeadTimedPresentationActive)
    {
        StartHeadVisibleTimerIfNeeded();
    }
    if (!bHeadTimedPresentationActive ||
        !HeadVisibleTimerHandle.IsValid())
    {
        return;
    }

    UWorld *World = GetOwningWorld();
    if (!World)
    {
        return;
    }

    FTimerManager &TimerManager = World->GetTimerManager();
    const bool bShouldCount =
        ShouldCountHeadVisibleDuration(SpawnedUIActor.IsValid());
    if (bShouldCount &&
        TimerManager.IsTimerPaused(HeadVisibleTimerHandle))
    {
        TimerManager.UnPauseTimer(HeadVisibleTimerHandle);
    }
    else if (!bShouldCount &&
             TimerManager.IsTimerActive(HeadVisibleTimerHandle))
    {
        TimerManager.PauseTimer(HeadVisibleTimerHandle);
    }
}

void UVRExpDetectableUIInfoTriggerLogic::EndTimedHeadPresentationState()
{
    if (!bHeadTimedPresentationActive)
    {
        return;
    }

    CancelHeadVisibleTimer();
    bHeadTimedPresentationActive = false;
    HeadActivationSequence = 0;
    bHeadClaimEligible = false;
    StartHeadCooldown();
}

void UVRExpDetectableUIInfoTriggerLogic::StartHeadCooldown()
{
    CancelHeadCooldownTimer();
    bAwaitingFreshHeadDetection = false;

    if (!IsTimedHeadPresentationEnabled())
    {
        bHeadCooldownActive = false;
        return;
    }

    UWorld *World = GetOwningWorld();
    if (!World)
    {
        bHeadCooldownActive = false;
        return;
    }

    bHeadCooldownActive = true;
    World->GetTimerManager().SetTimer(
        HeadCooldownTimerHandle,
        this,
        &UVRExpDetectableUIInfoTriggerLogic::HandleHeadCooldownTimerExpired,
        FMath::Max(0.1f, HeadCooldownSeconds),
        false);
}

void UVRExpDetectableUIInfoTriggerLogic::CancelHeadVisibleTimer()
{
    if (UWorld *World = GetOwningWorld())
    {
        World->GetTimerManager().ClearTimer(HeadVisibleTimerHandle);
    }
    HeadVisibleTimerHandle.Invalidate();
}

void UVRExpDetectableUIInfoTriggerLogic::CancelHeadCooldownTimer()
{
    if (UWorld *World = GetOwningWorld())
    {
        World->GetTimerManager().ClearTimer(HeadCooldownTimerHandle);
    }
    HeadCooldownTimerHandle.Invalidate();
}

void UVRExpDetectableUIInfoTriggerLogic::CancelHeadTimingTimers()
{
    CancelHeadVisibleTimer();
    CancelHeadCooldownTimer();
    bHeadTimedPresentationActive = false;
    bHeadCooldownActive = false;
    bAwaitingFreshHeadDetection = false;
}

void UVRExpDetectableUIInfoTriggerLogic::HandleHeadVisibleTimerExpired()
{
    HeadVisibleTimerHandle.Invalidate();
    if (!bHeadTimedPresentationActive)
    {
        return;
    }

    EndTimedHeadPresentationState();

    const FVRExpDetectableInteractionContext Context =
        BuildRefreshedHeadContext(
            EVRExpDetectableChangePhase::Updated);
    const EVRExpUIInfoInteractionSource NextSource =
        DetermineInteractionSource(Context);
    if (CurrentInteractionSource ==
            EVRExpUIInfoInteractionSource::HeadDetection &&
        NextSource == EVRExpUIInfoInteractionSource::None)
    {
        bGraceClaimEligible = false;
        RemoveExclusiveClaim();
        SetUIActorVisible(
            false,
            EVRExpUIInfoVisibilityReason::HeadVisibleDurationExpired);
    }

    EvaluateContext(Context);
    UpdateRuntimeDebugState();
    DrawRuntimeDebug();
    RefreshTickRegistration();
}

void UVRExpDetectableUIInfoTriggerLogic::HandleHeadCooldownTimerExpired()
{
    HeadCooldownTimerHandle.Invalidate();
    if (!bHeadCooldownActive)
    {
        return;
    }

    bHeadCooldownActive = false;
    if (!IsTimedHeadPresentationEnabled())
    {
        bAwaitingFreshHeadDetection = false;
        UpdateRuntimeDebugState();
        DrawRuntimeDebug();
        RefreshTickRegistration();
        return;
    }

    const bool bIsHeadDetected =
        DetectableComponent.IsValid() &&
        DetectableComponent->IsHeadDetected();
    if (ShouldAwaitFreshHeadDetectionAfterCooldown(bIsHeadDetected))
    {
        bAwaitingFreshHeadDetection = true;
        UpdateRuntimeDebugState();
        DrawRuntimeDebug();
        RefreshTickRegistration();
        return;
    }

    bAwaitingFreshHeadDetection = false;
    if (bIsHeadDetected)
    {
        EvaluateContext(
            BuildRefreshedHeadContext(
                EVRExpDetectableChangePhase::Began));
    }

    UpdateRuntimeDebugState();
    DrawRuntimeDebug();
    RefreshTickRegistration();
}

void UVRExpDetectableUIInfoTriggerLogic::StartInactivityTimers()
{
    if (!SpawnedUIActor.IsValid())
    {
        return;
    }

    UWorld *World = GetOwningWorld();
    if (!World)
    {
        return;
    }

    const float EffectiveHideDelay =
        FMath::Max(0.0f, HideDelaySeconds);
    const float EffectiveExitDuration =
        GetEffectiveExitTransitionDuration(
            EVRExpUIInfoVisibilityReason::InactivityTimeout);
    const float EffectiveHiddenCompletionDelay =
        EffectiveHideDelay + EffectiveExitDuration;
    float EffectiveDestroyDelay =
        FMath::Max(0.0f, DestroyDelaySeconds);
    if (EffectiveDestroyDelay < EffectiveHiddenCompletionDelay)
    {
        EffectiveDestroyDelay = EffectiveHiddenCompletionDelay;
        if (!bHasWarnedInvalidDestroyDelay)
        {
            bHasWarnedInvalidDestroyDelay = true;
            UE_LOG(LogVRExpDetectableUIInfoTriggerLogic, Warning,
                   TEXT("%s has DestroyDelaySeconds lower than HideDelaySeconds plus the exit transition duration; destroy delay is clamped to %.2f."),
                   *GetPathName(), EffectiveDestroyDelay);
        }
    }

    if (EffectiveHideDelay <= 0.0f)
    {
        HandleHideTimerExpired();
    }
    else
    {
        World->GetTimerManager().SetTimer(
            HideTimerHandle,
            this,
            &UVRExpDetectableUIInfoTriggerLogic::HandleHideTimerExpired,
            EffectiveHideDelay,
            false);
    }

    if (!SpawnedUIActor.IsValid())
    {
        return;
    }

    if (EffectiveDestroyDelay <= 0.0f)
    {
        HandleDestroyTimerExpired();
    }
    else
    {
        World->GetTimerManager().SetTimer(
            DestroyTimerHandle,
            this,
            &UVRExpDetectableUIInfoTriggerLogic::HandleDestroyTimerExpired,
            EffectiveDestroyDelay,
            false);
    }
}

void UVRExpDetectableUIInfoTriggerLogic::CancelInactivityTimers()
{
    if (UWorld *World = GetOwningWorld())
    {
        World->GetTimerManager().ClearTimer(HideTimerHandle);
        World->GetTimerManager().ClearTimer(DestroyTimerHandle);
    }
    HideTimerHandle.Invalidate();
    DestroyTimerHandle.Invalidate();
    bDestroyAfterExitTransition = false;
}

void UVRExpDetectableUIInfoTriggerLogic::HandleHideTimerExpired()
{
    if (bHasLocalInteraction)
    {
        return;
    }

    bGraceClaimEligible = false;
    RemoveExclusiveClaim();
    SetUIActorVisible(
        false, EVRExpUIInfoVisibilityReason::InactivityTimeout);
    bAwaitingPresentationTarget = false;
    UpdateRuntimeDebugState();
    RefreshTickRegistration();
}

void UVRExpDetectableUIInfoTriggerLogic::HandleDestroyTimerExpired()
{
    if (bHasLocalInteraction)
    {
        return;
    }

    if (bExitTransitionActive && !bExitTransitionReversing)
    {
        bDestroyAfterExitTransition = true;
        return;
    }

    DestroyUIActor(LastContext);
}

bool UVRExpDetectableUIInfoTriggerLogic::ShouldAnimateExit(
    EVRExpUIInfoVisibilityReason VisibilityReason) const
{
    if (ExitSettings.Mode != EVRExpUIInfoExitMode::Transition ||
        ExitSettings.DurationSeconds <= 0.0f)
    {
        return false;
    }

    switch (VisibilityReason)
    {
    case EVRExpUIInfoVisibilityReason::InactivityTimeout:
    case EVRExpUIInfoVisibilityReason::HiddenDuringRelease:
    case EVRExpUIInfoVisibilityReason::HeadVisibleDurationExpired:
    case EVRExpUIInfoVisibilityReason::HeadDetectionLost:
        return true;

    default:
        return false;
    }
}

float UVRExpDetectableUIInfoTriggerLogic::GetEffectiveExitTransitionDuration(
    EVRExpUIInfoVisibilityReason VisibilityReason) const
{
    return ShouldAnimateExit(VisibilityReason)
               ? FMath::Max(0.0f, ExitSettings.DurationSeconds)
               : 0.0f;
}

float UVRExpDetectableUIInfoTriggerLogic::EvaluateExitTransitionAlpha(
    float LinearAlpha) const
{
    const float ClampedLinearAlpha = FMath::Clamp(LinearAlpha, 0.0f, 1.0f);
    if (IsValid(ExitSettings.InterpolationCurve))
    {
        return FMath::Clamp(
            ExitSettings.InterpolationCurve->GetFloatValue(ClampedLinearAlpha),
            0.0f,
            1.0f);
    }

    return FMath::InterpEaseInOut(
        0.0f,
        1.0f,
        ClampedLinearAlpha,
        FMath::Max(0.01f, ExitSettings.EaseExponent));
}

void UVRExpDetectableUIInfoTriggerLogic::StartExitTransition(
    EVRExpUIInfoVisibilityReason VisibilityReason)
{
    AActor *UIActor = SpawnedUIActor.Get();
    if (!IsValid(UIActor) || !bUIActorVisible)
    {
        return;
    }

    if (bExitTransitionActive)
    {
        if (bExitTransitionReversing)
        {
            ExitVisibilityReason = VisibilityReason;
        }
        bExitTransitionReversing = false;
        RefreshTickRegistration();
        return;
    }

    ExitStartWorldTransform = UIActor->GetActorTransform();
    ExitTargetWorldTransform =
        ExitSettings.RelativeTransform * ExitStartWorldTransform;
    ExitVisibilityReason = VisibilityReason;
    ExitTransitionLinearAlpha = 0.0f;
    bExitTransitionActive = true;
    bExitTransitionReversing = false;
    bDestroyAfterExitTransition = false;

    CacheExitWidgetOpacities();
    ApplyExitTransitionVisuals(0.0f);
    RefreshHeadVisibleTimerCountingState();
    RefreshTickRegistration();
}

void UVRExpDetectableUIInfoTriggerLogic::ReverseExitTransition()
{
    if (!bExitTransitionActive)
    {
        return;
    }

    bExitTransitionReversing = true;
    bDestroyAfterExitTransition = false;
    RefreshTickRegistration();
}

void UVRExpDetectableUIInfoTriggerLogic::TickExitTransition(float DeltaTime)
{
    AActor *UIActor = SpawnedUIActor.Get();
    const float Duration = GetEffectiveExitTransitionDuration(
        ExitVisibilityReason);
    if (!IsValid(UIActor))
    {
        ResetExitTransitionState(false);
        bUIActorVisible = false;
        return;
    }
    if (Duration <= 0.0f)
    {
        if (bExitTransitionReversing)
        {
            ApplyExitTransitionVisuals(0.0f);
            RestoreExitTransitionVisuals();
            ResetExitTransitionState(false);
            bTransformSettled = false;
            bAwaitingPresentationTarget = bHasLocalInteraction;
        }
        else
        {
            SetUIActorVisibleImmediately(false, ExitVisibilityReason);
        }
        return;
    }

    const float AlphaStep = FMath::Max(0.0f, DeltaTime) / Duration;
    ExitTransitionLinearAlpha = bExitTransitionReversing
                                    ? FMath::Max(
                                          0.0f,
                                          ExitTransitionLinearAlpha - AlphaStep)
                                    : FMath::Min(
                                          1.0f,
                                          ExitTransitionLinearAlpha + AlphaStep);

    const float ExitAlpha = EvaluateExitTransitionAlpha(
        ExitTransitionLinearAlpha);
    ApplyExitTransitionVisuals(ExitAlpha);

    if (bExitTransitionReversing && ExitTransitionLinearAlpha <= 0.0f)
    {
        RestoreExitTransitionVisuals();
        ResetExitTransitionState(false);
        bTransformSettled = false;
        bAwaitingPresentationTarget = bHasLocalInteraction;
    }
    else if (!bExitTransitionReversing &&
             ExitTransitionLinearAlpha >= 1.0f)
    {
        CompleteExitTransition();
    }
}

void UVRExpDetectableUIInfoTriggerLogic::CompleteExitTransition()
{
    AActor *UIActor = SpawnedUIActor.Get();
    if (!IsValid(UIActor))
    {
        ResetExitTransitionState(false);
        bUIActorVisible = false;
        return;
    }

    const EVRExpUIInfoVisibilityReason CompletedReason =
        ExitVisibilityReason;
    const bool bVisibilityChanged = bUIActorVisible;
    const bool bShouldDestroyAfterTransition =
        bDestroyAfterExitTransition && !bHasLocalInteraction;

    UIActor->SetActorHiddenInGame(true);
    bUIActorVisible = false;
    RefreshHeadVisibleTimerCountingState();

    RestoreExitTransitionVisuals();
    ResetExitTransitionState(false);
    bTransformSettled = true;
    bAwaitingPresentationTarget = false;
    bDestroyAfterExitTransition = false;

    if (bVisibilityChanged)
    {
        RuntimeDebugState.LastVisibilityReason = CompletedReason;
        if (UIActor->GetClass()->ImplementsInterface(
                UVRExpUIInfoActorInterface::StaticClass()))
        {
            IVRExpUIInfoActorInterface::Execute_OnUIInfoVisibilityChanged(
                UIActor, false, CompletedReason, LastContext);
        }
        OnUIActorVisibilityChanged.Broadcast(
            UIActor, false, CompletedReason, LastContext);
    }

    if (bShouldDestroyAfterTransition && !bHasLocalInteraction &&
        SpawnedUIActor.IsValid())
    {
        DestroyUIActor(LastContext);
    }
}

void UVRExpDetectableUIInfoTriggerLogic::CacheExitWidgetOpacities()
{
    ExitWidgetOpacityStates.Reset();
    if (!ExitSettings.bFadeWidgetOpacity)
    {
        return;
    }

    AActor *UIActor = SpawnedUIActor.Get();
    if (!IsValid(UIActor))
    {
        return;
    }

    TInlineComponentArray<UWidgetComponent *> WidgetComponents(UIActor);
    for (UWidgetComponent *WidgetComponent : WidgetComponents)
    {
        if (!IsValid(WidgetComponent))
        {
            continue;
        }

        UUserWidget *UserWidget = WidgetComponent->GetUserWidgetObject();
        if (!IsValid(UserWidget))
        {
            continue;
        }

        FExitWidgetOpacityState &OpacityState =
            ExitWidgetOpacityStates.AddDefaulted_GetRef();
        OpacityState.Widget = UserWidget;
        OpacityState.OriginalOpacity = UserWidget->GetRenderOpacity();
    }
}

void UVRExpDetectableUIInfoTriggerLogic::ApplyExitTransitionVisuals(
    float ExitAlpha)
{
    const float ClampedExitAlpha = FMath::Clamp(ExitAlpha, 0.0f, 1.0f);
    for (const FExitWidgetOpacityState &OpacityState : ExitWidgetOpacityStates)
    {
        if (UUserWidget *UserWidget = OpacityState.Widget.Get())
        {
            UserWidget->SetRenderOpacity(
                OpacityState.OriginalOpacity * (1.0f - ClampedExitAlpha));
        }
    }

    if (ExitSettings.bApplyRelativeTransform)
    {
        if (AActor *UIActor = SpawnedUIActor.Get())
        {
            FTransform BlendedTransform;
            BlendedTransform.Blend(
                ExitStartWorldTransform,
                ExitTargetWorldTransform,
                ClampedExitAlpha);
            UIActor->SetActorTransform(
                BlendedTransform,
                false,
                nullptr,
                ETeleportType::TeleportPhysics);
        }
    }

    BroadcastExitTransitionUpdated(ClampedExitAlpha);
}

void UVRExpDetectableUIInfoTriggerLogic::RestoreExitTransitionVisuals()
{
    for (const FExitWidgetOpacityState &OpacityState : ExitWidgetOpacityStates)
    {
        if (UUserWidget *UserWidget = OpacityState.Widget.Get())
        {
            UserWidget->SetRenderOpacity(OpacityState.OriginalOpacity);
        }
    }
    ExitWidgetOpacityStates.Reset();

    if (bExitTransitionActive && ExitSettings.bApplyRelativeTransform)
    {
        if (AActor *UIActor = SpawnedUIActor.Get())
        {
            UIActor->SetActorTransform(
                ExitStartWorldTransform,
                false,
                nullptr,
                ETeleportType::TeleportPhysics);
        }
    }
}

void UVRExpDetectableUIInfoTriggerLogic::ResetExitTransitionState(
    bool bRestoreVisuals)
{
    if (bRestoreVisuals)
    {
        RestoreExitTransitionVisuals();
    }
    else
    {
        ExitWidgetOpacityStates.Reset();
    }

    ExitTransitionLinearAlpha = 0.0f;
    bExitTransitionActive = false;
    bExitTransitionReversing = false;
}

void UVRExpDetectableUIInfoTriggerLogic::BroadcastExitTransitionUpdated(
    float ExitAlpha) const
{
    AActor *UIActor = SpawnedUIActor.Get();
    if (!IsValid(UIActor) ||
        !UIActor->GetClass()->ImplementsInterface(
            UVRExpUIInfoActorInterface::StaticClass()))
    {
        return;
    }

    IVRExpUIInfoActorInterface::Execute_OnUIInfoExitTransitionUpdated(
        UIActor,
        FMath::Clamp(ExitAlpha, 0.0f, 1.0f),
        ExitVisibilityReason,
        LastContext);
}

void UVRExpDetectableUIInfoTriggerLogic::SetUIActorVisibleImmediately(
    bool bVisible,
    EVRExpUIInfoVisibilityReason VisibilityReason)
{
    AActor *UIActor = SpawnedUIActor.Get();
    if (!IsValid(UIActor))
    {
        ResetExitTransitionState(false);
        bUIActorVisible = false;
        return;
    }

    const bool bWasExitTransitionActive = bExitTransitionActive;
    if (!bVisible)
    {
        ExitVisibilityReason = VisibilityReason;
    }
    UIActor->SetActorHiddenInGame(!bVisible);
    if (bWasExitTransitionActive)
    {
        RestoreExitTransitionVisuals();
        ResetExitTransitionState(false);
    }
    if (bVisible && ExitVisibilityReason != EVRExpUIInfoVisibilityReason::None)
    {
        BroadcastExitTransitionUpdated(0.0f);
    }

    const bool bVisibilityChanged = bUIActorVisible != bVisible;
    bUIActorVisible = bVisible;
    if (bVisible)
    {
        StartHeadVisibleTimerIfNeeded();
    }
    RefreshHeadVisibleTimerCountingState();
    if (!bVisibilityChanged)
    {
        return;
    }

    RuntimeDebugState.LastVisibilityReason = VisibilityReason;
    if (UIActor->GetClass()->ImplementsInterface(
            UVRExpUIInfoActorInterface::StaticClass()))
    {
        IVRExpUIInfoActorInterface::Execute_OnUIInfoVisibilityChanged(
            UIActor, bVisible, VisibilityReason, LastContext);
    }
    OnUIActorVisibilityChanged.Broadcast(
        UIActor, bVisible, VisibilityReason, LastContext);
}

void UVRExpDetectableUIInfoTriggerLogic::SetUIActorVisible(
    bool bVisible,
    EVRExpUIInfoVisibilityReason VisibilityReason)
{
    if (bVisible)
    {
        if (bExitTransitionActive)
        {
            ReverseExitTransition();
            StartHeadVisibleTimerIfNeeded();
            RefreshHeadVisibleTimerCountingState();
            return;
        }

        SetUIActorVisibleImmediately(true, VisibilityReason);
        return;
    }

    if (ShouldAnimateExit(VisibilityReason))
    {
        StartExitTransition(VisibilityReason);
        return;
    }

    SetUIActorVisibleImmediately(false, VisibilityReason);
}

void UVRExpDetectableUIInfoTriggerLogic::BroadcastPresentationChanged()
{
    AActor *UIActor = SpawnedUIActor.Get();
    if (!IsValid(UIActor))
    {
        return;
    }

    if (UIActor->GetClass()->ImplementsInterface(
            UVRExpUIInfoActorInterface::StaticClass()))
    {
        IVRExpUIInfoActorInterface::Execute_OnUIInfoPresentationChanged(
            UIActor,
            CurrentInteractionSource,
            CurrentPresentationSettings,
            LastContext);
    }
    OnUIActorPresentationChanged.Broadcast(
        UIActor,
        CurrentInteractionSource,
        CurrentPresentationSettings,
        LastContext);
}

void UVRExpDetectableUIInfoTriggerLogic::UpdateRuntimeDebugState()
{
    RefreshHeadVisibleTimerCountingState();
    RuntimeDebugState.InteractionSource = CurrentInteractionSource;
    RuntimeDebugState.AnchorType = CurrentPresentationSettings.AnchorType;
    RuntimeDebugState.BindingMode = GetEffectiveBindingMode();
    RuntimeDebugState.PositionMode =
        CurrentPresentationSettings.PositionMode;
    RuntimeDebugState.OrientationMode =
        CurrentPresentationSettings.OrientationMode;
    RuntimeDebugState.PresentationPolicy =
        CurrentPresentationSettings.PresentationPolicy;
    RuntimeDebugState.bTargetResolved =
        bPresentationTargetAvailable;
    RuntimeDebugState.bUIActorVisible = IsUIActorVisible();
    RuntimeDebugState.bExclusivePresentationGranted =
        bExclusivePresentationGranted;
    RuntimeDebugState.ExclusivePriority = GetExclusivePriority();
    RuntimeDebugState.GripSource = LastContext.GripSource;
    RuntimeDebugState.MotionState = LastContext.MotionState;
    RuntimeDebugState.MotionPhase = LastContext.MotionPhase;
    RuntimeDebugState.PreviousMotionPhase =
        LastContext.PreviousMotionPhase;
    RuntimeDebugState.NormalMotionMode =
        LastContext.NormalMotionMode;
    RuntimeDebugState.ReleaseMotionMode =
        LastContext.ReleaseMotionMode;
    RuntimeDebugState.MotionChangeFlags =
        LastContext.MotionChangeFlags;
    RuntimeDebugState.MotionSnapshotSequence =
        LastContext.MotionSnapshotSequence;
    RuntimeDebugState.GrabbableMotionComponent =
        LastContext.GrabbableMotionComponent;
    RuntimeDebugState.MotionUpdatedComponent =
        LastContext.MotionUpdatedComponent;
    RuntimeDebugState.ActiveGripCount = LastContext.ActiveGripCount;
    RuntimeDebugState.bChangedGripHasMovementAuthority =
        LastContext.bChangedGripHasMovementAuthority;
    RuntimeDebugState.bHasAnyMovementAuthority =
        LastContext.bHasAnyMovementAuthority;
    RuntimeDebugState.bAwaitingFreshHeadDetection =
        bAwaitingFreshHeadDetection;
    RuntimeDebugState.bExitTransitionActive = bExitTransitionActive;
    RuntimeDebugState.bExitTransitionReversing =
        bExitTransitionActive && bExitTransitionReversing;
    RuntimeDebugState.ExitTransitionAlpha =
        bExitTransitionActive
            ? EvaluateExitTransitionAlpha(ExitTransitionLinearAlpha)
            : 0.0f;
    RuntimeDebugState.ExitTransitionRemainingSeconds =
        bExitTransitionActive
            ? GetEffectiveExitTransitionDuration(ExitVisibilityReason) *
                  (bExitTransitionReversing
                       ? ExitTransitionLinearAlpha
                       : 1.0f - ExitTransitionLinearAlpha)
            : -1.0f;
    RuntimeDebugState.ExitVisibilityReason = ExitVisibilityReason;

    const FVRExpDetectableTriggerRuntimeDebugState TriggerDebugState =
        GetTriggerRuntimeDebugState();
    RuntimeDebugState.MatchedActivationSource =
        TriggerDebugState.MatchedActivationSource;
    RuntimeDebugState.ActivationFailureReason =
        TriggerDebugState.FirstFailureReason;

    const bool bCanKeepLast =
        CurrentPresentationSettings.MissingAnchorPolicy ==
            EVRExpUIInfoMissingAnchorPolicy::KeepLastAndRetry &&
        bHasValidPresentationTransform;
    RuntimeDebugState.bExclusiveClaimEligible =
        IsExclusivePresentation() &&
        IsSourceClaimEligible(CurrentInteractionSource) &&
        (bPresentationTargetAvailable || bCanKeepLast);

    if (AActor *UIActor = SpawnedUIActor.Get())
    {
        RuntimeDebugState.CurrentWorldTransform =
            UIActor->GetActorTransform();
        RuntimeDebugState.PositionError =
            bPresentationTargetAvailable
                ? static_cast<float>(FVector::Distance(
                      RuntimeDebugState.CurrentWorldTransform.GetLocation(),
                      RuntimeDebugState.TargetWorldTransform.GetLocation()))
                : -1.0f;
        RuntimeDebugState.RotationErrorDegrees =
            bPresentationTargetAvailable
                ? static_cast<float>(FMath::RadiansToDegrees(
                      RuntimeDebugState.CurrentWorldTransform.GetRotation()
                          .AngularDistance(
                              RuntimeDebugState.TargetWorldTransform
                                  .GetRotation())))
                : -1.0f;
        RuntimeDebugState.ScaleError =
            bPresentationTargetAvailable
                ? static_cast<float>(
                      (RuntimeDebugState.CurrentWorldTransform.GetScale3D() -
                       RuntimeDebugState.TargetWorldTransform.GetScale3D())
                          .GetAbsMax())
                : -1.0f;
    }
    else
    {
        RuntimeDebugState.CurrentWorldTransform = FTransform::Identity;
        RuntimeDebugState.PositionError = -1.0f;
        RuntimeDebugState.RotationErrorDegrees = -1.0f;
        RuntimeDebugState.ScaleError = -1.0f;
    }

    RuntimeDebugState.HideRemainingSeconds = -1.0f;
    RuntimeDebugState.DestroyRemainingSeconds = -1.0f;
    RuntimeDebugState.HeadVisibleRemainingSeconds = -1.0f;
    RuntimeDebugState.HeadCooldownRemainingSeconds = -1.0f;
    if (UWorld *World = GetOwningWorld())
    {
        if (HideTimerHandle.IsValid())
        {
            RuntimeDebugState.HideRemainingSeconds =
                World->GetTimerManager().GetTimerRemaining(HideTimerHandle);
        }
        if (DestroyTimerHandle.IsValid())
        {
            RuntimeDebugState.DestroyRemainingSeconds =
                World->GetTimerManager().GetTimerRemaining(DestroyTimerHandle);
        }
        if (HeadVisibleTimerHandle.IsValid())
        {
            RuntimeDebugState.HeadVisibleRemainingSeconds =
                World->GetTimerManager().GetTimerRemaining(
                    HeadVisibleTimerHandle);
        }
        if (HeadCooldownTimerHandle.IsValid())
        {
            RuntimeDebugState.HeadCooldownRemainingSeconds =
                World->GetTimerManager().GetTimerRemaining(
                    HeadCooldownTimerHandle);
        }
    }

    RuntimeDebugState.FailureReason = LastPlacementFailureReason;
}

void UVRExpDetectableUIInfoTriggerLogic::DrawRuntimeDebug()
{
    if (!bDrawRuntimeDebug)
    {
        ClearDebugScreenMessage();
        return;
    }

    UWorld *World = GetOwningWorld();
    if (!World)
    {
        return;
    }

    const FColor DebugColor = GetRuntimeDebugColor();
    const float AxisLength = FMath::Max(1.0f, DebugAxisLength);

    if (bDrawDebugWorldGeometry)
    {
        if (RuntimeDebugState.bAnchorResolved)
        {
            DrawDebugCoordinateSystem(
                World,
                RuntimeDebugState.AnchorWorldTransform.GetLocation(),
                RuntimeDebugState.AnchorWorldTransform.Rotator(),
                AxisLength,
                false,
                0.0f,
                0,
                1.0f);
        }

        if (RuntimeDebugState.bTargetResolved)
        {
            DrawDebugCoordinateSystem(
                World,
                RuntimeDebugState.TargetWorldTransform.GetLocation(),
                RuntimeDebugState.TargetWorldTransform.Rotator(),
                AxisLength * 0.8f,
                false,
                0.0f,
                0,
                1.0f);

            if (RuntimeDebugState.bAnchorResolved)
            {
                DrawDebugLine(
                    World,
                    RuntimeDebugState.AnchorWorldTransform.GetLocation(),
                    RuntimeDebugState.TargetWorldTransform.GetLocation(),
                    DebugColor,
                    false,
                    0.0f,
                    0,
                    1.0f);
            }
        }

        if (SpawnedUIActor.IsValid())
        {
            const FVector CurrentLocation =
                RuntimeDebugState.CurrentWorldTransform.GetLocation();
            DrawDebugCoordinateSystem(
                World,
                CurrentLocation,
                RuntimeDebugState.CurrentWorldTransform.Rotator(),
                AxisLength * 0.6f,
                false,
                0.0f,
                0,
                1.0f);
            DrawDebugSphere(
                World,
                CurrentLocation,
                AxisLength * 0.12f,
                12,
                DebugColor,
                false,
                0.0f,
                0,
                1.5f);

            if (RuntimeDebugState.bTargetResolved)
            {
                DrawDebugLine(
                    World,
                    CurrentLocation,
                    RuntimeDebugState.TargetWorldTransform.GetLocation(),
                    DebugColor,
                    false,
                    0.0f,
                    0,
                    1.5f);
            }
        }
    }

    if (bDrawDebugScreenSummary && GEngine)
    {
        const FString AnchorName =
            IsValid(RuntimeDebugState.AnchorComponent.Get())
                ? GetNameSafe(RuntimeDebugState.AnchorComponent.Get())
                : GetNameSafe(RuntimeDebugState.AnchorActor.Get());
        const FString MotionName =
            GetNameSafe(RuntimeDebugState.GrabbableMotionComponent.Get());
        const FString UpdatedComponentName =
            GetNameSafe(RuntimeDebugState.MotionUpdatedComponent.Get());
        const FString FailureText =
            RuntimeDebugState.FailureReason.IsEmpty()
                ? TEXT("None")
                : RuntimeDebugState.FailureReason;
        const FString PositionErrorText =
            RuntimeDebugState.PositionError >= 0.0f
                ? FString::Printf(
                      TEXT("%.2f cm"), RuntimeDebugState.PositionError)
                : TEXT("N/A");
        const FString RotationErrorText =
            RuntimeDebugState.RotationErrorDegrees >= 0.0f
                ? FString::Printf(
                      TEXT("%.2f deg"),
                      RuntimeDebugState.RotationErrorDegrees)
                : TEXT("N/A");
        const FString ScaleErrorText =
            RuntimeDebugState.ScaleError >= 0.0f
                ? FString::Printf(
                      TEXT("%.4f"), RuntimeDebugState.ScaleError)
                : TEXT("N/A");
        const FString ActivationFailureText =
            RuntimeDebugState.ActivationFailureReason.IsEmpty()
                ? TEXT("None")
                : RuntimeDebugState.ActivationFailureReason;
        const FString MotionChangeFlagsText =
            GetMotionChangeFlagsText(
                RuntimeDebugState.MotionChangeFlags);
        const TCHAR *ExitStateText =
            RuntimeDebugState.bExitTransitionActive
                ? (RuntimeDebugState.bExitTransitionReversing
                       ? TEXT("Restoring")
                       : TEXT("Exiting"))
                : TEXT("Inactive");
        const FString SummaryText = FString::Printf(
            TEXT("[VR UI Info] %s\n")
            TEXT("Source: %s | Visible: %s | Target: %s\n")
            TEXT("Head Mode: %s | Visible Timer: %.2f | Cooldown: %.2f | Await Fresh: %s\n")
            TEXT("Anchor: %s / %s | Binding: %s | Position: %s | Orientation: %s\n")
            TEXT("GripSource: %s | Motion: %s / %s | State: %s | Phase: %s (Prev %s) | Grips: %d | Authority: %s\n")
            TEXT("Normal/Release: %s / %s | Snapshot: %lld | Changes: %s\n")
            TEXT("Activation Match: %s | Activation Failure: %s\n")
            TEXT("Policy: %s | Priority: %d | Eligible: %s | Granted: %s\n")
            TEXT("Exit: %s | Alpha: %.3f | Remaining: %.2f | Reason: %s\n")
            TEXT("Error P/R/S: %s / %s / %s | Hide: %.2f | Destroy: %.2f\n")
            TEXT("Failure: %s"),
            *GetNameSafe(DetectableComponent.IsValid()
                             ? DetectableComponent->GetOwner()
                             : nullptr),
            *GetUIInfoEnumDisplayName(CurrentInteractionSource),
            IsUIActorVisible() ? TEXT("Yes") : TEXT("No"),
            bPresentationTargetAvailable ? TEXT("Resolved")
                                          : TEXT("Missing"),
            *GetUIInfoEnumDisplayName(HeadPresentationMode),
            RuntimeDebugState.HeadVisibleRemainingSeconds,
            RuntimeDebugState.HeadCooldownRemainingSeconds,
            RuntimeDebugState.bAwaitingFreshHeadDetection
                ? TEXT("Yes")
                : TEXT("No"),
            *GetUIInfoEnumDisplayName(CurrentPresentationSettings.AnchorType),
            *AnchorName,
            *GetUIInfoEnumDisplayName(GetEffectiveBindingMode()),
            *GetUIInfoEnumDisplayName(
                CurrentPresentationSettings.PositionMode),
            *GetUIInfoEnumDisplayName(
                CurrentPresentationSettings.OrientationMode),
            *GetUIInfoEnumDisplayName(RuntimeDebugState.GripSource),
            *MotionName,
            *UpdatedComponentName,
            *GetUIInfoEnumDisplayName(RuntimeDebugState.MotionState),
            *GetUIInfoEnumDisplayName(RuntimeDebugState.MotionPhase),
            *GetUIInfoEnumDisplayName(
                RuntimeDebugState.PreviousMotionPhase),
            RuntimeDebugState.ActiveGripCount,
            RuntimeDebugState.bHasAnyMovementAuthority
                ? TEXT("Yes")
                : TEXT("No"),
            *GetUIInfoEnumDisplayName(
                RuntimeDebugState.NormalMotionMode),
            *GetUIInfoEnumDisplayName(
                RuntimeDebugState.ReleaseMotionMode),
            RuntimeDebugState.MotionSnapshotSequence,
            *MotionChangeFlagsText,
            *GetUIInfoEnumDisplayName(
                RuntimeDebugState.MatchedActivationSource),
            *ActivationFailureText,
            *GetUIInfoEnumDisplayName(
                CurrentPresentationSettings.PresentationPolicy),
            GetExclusivePriority(),
            RuntimeDebugState.bExclusiveClaimEligible ? TEXT("Yes")
                                                       : TEXT("No"),
            bExclusivePresentationGranted ? TEXT("Yes") : TEXT("No"),
            ExitStateText,
            RuntimeDebugState.ExitTransitionAlpha,
            RuntimeDebugState.ExitTransitionRemainingSeconds,
            *GetUIInfoEnumDisplayName(
                RuntimeDebugState.ExitVisibilityReason),
            *PositionErrorText,
            *RotationErrorText,
            *ScaleErrorText,
            RuntimeDebugState.HideRemainingSeconds,
            RuntimeDebugState.DestroyRemainingSeconds,
            *FailureText);

        const float TextScale = FMath::Max(0.5f, DebugScreenTextScale);
        GEngine->AddOnScreenDebugMessage(
            GetDebugScreenMessageKey(),
            0.25f,
            DebugColor,
            SummaryText,
            false,
            FVector2D(TextScale, TextScale));
    }
}

FColor UVRExpDetectableUIInfoTriggerLogic::GetRuntimeDebugColor() const
{
    if (CurrentInteractionSource != EVRExpUIInfoInteractionSource::None &&
        !bPresentationTargetAvailable)
    {
        return FColor::Red;
    }
    if (bWasPriorityDisplaced)
    {
        return FColor(180, 0, 255);
    }
    if (CurrentInteractionSource ==
        EVRExpUIInfoInteractionSource::GracePeriod)
    {
        return FColor::Yellow;
    }
    if (!IsExclusivePresentation())
    {
        return FColor::Cyan;
    }
    return IsUIActorVisible() ? FColor::Green : FColor::Silver;
}

uint64 UVRExpDetectableUIInfoTriggerLogic::GetDebugScreenMessageKey() const
{
    constexpr uint64 DebugMessageKeyPrefix = 0x5652554900000000ULL;
    return DebugMessageKeyPrefix |
           static_cast<uint64>(static_cast<uint32>(GetUniqueID()));
}

void UVRExpDetectableUIInfoTriggerLogic::ClearDebugScreenMessage() const
{
    if (GEngine)
    {
        GEngine->RemoveOnScreenDebugMessage(GetDebugScreenMessageKey());
    }
}

UWorld *UVRExpDetectableUIInfoTriggerLogic::GetOwningWorld() const
{
    return DetectableComponent.IsValid()
               ? DetectableComponent->GetWorld()
               : nullptr;
}

UVRExpUIInfoPresentationSubsystem *
UVRExpDetectableUIInfoTriggerLogic::GetPresentationSubsystem() const
{
    UWorld *World = GetOwningWorld();
    return World
               ? World->GetSubsystem<UVRExpUIInfoPresentationSubsystem>()
               : nullptr;
}
