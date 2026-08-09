#include "Detection/VRExpDetectableTriggerLogicBase.h"

#include "Detection/VRExpDetectableComponent.h"

UVRExpDetectableTriggerLogicBase::UVRExpDetectableTriggerLogicBase()
    : bEnabled(true),
      bIsActive(false),
      bIsInitialized(false)
{
}

void UVRExpDetectableTriggerLogicBase::Initialize(
    UVRExpDetectableComponent *InDetectableComponent,
    const FVRExpDetectableInteractionContext &InitialContext)
{
    if (bIsInitialized)
    {
        Deinitialize(InitialContext);
    }

    DetectableComponent = InDetectableComponent;
    bIsInitialized = true;
    bIsActive = false;
    TriggerRuntimeDebugState =
        FVRExpDetectableTriggerRuntimeDebugState();

    OnInitialized(InitialContext);
    EvaluateContext(InitialContext);
}

void UVRExpDetectableTriggerLogicBase::Deinitialize(
    const FVRExpDetectableInteractionContext &FinalContext)
{
    if (!bIsInitialized)
    {
        return;
    }

    if (bIsActive)
    {
        bIsActive = false;
        OnDeactivated(FinalContext);
    }

    OnDeinitialized(FinalContext);
    DetectableComponent.Reset();
    bIsInitialized = false;
}

void UVRExpDetectableTriggerLogicBase::EvaluateContext(
    const FVRExpDetectableInteractionContext &Context)
{
    if (!bIsInitialized)
    {
        return;
    }

    TriggerRuntimeDebugState =
        FVRExpDetectableTriggerRuntimeDebugState();
    TriggerRuntimeDebugState.MotionSnapshotSequence =
        Context.MotionSnapshotSequence;
    TriggerRuntimeDebugState.MotionPhase = Context.MotionPhase;
    TriggerRuntimeDebugState.PreviousMotionPhase =
        Context.PreviousMotionPhase;

    bool bActivationMatches = false;
    if (bEnabled)
    {
        bActivationMatches =
            EvaluateSourceSwitches(
                Context,
                TriggerRuntimeDebugState.MatchedActivationSource,
                TriggerRuntimeDebugState.FirstFailureReason);
    }
    else
    {
        TriggerRuntimeDebugState.FirstFailureReason =
            TEXT("Trigger Logic is disabled.");
    }

    const bool bShouldBeActive =
        bEnabled && bActivationMatches;
    TriggerRuntimeDebugState.bFinalActive = bShouldBeActive;
    if (bShouldBeActive == bIsActive)
    {
        if (bIsActive)
        {
            OnActiveContextUpdated(Context);
        }
        return;
    }

    bIsActive = bShouldBeActive;
    if (bIsActive)
    {
        OnActivated(Context);
    }
    else
    {
        OnDeactivated(Context);
    }
}

void UVRExpDetectableTriggerLogicBase::OnInitialized(
    const FVRExpDetectableInteractionContext &InitialContext)
{
}

void UVRExpDetectableTriggerLogicBase::OnActivated(
    const FVRExpDetectableInteractionContext &Context)
{
}

void UVRExpDetectableTriggerLogicBase::OnActiveContextUpdated(
    const FVRExpDetectableInteractionContext &Context)
{
}

void UVRExpDetectableTriggerLogicBase::OnDeactivated(
    const FVRExpDetectableInteractionContext &Context)
{
}

void UVRExpDetectableTriggerLogicBase::OnDeinitialized(
    const FVRExpDetectableInteractionContext &FinalContext)
{
}

bool UVRExpDetectableTriggerLogicBase::EvaluateSourceSwitches(
    const FVRExpDetectableInteractionContext &Context,
    EVRExpDetectableActivationMatchSource &OutMatchedSource,
    FString &OutFailureReason) const
{
    OutMatchedSource =
        EVRExpDetectableActivationMatchSource::None;
    OutFailureReason =
        TEXT("Source Switches are not implemented by this Trigger Logic.");
    return false;
}
