#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "Detection/VRExpDetectableTriggerLogicBase.h"
#include "UI/VRExpUIInfoPlacementResolver.h"
#include "UI/VRExpUIInfoTypes.h"
#include "VRExpDetectableUIInfoTriggerLogic.generated.h"

class AActor;
class APlayerController;
class UCameraComponent;
class USceneComponent;
class UUserWidget;
class UVRExpUIInfoPresentationSubsystem;
struct FPropertyChangedEvent;

UCLASS(BlueprintType, EditInlineNew)
class VREXPANSIONEXTENSIONS_API UVRExpDetectableUIInfoTriggerLogic
    : public UVRExpDetectableTriggerLogicBase
{
    GENERATED_BODY()

public:
    UVRExpDetectableUIInfoTriggerLogic();

    UFUNCTION(BlueprintPure, Category = "VRExpansionExtensions|UI Info")
    AActor *GetSpawnedUIActor() const;

    UFUNCTION(BlueprintPure, Category = "VRExpansionExtensions|UI Info")
    bool IsUIActorVisible() const;

    UFUNCTION(BlueprintPure, Category = "VRExpansionExtensions|UI Info|Debug")
    FVRExpUIInfoRuntimeDebugState GetRuntimeDebugState() const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info")
    TSubclassOf<AActor> UIActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Activation",
              meta = (DisplayName = "Activate On Head Detection",
                      ToolTip = "Allow Head Detection to activate this UI Trigger Logic."))
    bool bActivateOnHeadDetection;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Activation",
              meta = (DisplayName = "Head Only In Normal Phase",
                      EditCondition = "bActivateOnHeadDetection",
                      EditConditionHides,
                      ToolTip = "When a Motion Component exists, only allow Head Detection during MotionPhase.Normal. Head Detection remains available when no Motion Component exists."))
    bool bHeadOnlyInNormalPhase;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Activation|Head",
              meta = (DisplayName = "Head Presentation Mode",
                      EditCondition = "bActivateOnHeadDetection",
                      EditConditionHides,
                      ToolTip = "Keep the existing while-detected behavior or use a fixed visible duration followed by a Head-only cooldown."))
    EVRExpUIInfoHeadPresentationMode HeadPresentationMode;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Activation|Head",
              meta = (DisplayName = "Head Visible Duration",
                      EditCondition = "bActivateOnHeadDetection && HeadPresentationMode == EVRExpUIInfoHeadPresentationMode::TimedWithCooldown",
                      EditConditionHides,
                      ClampMin = "0.1",
                      UIMin = "0.1",
                      Units = "Seconds",
                      ToolTip = "Visible time for one timed Head presentation. Timing starts only after the UI Actor is actually visible."))
    float HeadVisibleDurationSeconds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Activation|Head",
              meta = (DisplayName = "Head Detection Loss Behavior",
                      EditCondition = "bActivateOnHeadDetection && HeadPresentationMode == EVRExpUIInfoHeadPresentationMode::TimedWithCooldown",
                      EditConditionHides,
                      ToolTip = "Choose whether a timed Head presentation remains eligible for its full duration after Head Detection ends."))
    EVRExpUIInfoHeadDetectionLossBehavior HeadDetectionLossBehavior;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Activation|Head",
              meta = (DisplayName = "Head Cooldown",
                      EditCondition = "bActivateOnHeadDetection && HeadPresentationMode == EVRExpUIInfoHeadPresentationMode::TimedWithCooldown",
                      EditConditionHides,
                      ClampMin = "0.1",
                      UIMin = "0.1",
                      Units = "Seconds",
                      ToolTip = "Time during which Head Detection cannot start another timed Head presentation."))
    float HeadCooldownSeconds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Activation|Head",
              meta = (DisplayName = "Head Cooldown Completion Behavior",
                      EditCondition = "bActivateOnHeadDetection && HeadPresentationMode == EVRExpUIInfoHeadPresentationMode::TimedWithCooldown",
                      EditConditionHides,
                      ToolTip = "Choose whether an active Head Detection may reactivate immediately when cooldown ends or must begin again."))
    EVRExpUIInfoHeadCooldownCompletionBehavior HeadCooldownCompletionBehavior;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Activation",
              meta = (DisplayName = "Activate While Gripped",
                      ToolTip = "Allow either direct Grip routing or a Grabbable Motion Component Grip to activate this UI Trigger Logic."))
    bool bActivateWhileGripped;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Presentation")
    FVRExpUIInfoPresentationSettings HeadPresentationSettings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Presentation")
    FVRExpUIInfoPresentationSettings GripPresentationSettings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Release",
              meta = (DisplayName = "Release Behavior",
                      ToolTip = "Controls UI presentation while the authoritative Motion Phase is Releasing."))
    EVRExpUIInfoReleaseBehavior ReleaseBehavior;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Release",
              meta = (DisplayName = "Release Presentation Settings",
                      EditCondition = "ReleaseBehavior == EVRExpUIInfoReleaseBehavior::UseReleasePresentation",
                      EditConditionHides,
                      ToolTip = "Independent placement and arbitration settings used when Release Behavior is Use Release Presentation."))
    FVRExpUIInfoPresentationSettings ReleasePresentationSettings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Activation",
              meta = (DisplayName = "Enable Motion-Only Presentation",
                      ToolTip = "Allow an authoritative Motion Component to present UI without Head or Grip interaction."))
    bool bEnableMotionOnlyPresentation;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Motion",
              meta = (DisplayName = "Motion Presentation Settings",
                      EditCondition = "bEnableMotionOnlyPresentation",
                      EditConditionHides,
                      ToolTip = "Placement and arbitration settings used when Motion is the active presentation source."))
    FVRExpUIInfoPresentationSettings MotionPresentationSettings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Motion",
              meta = (DisplayName = "Motion Local Player Index",
                      EditCondition = "bEnableMotionOnlyPresentation",
                      EditConditionHides,
                      ClampMin = "0",
                      ToolTip = "Index into the Game Instance local-player list. Motion-only presentation never infers a player from remote Grip Controllers."))
    int32 MotionLocalPlayerIndex;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Entry")
    FVRExpUIInfoEntrySettings EntrySettings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Exit")
    FVRExpUIInfoExitSettings ExitSettings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Lifetime",
              meta = (DisplayName = "Hide Delay",
                      ClampMin = "0.0",
                      Units = "Seconds",
                      ToolTip = "Time to remain visible after normal interaction ends before the inactivity exit transition begins."))
    float HideDelaySeconds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Lifetime",
              meta = (DisplayName = "Destroy Delay",
                      ClampMin = "0.0",
                      Units = "Seconds",
                      ToolTip = "Time after normal interaction ends before destroying the UI Actor. Runtime clamps this to Hide Delay plus the exit duration."))
    float DestroyDelaySeconds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Priority",
              meta = (DisplayName = "Restore After Priority Loss",
                      ToolTip = "Allow an eligible exclusive presentation to become visible again after a higher-priority presentation releases the slot."))
    bool bRestoreAfterPriorityLoss;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Priority",
              meta = (DisplayName = "Keep Exclusive Claim During Grace Period",
                      ToolTip = "Keep an exclusive claim during the visible grace period when this presentation was the winner."))
    bool bKeepExclusiveClaimDuringGracePeriod;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Priority",
              meta = (DisplayName = "Grace Period Priority",
                      EditCondition = "bKeepExclusiveClaimDuringGracePeriod",
                      EditConditionHides,
                      ToolTip = "Priority used by a retained exclusive claim during the grace period."))
    int32 GracePeriodPriority;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Debug",
              meta = (DisplayName = "Draw Runtime Debug",
                      ToolTip = "Draw placement geometry and a fixed non-stacking summary in PIE/Game."))
    bool bDrawRuntimeDebug;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Debug",
              meta = (DisplayName = "Draw World Geometry",
                      EditCondition = "bDrawRuntimeDebug",
                      EditConditionHides,
                      ToolTip = "Draw Anchor, Target, and Current coordinate systems plus status-colored connection lines every runtime frame."))
    bool bDrawDebugWorldGeometry;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Debug",
              meta = (DisplayName = "Draw Screen Summary",
                      EditCondition = "bDrawRuntimeDebug",
                      EditConditionHides,
                      ToolTip = "Display one fixed-key, non-stacking runtime status summary for this trigger logic instance."))
    bool bDrawDebugScreenSummary;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Debug",
              meta = (DisplayName = "Debug Axis Length",
                      EditCondition = "bDrawRuntimeDebug && bDrawDebugWorldGeometry",
                      EditConditionHides,
                      ClampMin = "1.0",
                      Units = "cm",
                      ToolTip = "World-space length in centimeters used for the debug coordinate systems."))
    float DebugAxisLength;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Info|Debug",
              meta = (DisplayName = "Debug Screen Text Scale",
                      EditCondition = "bDrawRuntimeDebug && bDrawDebugScreenSummary",
                      EditConditionHides,
                      ClampMin = "0.5",
                      ClampMax = "3.0",
                      ToolTip = "Scale applied to the fixed-key on-screen runtime status text."))
    float DebugScreenTextScale;

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "UI Info|Editor Preview",
              meta = (DisplayName = "Editor Preview Mode",
                      ToolTip = "Disabled has no editor side effects. UI Actor explicitly runs the preview class Construction Script."))
    EVRExpUIInfoEditorPreviewMode EditorPreviewMode;

    UPROPERTY(EditAnywhere, Category = "UI Info|Editor Preview",
              meta = (DisplayName = "Editor Preview Source",
                      EditCondition = "EditorPreviewMode != EVRExpUIInfoEditorPreviewMode::Disabled",
                      EditConditionHides,
                      ToolTip = "Preview exactly one presentation source in Blueprint Editor and Level Editor viewports."))
    EVRExpUIInfoEditorPreviewSource EditorPreviewSource;

    UPROPERTY(VisibleAnywhere, Transient, Category = "UI Info|Editor Preview",
              meta = (DisplayName = "Editor Preview Status"))
    EVRExpUIInfoEditorPreviewStatus EditorPreviewStatus;

    UPROPERTY(VisibleAnywhere, Transient, Category = "UI Info|Editor Preview",
              meta = (DisplayName = "Editor Preview Failure Reason",
                      EditCondition = "EditorPreviewStatus != EVRExpUIInfoEditorPreviewStatus::Disabled && EditorPreviewStatus != EVRExpUIInfoEditorPreviewStatus::Active",
                      EditConditionHides))
    FString EditorPreviewFailureReason;
