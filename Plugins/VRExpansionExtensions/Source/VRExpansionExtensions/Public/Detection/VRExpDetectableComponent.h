#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Detection/VRExpDetectableTypes.h"
#include "VRExpDetectableComponent.generated.h"

class UChildActorComponent;
class UGripMotionControllerComponent;
class UPrimitiveComponent;
class USceneComponent;
class UVRExpDetectableComponent;
class UVRExpDetectableTriggerLogicBase;
class UVRExpGrabbableMotionComponent;
class UVRExpGripEventRouterSubsystem;
class UVRExpHeadDetectionComponent;

#if WITH_EDITOR
DECLARE_MULTICAST_DELEGATE_OneParam(
    FVRExpDetectableEditorUnregisterEvent,
    UVRExpDetectableComponent *);
#endif

UCLASS(Blueprintable, ClassGroup = (VRExpansionExtensions), meta = (BlueprintSpawnableComponent))
class VREXPANSIONEXTENSIONS_API UVRExpDetectableComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVRExpDetectableComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnUnregister() override;

#if WITH_EDITOR
    static FVRExpDetectableEditorUnregisterEvent
        OnEditorUnregister;

    UChildActorComponent *GetEditorPreviewChildActorComponent() const;
    void SetEditorPreviewChildActorComponent(
        UChildActorComponent *PreviewComponent);
    void DestroyEditorPreviewChildActorComponent();
#endif

    UFUNCTION(BlueprintPure, Category = "VRExpansionExtensions|Detection")
    bool IsHeadDetected() const { return bIsHeadDetected; }

    UFUNCTION(BlueprintPure, Category = "VRExpansionExtensions|Detection")
    bool IsGripped() const { return bIsGripped; }

    UFUNCTION(BlueprintPure, Category = "VRExpansionExtensions|Detection")
    bool IsInteracting() const { return bIsInteracting; }

    UFUNCTION(BlueprintCallable, Category = "VRExpansionExtensions|Detection")
    void RefreshGripSource();

    UFUNCTION(BlueprintPure, Category = "VRExpansionExtensions|Detection")
    EVRExpDetectableGripSource GetResolvedGripSource() const { return ResolvedGripSource; }

    UFUNCTION(BlueprintPure, Category = "VRExpansionExtensions|Detection")
    UVRExpGrabbableMotionComponent *GetResolvedGrabbableMotionComponent() const
    {
        return ResolvedGrabbableMotionComponent.Get();
    }

    UFUNCTION(BlueprintPure, Category = "VRExpansionExtensions|Detection")
    USceneComponent *GetResolvedMotionUpdatedComponent() const;

    UFUNCTION(BlueprintPure, Category = "VRExpansionExtensions|Detection",
              meta = (DeterminesOutputType = "TriggerLogicClass",
                      ToolTip = "Return the first valid Trigger Logic that is an instance of the requested class or one of its subclasses."))
    UVRExpDetectableTriggerLogicBase *GetTriggerLogicByClass(
        TSubclassOf<UVRExpDetectableTriggerLogicBase> TriggerLogicClass) const;

    bool AcceptsDetectedPrimitive(const UPrimitiveComponent *PrimitiveComponent) const;

    void NotifyHeadDetectionBegin(UVRExpHeadDetectionComponent *HeadDetector, UPrimitiveComponent *DetectedPrimitive);

    void NotifyHeadDetectionEnd(UVRExpHeadDetectionComponent *HeadDetector, UPrimitiveComponent *DetectedPrimitive);

    void NotifyGripBegin(UGripMotionControllerComponent *GripController,
                         const FBPActorGripInformation &GripInformation);

    void NotifyGripEnd(UGripMotionControllerComponent *GripController, const FBPActorGripInformation &GripInformation,
                       bool bWasSocketed);

    void GetGripTargets(TArray<UObject *> &OutGripTargets) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head Detection",
              meta = (DisplayName = "Head Primitive Mode",
                      ToolTip = "Select which primitives on the owner may be accepted by head detection."))
    EVRExpDetectablePrimitiveMode HeadPrimitiveMode;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head Detection",
              meta = (EditCondition = "HeadPrimitiveMode == EVRExpDetectablePrimitiveMode::ExplicitPrimitives",
                      EditConditionHides))
    TArray<TObjectPtr<UPrimitiveComponent>> ExplicitPrimitives;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip",
              meta = (DisplayName = "Grip Source Mode",
                      ToolTip = "Disable grip detection, resolve a motion source automatically, use direct detectable targets, or require a Grabbable Motion Component."))
    EVRExpDetectableGripSourceMode GripSourceMode;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip",
              meta = (DisplayName = "Grabbable Motion Source",
                      EditCondition = "GripSourceMode == EVRExpDetectableGripSourceMode::GrabbableMotionComponent",
                      EditConditionHides,
                      ToolTip = "Optional explicit Grabbable Motion Component on the detectable owner. Leave unset to use unique-component discovery."))
    FComponentReference GrabbableMotionSource;

    UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Trigger Logic")
    TArray<TObjectPtr<UVRExpDetectableTriggerLogicBase>> TriggerLogics;

    UPROPERTY(BlueprintAssignable, Category = "VRExpansionExtensions|Detection")
    FVRExpDetectableInteractionEvent OnHeadDetectionStarted;

    UPROPERTY(BlueprintAssignable, Category = "VRExpansionExtensions|Detection")
    FVRExpDetectableInteractionEvent OnHeadDetectionEnded;

    UPROPERTY(BlueprintAssignable, Category = "VRExpansionExtensions|Detection")
    FVRExpDetectableInteractionEvent OnGripStarted;

    UPROPERTY(BlueprintAssignable, Category = "VRExpansionExtensions|Detection")
    FVRExpDetectableInteractionEvent OnGripEnded;

    UPROPERTY(BlueprintAssignable, Category = "VRExpansionExtensions|Detection")
    FVRExpDetectableInteractionEvent OnInteractionStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "VRExpansionExtensions|Detection")
    FVRExpDetectableInteractionEvent OnInteractionContextUpdated;