#endif

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "UI Info|Debug|Runtime")
    FVRExpUIInfoRuntimeDebugState RuntimeDebugState;

    UPROPERTY(BlueprintAssignable, Category = "VRExpansionExtensions|UI Info")
    FVRExpUIInfoActorSpawnedEvent OnUIActorSpawned;

    UPROPERTY(BlueprintAssignable, Category = "VRExpansionExtensions|UI Info")
    FVRExpUIInfoPresentationChangedEvent OnUIActorPresentationChanged;

    UPROPERTY(BlueprintAssignable, Category = "VRExpansionExtensions|UI Info")
    FVRExpUIInfoVisibilityChangedEvent OnUIActorVisibilityChanged;

    UPROPERTY(BlueprintAssignable, Category = "VRExpansionExtensions|UI Info")
    FVRExpUIInfoActorDestroyedEvent OnUIActorDestroyed;

#if WITH_EDITOR
    void SetEditorPreviewStatus(
        EVRExpUIInfoEditorPreviewStatus NewStatus,
        const FString &FailureReason);
#endif

protected:
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif

    virtual void OnInitialized(const FVRExpDetectableInteractionContext &InitialContext) override;
    virtual void OnActivated(const FVRExpDetectableInteractionContext &Context) override;
    virtual void OnActiveContextUpdated(const FVRExpDetectableInteractionContext &Context) override;
    virtual void OnDeactivated(const FVRExpDetectableInteractionContext &Context) override;
    virtual void OnDeinitialized(const FVRExpDetectableInteractionContext &FinalContext) override;
    virtual bool EvaluateSourceSwitches(
        const FVRExpDetectableInteractionContext &Context,
        EVRExpDetectableActivationMatchSource &OutMatchedSource,
        FString &OutFailureReason) const override;