protected:
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
    bool bIsHeadDetected;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
    bool bIsGripped;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
    bool bIsInteracting;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
    EVRExpDetectableGripSource ResolvedGripSource;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
    TObjectPtr<UVRExpGrabbableMotionComponent> ResolvedGrabbableMotionComponentForDebug;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
    int32 ActiveGripCount;

private:
    FVRExpDetectableInteractionContext
    MakeContext(EVRExpDetectableChangeSource ChangeSource, EVRExpDetectableChangePhase ChangePhase,
                UVRExpHeadDetectionComponent *HeadDetector, UPrimitiveComponent *DetectedPrimitive,
                UGripMotionControllerComponent *GripController, const FBPActorGripInformation *GripInformation,
                bool bWasSocketed, bool bChangedGripHasMovementAuthority = false) const;

    void EvaluateTriggerLogics(const FVRExpDetectableInteractionContext &Context);
    void InitializeTriggerLogicsAfterOwnerBeginPlay();
    void InitializeTriggerLogics();
    void DeinitializeTriggerLogics();
    void ClearGripState(bool bBroadcastStateChange);
    void ResolveGripSource(
        bool bRegisterWithRouter,
        const UVRExpGrabbableMotionComponent *IgnoredMotionComponent = nullptr);
    UVRExpGrabbableMotionComponent *ResolveConfiguredMotionSource(
        FString &OutFailureReason,
        const UVRExpGrabbableMotionComponent *IgnoredMotionComponent = nullptr) const;
    void BroadcastGripSourceResolution(
        bool bWasInteractingBeforeResolve);
    void BindMotionSource(UVRExpGrabbableMotionComponent *MotionComponent);
    void UnbindMotionSource();
    void RegisterDirectGripTargets();
    void UnregisterDirectGripTargets();
    void CollectDirectGripTargets(TArray<UObject *> &OutGripTargets) const;
    void ApplyGripTransition(
        EVRExpDetectableChangePhase ChangePhase,
        UGripMotionControllerComponent *GripController,
        const FBPActorGripInformation &GripInformation,
        bool bWasSocketed,
        bool bHasMovementAuthority);
    int32 FindActiveGripIndex(
        const UGripMotionControllerComponent *GripController,
        uint8 GripID) const;
    void PruneInvalidHeadDetectors();
    void PruneInvalidActiveGrips();
    void WarnIfExplicitPrimitiveListIsEmpty() const;

    UFUNCTION()
    void HandleMotionRuntimeSnapshotChanged(
        const FVRExpGrabbableMotionRuntimeSnapshot &Snapshot);

    UFUNCTION()
    void HandleMotionSourceInvalidated(
        UVRExpGrabbableMotionComponent *MotionComponent);

    TSet<TWeakObjectPtr<UVRExpHeadDetectionComponent>> ActiveHeadDetectors;

    UPROPERTY(Transient)
    TArray<FVRExpGrabbableActiveGrip> ActiveGrips;

    TWeakObjectPtr<UVRExpHeadDetectionComponent> LastHeadDetector;
    TWeakObjectPtr<UPrimitiveComponent> LastDetectedPrimitive;
    TWeakObjectPtr<UGripMotionControllerComponent> LastGripController;
    TWeakObjectPtr<UVRExpGrabbableMotionComponent> ResolvedGrabbableMotionComponent;
    FBPActorGripInformation LastGripInformation;

    UPROPERTY(Transient)
    FVRExpGrabbableMotionRuntimeSnapshot CachedMotionSnapshot;

#if WITH_EDITORONLY_DATA
    UPROPERTY(Transient, TextExportTransient, NonPIEDuplicateTransient)
    TObjectPtr<UChildActorComponent> EditorPreviewChildActorComponent;
#endif

    bool bLastGripWasSocketed;
    bool bLastGripHadMovementAuthority;
    bool bRegisteredDirectlyWithRouter;
    bool bTriggerLogicsInitialized;
    mutable bool bHasWarnedAboutEmptyExplicitPrimitives;
    mutable bool bHasWarnedAboutAmbiguousMotionSource;
    mutable bool bHasWarnedAboutMissingMotionSource;

    friend class UVRExpGripEventRouterSubsystem;
};