private:
    using FResolvedAnchor = FVRExpUIInfoResolvedAnchor;

    struct FExitWidgetOpacityState
    {
        TWeakObjectPtr<UUserWidget> Widget;
        float OriginalOpacity = 1.0f;
    };

    void HandleActiveContext(const FVRExpDetectableInteractionContext &Context);
    UFUNCTION()
    void HandleHeadDetectionStarted(const FVRExpDetectableInteractionContext &Context);
    void BeginGracePeriod(const FVRExpDetectableInteractionContext &Context,
                          bool bRecordContextTransition);
    void RecordContextTransition(const FVRExpDetectableInteractionContext &Context);
    EVRExpUIInfoInteractionSource DetermineInteractionSource(
        const FVRExpDetectableInteractionContext &Context) const;
    bool IsHeadSourceEligible(
        const FVRExpDetectableInteractionContext &Context) const;
    bool IsGripSourceEligible(
        const FVRExpDetectableInteractionContext &Context) const;
    bool IsReleaseSourceEligible(
        const FVRExpDetectableInteractionContext &Context) const;
    bool IsMotionSourceEligible(
        const FVRExpDetectableInteractionContext &Context) const;
    uint64 GetActivationSequenceForSource(EVRExpUIInfoInteractionSource InteractionSource) const;
    bool IsSourceClaimEligible(EVRExpUIInfoInteractionSource InteractionSource) const;
    void SetSourceClaimEligible(EVRExpUIInfoInteractionSource InteractionSource, bool bEligible);

    bool EnsureUIActor(const FVRExpDetectableInteractionContext &Context);
    bool ResolveEntryWorldTransform(FTransform &OutTransform, FString &OutFailureReason);
    UFUNCTION()
    void HandleSpawnedUIActorDestroyed(AActor *DestroyedActor);
    void HandleExternalUIActorDestruction(AActor *DestroyedActor);
    void DestroyUIActor(const FVRExpDetectableInteractionContext &Context);

    void ApplyPresentation(EVRExpUIInfoInteractionSource NewSource,
                           const FVRExpDetectableInteractionContext &Context);
    void ApplyHiddenReleasePresentation(
        const FVRExpDetectableInteractionContext &Context);
    const FVRExpUIInfoPresentationSettings &GetPresentationSettings(
        EVRExpUIInfoInteractionSource InteractionSource) const;
    EVRExpUIInfoBindingMode GetEffectiveBindingMode() const;
    bool IsExclusivePresentation() const;
    int32 GetExclusivePriority() const;
    void ResetResolvedOnceTarget();

    bool ResolvePresentationTarget(FTransform &OutTargetWorldTransform,
                                   FResolvedAnchor &OutAnchor,
                                   FString &OutFailureReason);
    bool ResolveAnchor(FResolvedAnchor &OutAnchor, FString &OutFailureReason) const;
    bool ResolveComponentAnchor(const FComponentReference &ComponentReference,
                                FName SocketName,
                                FResolvedAnchor &OutAnchor,
                                FString &OutFailureReason) const;
    bool ApplyPositionMode(FTransform &InOutTargetWorldTransform,
                           const FResolvedAnchor &Anchor,
                           FString &OutFailureReason) const;
    bool ResolvePositionUpVector(const FResolvedAnchor &Anchor,
                                 FVector &OutUpVector,
                                 FString &OutFailureReason) const;
    bool ApplyOrientation(FTransform &InOutTargetWorldTransform,
                          const FResolvedAnchor &Anchor,
                          FString &OutFailureReason);
    bool ResolveFacingUpVector(const FResolvedAnchor &Anchor,
                               FVector &OutUpVector,
                               FString &OutFailureReason) const;
    bool BuildCameraFacingRotation(const FVector &TargetLocation,
                                   const FResolvedAnchor &Anchor,
                                   bool bPlanar,
                                   FQuat &OutRotation,
                                   FString &OutFailureReason);
    static FVector GetAxisVector(EVRExpUIInfoAxisDirection Axis);

    void ConfigureAttachment(const FResolvedAnchor &Anchor,
                             EVRExpUIInfoBindingMode BindingMode);
    void ApplySmoothedWorldTransform(const FTransform &TargetWorldTransform,
                                     float DeltaTime);
    bool IsTransformSettled(const FTransform &CurrentTransform,
                            const FTransform &TargetTransform) const;

    UCameraComponent *ResolveLocalCameraComponent() const;
    bool ResolveLocalViewTransform(FTransform &OutViewTransform) const;
    APlayerController *ResolveLocalPlayerController(
        const FVRExpDetectableInteractionContext &Context,
        EVRExpUIInfoInteractionSource InteractionSource) const;

    void UpdateExclusiveClaim(bool bEligible);
    void RemoveExclusiveClaim();
    void HandleExclusivePresentationGranted(bool bGranted);
    void HandleExclusiveClaimMadeIneligible();
    bool ShouldRestoreAfterPriorityLoss() const;

    void TickPresentation(float DeltaTime);
    bool RequiresPresentationTick() const;
    void RefreshTickRegistration();

    bool IsTimedHeadPresentationEnabled() const;
    bool ShouldEndTimedHeadPresentationOnDetectionLoss(
        const FVRExpDetectableInteractionContext &Context) const;
    bool ShouldAwaitFreshHeadDetectionAfterCooldown(
        bool bIsHeadDetected) const;
    FVRExpDetectableInteractionContext BuildRefreshedHeadContext(
        EVRExpDetectableChangePhase ChangePhase) const;
    bool ShouldCountHeadVisibleDuration(bool bHasValidUIActor) const;
    void StartHeadVisibleTimerIfNeeded();
    void RefreshHeadVisibleTimerCountingState();
    void EndTimedHeadPresentationState();
    void StartHeadCooldown();
    void CancelHeadVisibleTimer();
    void CancelHeadCooldownTimer();
    void CancelHeadTimingTimers();
    void HandleHeadVisibleTimerExpired();
    void HandleHeadCooldownTimerExpired();

    void StartInactivityTimers();
    void CancelInactivityTimers();
    void HandleHideTimerExpired();
    void HandleDestroyTimerExpired();
    bool ShouldAnimateExit(EVRExpUIInfoVisibilityReason VisibilityReason) const;
    float GetEffectiveExitTransitionDuration(
        EVRExpUIInfoVisibilityReason VisibilityReason) const;
    float EvaluateExitTransitionAlpha(float LinearAlpha) const;
    void StartExitTransition(EVRExpUIInfoVisibilityReason VisibilityReason);
    void ReverseExitTransition();
    void TickExitTransition(float DeltaTime);
    void CompleteExitTransition();
    void CacheExitWidgetOpacities();
    void ApplyExitTransitionVisuals(float ExitAlpha);
    void RestoreExitTransitionVisuals();
    void ResetExitTransitionState(bool bRestoreVisuals);
    void BroadcastExitTransitionUpdated(float ExitAlpha) const;
    void SetUIActorVisibleImmediately(
        bool bVisible, EVRExpUIInfoVisibilityReason VisibilityReason);
    void SetUIActorVisible(bool bVisible, EVRExpUIInfoVisibilityReason VisibilityReason);
    void BroadcastPresentationChanged();

    void UpdateRuntimeDebugState();
    void DrawRuntimeDebug();
    FColor GetRuntimeDebugColor() const;
    uint64 GetDebugScreenMessageKey() const;
    void ClearDebugScreenMessage() const;

    UWorld *GetOwningWorld() const;
    UVRExpUIInfoPresentationSubsystem *GetPresentationSubsystem() const;

    TWeakObjectPtr<AActor> SpawnedUIActor;
    TWeakObjectPtr<APlayerController> LocalPlayerController;
    TWeakObjectPtr<APlayerController> LastReleasedLocalPlayerController;
    FVRExpDetectableInteractionContext LastContext;
    FVRExpUIInfoPresentationSettings CurrentPresentationSettings;
    EVRExpUIInfoInteractionSource CurrentInteractionSource;

    FTransform CachedResolvedOnceBaseTransform;
    FResolvedAnchor CachedResolvedOnceAnchor;
    FQuat LastValidFacingRotation;
    FTransform ExitStartWorldTransform;
    FTransform ExitTargetWorldTransform;
    TArray<FExitWidgetOpacityState> ExitWidgetOpacityStates;
    EVRExpUIInfoVisibilityReason ExitVisibilityReason;
    float ExitTransitionLinearAlpha;

    uint64 HeadActivationSequence;
    uint64 GripActivationSequence;
    uint64 ReleaseActivationSequence;
    uint64 MotionActivationSequence;
    uint64 LastInteractionSequence;
    bool bHeadClaimEligible;
    bool bGripClaimEligible;
    bool bReleaseClaimEligible;
    bool bMotionClaimEligible;
    bool bGraceClaimEligible;
    bool bHasLocalInteraction;
    bool bUIActorVisible;
    bool bExclusivePresentationGranted;
    bool bPresentationTargetAvailable;
    bool bWasPriorityDisplaced;
    bool bHasSpawnedUIActor;
    bool bHasResolvedOnceTarget;
    bool bHasLastValidFacingRotation;
    bool bHasValidPresentationTransform;
    bool bTransformSettled;
    bool bAwaitingPresentationTarget;
    bool bHeadTimedPresentationActive;
    bool bHeadCooldownActive;
    bool bAwaitingFreshHeadDetection;
    bool bExitTransitionActive;
    bool bExitTransitionReversing;
    bool bDestroyAfterExitTransition;
    bool bTickRegistered;
    bool bDestroyingUIActorInternally;
    bool bHasWarnedMissingClass;
    bool bHasWarnedInvalidDestroyDelay;
    bool bHasWarnedMissingLocalPlayer;
    bool bHasWarnedInvalidBinding;
    bool bHasWarnedInvalidPositionBinding;
    bool bHasWarnedInvalidFacingAxes;

    FString LastPlacementFailureReason;

    FTimerHandle HideTimerHandle;
    FTimerHandle DestroyTimerHandle;
    FTimerHandle HeadVisibleTimerHandle;
    FTimerHandle HeadCooldownTimerHandle;

    friend class UVRExpUIInfoPresentationSubsystem;
#if WITH_DEV_AUTOMATION_TESTS
    friend class FVRExpUIHeadTimedPresentationTest;
    friend class FVRExpUIExitTransitionTest;
#endif
};
