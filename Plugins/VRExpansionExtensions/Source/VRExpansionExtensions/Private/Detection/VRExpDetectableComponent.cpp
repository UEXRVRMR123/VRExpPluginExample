#include "Detection/VRExpDetectableComponent.h"

#include "Components/ActorComponent.h"
#if WITH_EDITOR
#include "Components/ChildActorComponent.h"
#endif
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Detection/VRExpDetectableTriggerLogicBase.h"
#include "Detection/VRExpHeadDetectionComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GripMotionControllerComponent.h"
#include "Interaction/VRExpGrabbableMotionComponent.h"
#include "Interaction/VRExpGripEventRouterSubsystem.h"
#include "TimerManager.h"
#include "VRGripInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogVRExpDetectableComponent, Log, All);

#if WITH_EDITOR
FVRExpDetectableEditorUnregisterEvent
    UVRExpDetectableComponent::OnEditorUnregister;
#endif

UVRExpDetectableComponent::UVRExpDetectableComponent()
    : HeadPrimitiveMode(EVRExpDetectablePrimitiveMode::OwnerAnyPrimitive),
      GripSourceMode(EVRExpDetectableGripSourceMode::Auto),
      bIsHeadDetected(false),
      bIsGripped(false),
      bIsInteracting(false),
      ResolvedGripSource(EVRExpDetectableGripSource::None),
      ResolvedGrabbableMotionComponentForDebug(nullptr),
      ActiveGripCount(0),
      bLastGripWasSocketed(false),
      bLastGripHadMovementAuthority(false),
      bRegisteredDirectlyWithRouter(false),
      bTriggerLogicsInitialized(false),
      bHasWarnedAboutEmptyExplicitPrimitives(false),
      bHasWarnedAboutAmbiguousMotionSource(false),
      bHasWarnedAboutMissingMotionSource(false)
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UVRExpDetectableComponent::BeginPlay()
{
    Super::BeginPlay();

    WarnIfExplicitPrimitiveListIsEmpty();
    ResolveGripSource(false);

    // UActorComponent::BeginPlay runs before the owning Actor's Blueprint
    // BeginPlay. Defer initialization so Blueprint code can bind the Trigger
    // Logic's BlueprintAssignable delegates before the initial context emits.
    if (UWorld *World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick(
            this,
            &UVRExpDetectableComponent::InitializeTriggerLogicsAfterOwnerBeginPlay);
    }
    else
    {
        InitializeTriggerLogics();
    }

    if (ResolvedGripSource == EVRExpDetectableGripSource::DetectableTargets)
    {
        RegisterDirectGripTargets();
    }
    else if (
        ResolvedGripSource ==
        EVRExpDetectableGripSource::GrabbableMotionComponent)
    {
        BroadcastGripSourceResolution(false);
    }
}

void UVRExpDetectableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnregisterDirectGripTargets();
    UnbindMotionSource();

    ActiveHeadDetectors.Reset();
    ActiveGrips.Reset();
    ActiveGripCount = 0;
    bIsHeadDetected = false;
    bIsGripped = false;
    bIsInteracting = false;
    ResolvedGripSource = EVRExpDetectableGripSource::None;
    ResolvedGrabbableMotionComponentForDebug = nullptr;
    DeinitializeTriggerLogics();

    Super::EndPlay(EndPlayReason);
}

void UVRExpDetectableComponent::OnUnregister()
{
#if WITH_EDITOR
    OnEditorUnregister.Broadcast(this);
    DestroyEditorPreviewChildActorComponent();
#endif
    Super::OnUnregister();
}

#if WITH_EDITOR
UChildActorComponent *
UVRExpDetectableComponent::GetEditorPreviewChildActorComponent() const
{
    return EditorPreviewChildActorComponent;
}

void UVRExpDetectableComponent::SetEditorPreviewChildActorComponent(
    UChildActorComponent *PreviewComponent)
{
    if (EditorPreviewChildActorComponent == PreviewComponent)
    {
        return;
    }

    DestroyEditorPreviewChildActorComponent();
    EditorPreviewChildActorComponent = PreviewComponent;
}

void UVRExpDetectableComponent::DestroyEditorPreviewChildActorComponent()
{
    UChildActorComponent *PreviewComponent =
        EditorPreviewChildActorComponent.Get();
    EditorPreviewChildActorComponent = nullptr;
    if (IsValid(PreviewComponent))
    {
        PreviewComponent->DestroyComponent();
    }
}
#endif

void UVRExpDetectableComponent::RefreshGripSource()
{
    if (!HasBegunPlay())
    {
        return;
    }

    ClearGripState(true);
    const bool bWasInteractingBeforeResolve = bIsInteracting;
    ResolveGripSource(true);
    BroadcastGripSourceResolution(bWasInteractingBeforeResolve);
}

USceneComponent *UVRExpDetectableComponent::GetResolvedMotionUpdatedComponent() const
{
    return ResolvedGrabbableMotionComponent.IsValid()
               ? CachedMotionSnapshot.UpdatedComponent.Get()
               : nullptr;
}

UVRExpDetectableTriggerLogicBase *
UVRExpDetectableComponent::GetTriggerLogicByClass(
    TSubclassOf<UVRExpDetectableTriggerLogicBase> TriggerLogicClass) const
{
    const UClass *RequestedClass = TriggerLogicClass.Get();
    if (!IsValid(RequestedClass))
    {
        return nullptr;
    }

    for (UVRExpDetectableTriggerLogicBase *TriggerLogic : TriggerLogics)
    {
        if (IsValid(TriggerLogic) &&
            TriggerLogic->IsA(RequestedClass))
        {
            return TriggerLogic;
        }
    }

    return nullptr;
}

bool UVRExpDetectableComponent::AcceptsDetectedPrimitive(
    const UPrimitiveComponent *PrimitiveComponent) const
{
    const AActor *Owner = GetOwner();
    if (!Owner || !IsValid(PrimitiveComponent) ||
        PrimitiveComponent->GetOwner() != Owner)
    {
        return false;
    }

    switch (HeadPrimitiveMode)
    {
    case EVRExpDetectablePrimitiveMode::OwnerAnyPrimitive:
        return true;

    case EVRExpDetectablePrimitiveMode::OwnerRootPrimitive:
        return PrimitiveComponent ==
               Cast<UPrimitiveComponent>(Owner->GetRootComponent());

    case EVRExpDetectablePrimitiveMode::AutoGripInterfacePrimitive:
    {
        bool bHasGripInterfacePrimitive = false;
        TInlineComponentArray<UPrimitiveComponent *> PrimitiveComponents(Owner);
        for (const UPrimitiveComponent *Candidate : PrimitiveComponents)
        {
            if (IsValid(Candidate) &&
                Candidate->GetClass()->ImplementsInterface(
                    UVRGripInterface::StaticClass()))
            {
                bHasGripInterfacePrimitive = true;
                if (PrimitiveComponent == Candidate)
                {
                    return true;
                }
            }
        }

        if (!bHasGripInterfacePrimitive)
        {
            return PrimitiveComponent ==
                   Cast<UPrimitiveComponent>(Owner->GetRootComponent());
        }
        return false;
    }

    case EVRExpDetectablePrimitiveMode::ExplicitPrimitives:
        if (ExplicitPrimitives.IsEmpty())
        {
            WarnIfExplicitPrimitiveListIsEmpty();
            return false;
        }

        return ExplicitPrimitives.ContainsByPredicate(
            [PrimitiveComponent](
                const TObjectPtr<UPrimitiveComponent> &Candidate)
            {
                return Candidate.Get() == PrimitiveComponent;
            });

    default:
        return false;
    }
}

void UVRExpDetectableComponent::NotifyHeadDetectionBegin(
    UVRExpHeadDetectionComponent *HeadDetector,
    UPrimitiveComponent *DetectedPrimitive)
{
    if (!IsValid(HeadDetector) ||
        !AcceptsDetectedPrimitive(DetectedPrimitive))
    {
        return;
    }

    PruneInvalidHeadDetectors();
    const bool bWasHeadDetected = !ActiveHeadDetectors.IsEmpty();
    ActiveHeadDetectors.Add(HeadDetector);

    LastHeadDetector = HeadDetector;
    LastDetectedPrimitive = DetectedPrimitive;

    if (bWasHeadDetected)
    {
        return;
    }

    const bool bWasInteracting = bIsInteracting;
    bIsHeadDetected = true;
    bIsInteracting = bIsHeadDetected || bIsGripped;

    const FVRExpDetectableInteractionContext Context =
        MakeContext(
            EVRExpDetectableChangeSource::HeadDetection,
            EVRExpDetectableChangePhase::Began,
            HeadDetector,
            DetectedPrimitive,
            LastGripController.Get(),
            &LastGripInformation,
            bLastGripWasSocketed,
            bLastGripHadMovementAuthority);

    OnHeadDetectionStarted.Broadcast(Context);
    OnInteractionContextUpdated.Broadcast(Context);
    EvaluateTriggerLogics(Context);
    if (bWasInteracting != bIsInteracting)
    {
        OnInteractionStateChanged.Broadcast(Context);
    }
}

void UVRExpDetectableComponent::NotifyHeadDetectionEnd(
    UVRExpHeadDetectionComponent *HeadDetector,
    UPrimitiveComponent *DetectedPrimitive)
{
    if (!HeadDetector)
    {
        return;
    }

    PruneInvalidHeadDetectors();
    if (ActiveHeadDetectors.Remove(HeadDetector) == 0 ||
        !ActiveHeadDetectors.IsEmpty())
    {
        return;
    }

    LastHeadDetector = HeadDetector;
    LastDetectedPrimitive = DetectedPrimitive;

    const bool bWasInteracting = bIsInteracting;
    bIsHeadDetected = false;
    bIsInteracting = bIsHeadDetected || bIsGripped;

    const FVRExpDetectableInteractionContext Context =
        MakeContext(
            EVRExpDetectableChangeSource::HeadDetection,
            EVRExpDetectableChangePhase::Ended,
            HeadDetector,
            DetectedPrimitive,
            LastGripController.Get(),
            &LastGripInformation,
            bLastGripWasSocketed,
            bLastGripHadMovementAuthority);

    OnHeadDetectionEnded.Broadcast(Context);
    OnInteractionContextUpdated.Broadcast(Context);
    EvaluateTriggerLogics(Context);
    if (bWasInteracting != bIsInteracting)
    {
        OnInteractionStateChanged.Broadcast(Context);
    }
}

void UVRExpDetectableComponent::NotifyGripBegin(
    UGripMotionControllerComponent *GripController,
    const FBPActorGripInformation &GripInformation)
{
    if (ResolvedGripSource != EVRExpDetectableGripSource::DetectableTargets ||
        !IsValid(GripController) ||
        GripInformation.GripID == INVALID_VRGRIP_ID)
    {
        return;
    }

    ApplyGripTransition(
        EVRExpDetectableChangePhase::Began,
        GripController,
        GripInformation,
        false,
        GripController->HasGripMovementAuthority(GripInformation));
}

void UVRExpDetectableComponent::NotifyGripEnd(
    UGripMotionControllerComponent *GripController,
    const FBPActorGripInformation &GripInformation,
    bool bWasSocketed)
{
    if (ResolvedGripSource != EVRExpDetectableGripSource::DetectableTargets ||
        GripInformation.GripID == INVALID_VRGRIP_ID)
    {
        return;
    }

    ApplyGripTransition(
        EVRExpDetectableChangePhase::Ended,
        GripController,
        GripInformation,
        bWasSocketed,
        false);
}

void UVRExpDetectableComponent::GetGripTargets(
    TArray<UObject *> &OutGripTargets) const
{
    OutGripTargets.Reset();

    if (ResolvedGripSource != EVRExpDetectableGripSource::DetectableTargets)
    {
        return;
    }

    CollectDirectGripTargets(OutGripTargets);
}

void UVRExpDetectableComponent::CollectDirectGripTargets(
    TArray<UObject *> &OutGripTargets) const
{
    OutGripTargets.Reset();

    AActor *Owner = GetOwner();
    if (!IsValid(Owner))
    {
        return;
    }

    if (Owner->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
    {
        OutGripTargets.Add(Owner);
    }

    TInlineComponentArray<UActorComponent *> OwnerComponents(Owner);
    for (UActorComponent *OwnerComponent : OwnerComponents)
    {
        if (IsValid(OwnerComponent) &&
            OwnerComponent->GetClass()->ImplementsInterface(
                UVRGripInterface::StaticClass()))
        {
            OutGripTargets.AddUnique(OwnerComponent);
        }
    }
}

FVRExpDetectableInteractionContext UVRExpDetectableComponent::MakeContext(
    EVRExpDetectableChangeSource ChangeSource,
    EVRExpDetectableChangePhase ChangePhase,
    UVRExpHeadDetectionComponent *HeadDetector,
    UPrimitiveComponent *DetectedPrimitive,
    UGripMotionControllerComponent *GripController,
    const FBPActorGripInformation *GripInformation,
    bool bWasSocketed,
    bool bChangedGripHasMovementAuthority) const
{
    FVRExpDetectableInteractionContext Context;
    Context.bIsHeadDetected = bIsHeadDetected;
    Context.bIsGripped = bIsGripped;
    Context.bIsInteracting = bIsInteracting;
    Context.ChangeSource = ChangeSource;
    Context.ChangePhase = ChangePhase;
    Context.DetectableComponent =
        const_cast<UVRExpDetectableComponent *>(this);
    Context.HeadDetector = HeadDetector;
    Context.DetectedPrimitive = DetectedPrimitive;
    Context.GripController = GripController;
    if (GripInformation)
    {
        Context.GripInformation = *GripInformation;
    }
    Context.bWasSocketed = bWasSocketed;
    Context.bChangedGripHasMovementAuthority =
        bChangedGripHasMovementAuthority;
    Context.GripSource = ResolvedGripSource;
    Context.ActiveGrips = ActiveGrips;
    Context.ActiveGripCount = ActiveGrips.Num();
    Context.GrabbableMotionComponent =
        CachedMotionSnapshot.MotionComponent.Get();
    Context.MotionUpdatedComponent =
        GetResolvedMotionUpdatedComponent();
    Context.MotionSnapshot = CachedMotionSnapshot;
    Context.MotionState = CachedMotionSnapshot.MotionState;
    Context.MotionPhase =
        IsValid(Context.GrabbableMotionComponent.Get())
            ? CachedMotionSnapshot.MotionPhase
            : EVRExpGrabbableMotionPhase::Unavailable;
    Context.PreviousMotionPhase =
        IsValid(Context.GrabbableMotionComponent.Get())
            ? CachedMotionSnapshot.PreviousMotionPhase
            : EVRExpGrabbableMotionPhase::Unavailable;
    Context.NormalMotionMode = CachedMotionSnapshot.NormalMotionMode;
    Context.ReleaseMotionMode = CachedMotionSnapshot.ReleaseMotionMode;
    Context.MotionChangeFlags = CachedMotionSnapshot.ChangeFlags;
    Context.MotionSnapshotSequence = CachedMotionSnapshot.Sequence;
    Context.bHasAnyMovementAuthority =
        ActiveGrips.ContainsByPredicate(
            [](const FVRExpGrabbableActiveGrip &ActiveGrip)
            {
                return ActiveGrip.bHasMovementAuthority;
            });
    return Context;
}

void UVRExpDetectableComponent::EvaluateTriggerLogics(
    const FVRExpDetectableInteractionContext &Context)
{
    for (UVRExpDetectableTriggerLogicBase *TriggerLogic : TriggerLogics)
    {
        if (IsValid(TriggerLogic))
        {
            TriggerLogic->EvaluateContext(Context);
        }
    }
}

void UVRExpDetectableComponent::InitializeTriggerLogicsAfterOwnerBeginPlay()
{
    if (!HasBegunPlay() || bTriggerLogicsInitialized)
    {
        return;
    }

    AActor *Owner = GetOwner();
    if (IsValid(Owner) && !Owner->HasActorBegunPlay())
    {
        if (UWorld *World = GetWorld())
        {
            World->GetTimerManager().SetTimerForNextTick(
                this,
                &UVRExpDetectableComponent::InitializeTriggerLogicsAfterOwnerBeginPlay);
        }
        return;
    }

    InitializeTriggerLogics();
}

void UVRExpDetectableComponent::InitializeTriggerLogics()
{
    if (bTriggerLogicsInitialized)
    {
        return;
    }

    bTriggerLogicsInitialized = true;
    const FVRExpDetectableInteractionContext InitialContext =
        MakeContext(
            EVRExpDetectableChangeSource::None,
            EVRExpDetectableChangePhase::None,
            nullptr,
            nullptr,
            LastGripController.Get(),
            &LastGripInformation,
            bLastGripWasSocketed,
            bLastGripHadMovementAuthority);

    for (UVRExpDetectableTriggerLogicBase *TriggerLogic : TriggerLogics)
    {
        if (IsValid(TriggerLogic))
        {
            TriggerLogic->Initialize(this, InitialContext);
        }
    }
}

void UVRExpDetectableComponent::DeinitializeTriggerLogics()
{
    if (!bTriggerLogicsInitialized)
    {
        return;
    }

    const FVRExpDetectableInteractionContext FinalContext =
        MakeContext(
            EVRExpDetectableChangeSource::None,
            EVRExpDetectableChangePhase::None,
            nullptr,
            nullptr,
            LastGripController.Get(),
            &LastGripInformation,
            bLastGripWasSocketed,
            bLastGripHadMovementAuthority);

    for (UVRExpDetectableTriggerLogicBase *TriggerLogic : TriggerLogics)
    {
        if (IsValid(TriggerLogic))
        {
            TriggerLogic->Deinitialize(FinalContext);
        }
    }

    bTriggerLogicsInitialized = false;
}

void UVRExpDetectableComponent::ClearGripState(bool bBroadcastStateChange)
{
    if (ActiveGrips.IsEmpty() && !bIsGripped)
    {
        return;
    }

    ActiveGrips.Reset();
    ActiveGripCount = 0;
    if (ResolvedGripSource ==
        EVRExpDetectableGripSource::GrabbableMotionComponent)
    {
        CachedMotionSnapshot =
            FVRExpGrabbableMotionRuntimeSnapshot();
    }
    const bool bWasInteracting = bIsInteracting;
    bIsGripped = false;
    bIsInteracting = bIsHeadDetected;

    const FVRExpDetectableInteractionContext Context =
        MakeContext(
            EVRExpDetectableChangeSource::Grip,
            EVRExpDetectableChangePhase::Ended,
            LastHeadDetector.Get(),
            LastDetectedPrimitive.Get(),
            LastGripController.Get(),
            &LastGripInformation,
            bLastGripWasSocketed,
            bLastGripHadMovementAuthority);

    if (bBroadcastStateChange)
    {
        OnGripEnded.Broadcast(Context);
        OnInteractionContextUpdated.Broadcast(Context);
    }
    EvaluateTriggerLogics(Context);
    if (bBroadcastStateChange && bWasInteracting != bIsInteracting)
    {
        OnInteractionStateChanged.Broadcast(Context);
    }
}

void UVRExpDetectableComponent::ResolveGripSource(
    bool bRegisterWithRouter,
    const UVRExpGrabbableMotionComponent *IgnoredMotionComponent)
{
    UnregisterDirectGripTargets();
    UnbindMotionSource();

    ActiveGrips.Reset();
    ActiveGripCount = 0;
    bIsGripped = false;
    bIsInteracting = bIsHeadDetected;
    ResolvedGripSource = EVRExpDetectableGripSource::None;
    ResolvedGrabbableMotionComponentForDebug = nullptr;
    CachedMotionSnapshot = FVRExpGrabbableMotionRuntimeSnapshot();

    if (GripSourceMode == EVRExpDetectableGripSourceMode::Disabled)
    {
        return;
    }

    if (GripSourceMode == EVRExpDetectableGripSourceMode::DetectableTargets)
    {
        TArray<UObject *> DirectGripTargets;
        CollectDirectGripTargets(DirectGripTargets);
        if (DirectGripTargets.IsEmpty())
        {
            return;
        }

        ResolvedGripSource = EVRExpDetectableGripSource::DetectableTargets;
        if (bRegisterWithRouter)
        {
            RegisterDirectGripTargets();
        }
        return;
    }

    FString MotionFailureReason;
    UVRExpGrabbableMotionComponent *MotionComponent =
        ResolveConfiguredMotionSource(
            MotionFailureReason,
            IgnoredMotionComponent);
    if (IsValid(MotionComponent))
    {
        BindMotionSource(MotionComponent);
        return;
    }

    const bool bAmbiguousSource =
        MotionFailureReason.StartsWith(TEXT("Multiple"));
    if (bAmbiguousSource)
    {
        if (!bHasWarnedAboutAmbiguousMotionSource)
        {
            bHasWarnedAboutAmbiguousMotionSource = true;
            UE_LOG(
                LogVRExpDetectableComponent,
                Warning,
                TEXT("%s cannot resolve an authoritative grip source: %s"),
                *GetPathName(),
                *MotionFailureReason);
        }
        return;
    }

    if (GripSourceMode ==
        EVRExpDetectableGripSourceMode::GrabbableMotionComponent)
    {
        if (!bHasWarnedAboutMissingMotionSource)
        {
            bHasWarnedAboutMissingMotionSource = true;
            UE_LOG(
                LogVRExpDetectableComponent,
                Warning,
                TEXT("%s requires a Grabbable Motion Component, but none could be resolved."),
                *GetPathName());
        }
        return;
    }

    TArray<UObject *> DirectGripTargets;
    CollectDirectGripTargets(DirectGripTargets);
    ResolvedGripSource = ResolveVRExpAutomaticGripSource(
        0,
        !DirectGripTargets.IsEmpty());
    if (ResolvedGripSource ==
        EVRExpDetectableGripSource::None)
    {
        return;
    }

    if (bRegisterWithRouter)
    {
        RegisterDirectGripTargets();
    }
}

UVRExpGrabbableMotionComponent *
UVRExpDetectableComponent::ResolveConfiguredMotionSource(
    FString &OutFailureReason,
    const UVRExpGrabbableMotionComponent *IgnoredMotionComponent) const
{
    OutFailureReason.Reset();
    AActor *Owner = GetOwner();
    if (!IsValid(Owner))
    {
        OutFailureReason = TEXT("The detectable owner is unavailable.");
        return nullptr;
    }

    if (GripSourceMode ==
        EVRExpDetectableGripSourceMode::GrabbableMotionComponent)
    {
        if (UActorComponent *ReferencedComponent =
                GrabbableMotionSource.GetComponent(Owner))
        {
            if (UVRExpGrabbableMotionComponent *ReferencedMotion =
                    Cast<UVRExpGrabbableMotionComponent>(
                        ReferencedComponent))
            {
                if (ReferencedMotion != IgnoredMotionComponent &&
                    IsValid(ReferencedMotion))
                {
                    return ReferencedMotion;
                }
            }
        }
    }

    TInlineComponentArray<UVRExpGrabbableMotionComponent *> MotionComponents(
        Owner);
    MotionComponents.RemoveAll(
        [IgnoredMotionComponent](
            const UVRExpGrabbableMotionComponent *MotionComponent)
        {
            return !IsValid(MotionComponent) ||
                   MotionComponent == IgnoredMotionComponent;
        });

    if (MotionComponents.Num() == 1)
    {
        return MotionComponents[0];
    }

    if (MotionComponents.Num() > 1)
    {
        if (GripSourceMode == EVRExpDetectableGripSourceMode::Auto)
        {
            OutFailureReason = FString::Printf(
                TEXT("Multiple Grabbable Motion Components were found on %s; "
                     "switch Grip Source Mode to Grabbable Motion Component "
                     "and configure Grabbable Motion Source explicitly."),
                *GetNameSafe(Owner));
        }
        else
        {
            OutFailureReason = FString::Printf(
                TEXT("Multiple Grabbable Motion Components were found on %s; "
                     "configure Grabbable Motion Source explicitly."),
                *GetNameSafe(Owner));
        }
    }
    else
    {
        OutFailureReason =
            FString::Printf(
                TEXT("No Grabbable Motion Component was found on %s."),
                *GetNameSafe(Owner));
    }
    return nullptr;
}

void UVRExpDetectableComponent::BroadcastGripSourceResolution(
    bool bWasInteractingBeforeResolve)
{
    const bool bDirectGripWasAlreadyBroadcast =
        ResolvedGripSource ==
            EVRExpDetectableGripSource::DetectableTargets &&
        bIsGripped;
    if (bDirectGripWasAlreadyBroadcast)
    {
        return;
    }

    const FVRExpDetectableInteractionContext Context =
        MakeContext(
            EVRExpDetectableChangeSource::MotionState,
            EVRExpDetectableChangePhase::Updated,
            LastHeadDetector.Get(),
            LastDetectedPrimitive.Get(),
            LastGripController.Get(),
            &LastGripInformation,
            bLastGripWasSocketed,
            bLastGripHadMovementAuthority);

    if (ResolvedGripSource ==
            EVRExpDetectableGripSource::GrabbableMotionComponent &&
        bIsGripped)
    {
        OnGripStarted.Broadcast(Context);
    }

    OnInteractionContextUpdated.Broadcast(Context);
    EvaluateTriggerLogics(Context);
    if (bWasInteractingBeforeResolve != bIsInteracting)
    {
        OnInteractionStateChanged.Broadcast(Context);
    }
}

void UVRExpDetectableComponent::BindMotionSource(
    UVRExpGrabbableMotionComponent *MotionComponent)
{
    if (!IsValid(MotionComponent))
    {
        return;
    }

    ResolvedGrabbableMotionComponent = MotionComponent;
    ResolvedGrabbableMotionComponentForDebug = MotionComponent;
    ResolvedGripSource =
        EVRExpDetectableGripSource::GrabbableMotionComponent;
    CachedMotionSnapshot = MotionComponent->GetRuntimeSnapshot();

    MotionComponent->OnRuntimeSnapshotChanged.AddUniqueDynamic(
        this,
        &UVRExpDetectableComponent::HandleMotionRuntimeSnapshotChanged);
    MotionComponent->OnMotionSourceInvalidated.AddUniqueDynamic(
        this,
        &UVRExpDetectableComponent::HandleMotionSourceInvalidated);

    ActiveGrips = CachedMotionSnapshot.ActiveGrips;
    ActiveGripCount = CachedMotionSnapshot.ActiveGripCount;
    bIsGripped = CachedMotionSnapshot.bIsActivelyGripped;
    bIsInteracting = bIsHeadDetected || bIsGripped;

    if (!ActiveGrips.IsEmpty())
    {
        const FVRExpGrabbableActiveGrip &LastGrip = ActiveGrips.Last();
        LastGripController = LastGrip.GripController;
        LastGripInformation = LastGrip.GripInformation;
        bLastGripHadMovementAuthority =
            LastGrip.bHasMovementAuthority;
        bLastGripWasSocketed = false;
    }
}

void UVRExpDetectableComponent::UnbindMotionSource()
{
    if (ResolvedGrabbableMotionComponent.IsValid())
    {
        ResolvedGrabbableMotionComponent->OnRuntimeSnapshotChanged.RemoveDynamic(
            this,
            &UVRExpDetectableComponent::HandleMotionRuntimeSnapshotChanged);
        ResolvedGrabbableMotionComponent->OnMotionSourceInvalidated.RemoveDynamic(
            this,
            &UVRExpDetectableComponent::HandleMotionSourceInvalidated);
    }

    ResolvedGrabbableMotionComponent.Reset();
    ResolvedGrabbableMotionComponentForDebug = nullptr;
    CachedMotionSnapshot = FVRExpGrabbableMotionRuntimeSnapshot();
}

void UVRExpDetectableComponent::RegisterDirectGripTargets()
{
    if (bRegisteredDirectlyWithRouter ||
        ResolvedGripSource !=
            EVRExpDetectableGripSource::DetectableTargets)
    {
        return;
    }

    TArray<UObject *> DirectGripTargets;
    CollectDirectGripTargets(DirectGripTargets);
    if (DirectGripTargets.IsEmpty())
    {
        ResolvedGripSource = EVRExpDetectableGripSource::None;
        return;
    }

    if (UWorld *World = GetWorld())
    {
        if (UVRExpGripEventRouterSubsystem *Router =
                World->GetSubsystem<UVRExpGripEventRouterSubsystem>())
        {
            Router->RegisterDetectable(this);
            bRegisteredDirectlyWithRouter = true;
        }
    }
}

void UVRExpDetectableComponent::UnregisterDirectGripTargets()
{
    if (!bRegisteredDirectlyWithRouter)
    {
        return;
    }

    if (UWorld *World = GetWorld())
    {
        if (UVRExpGripEventRouterSubsystem *Router =
                World->GetSubsystem<UVRExpGripEventRouterSubsystem>())
        {
            Router->UnregisterDetectable(this);
        }
    }
    bRegisteredDirectlyWithRouter = false;
}

void UVRExpDetectableComponent::ApplyGripTransition(
    EVRExpDetectableChangePhase ChangePhase,
    UGripMotionControllerComponent *GripController,
    const FBPActorGripInformation &GripInformation,
    bool bWasSocketed,
    bool bHasMovementAuthority)
{
    PruneInvalidActiveGrips();
    const bool bWasGripped = !ActiveGrips.IsEmpty();
    const bool bWasInteracting = bIsInteracting;

    if (ChangePhase == EVRExpDetectableChangePhase::Began)
    {
        if (FindActiveGripIndex(
                GripController,
                GripInformation.GripID) != INDEX_NONE)
        {
            return;
        }

        FVRExpGrabbableActiveGrip ActiveGrip;
        ActiveGrip.GripController = GripController;
        ActiveGrip.GripInformation = GripInformation;
        ActiveGrip.bHasMovementAuthority = bHasMovementAuthority;
        ActiveGrips.Add(ActiveGrip);
    }
    else if (ChangePhase == EVRExpDetectableChangePhase::Ended)
    {
        const int32 ActiveGripIndex =
            FindActiveGripIndex(GripController, GripInformation.GripID);
        if (ActiveGripIndex == INDEX_NONE)
        {
            return;
        }
        bHasMovementAuthority =
            ActiveGrips[ActiveGripIndex].bHasMovementAuthority;
        ActiveGrips.RemoveAt(ActiveGripIndex);
    }
    else
    {
        return;
    }

    LastGripController = GripController;
    LastGripInformation = GripInformation;
    bLastGripWasSocketed = bWasSocketed;
    bLastGripHadMovementAuthority = bHasMovementAuthority;
    ActiveGripCount = ActiveGrips.Num();
    bIsGripped = !ActiveGrips.IsEmpty();
    bIsInteracting = bIsHeadDetected || bIsGripped;

    const FVRExpDetectableInteractionContext Context =
        MakeContext(
            EVRExpDetectableChangeSource::Grip,
            ChangePhase,
            LastHeadDetector.Get(),
            LastDetectedPrimitive.Get(),
            GripController,
            &GripInformation,
            bWasSocketed,
            bHasMovementAuthority);

    if (!bWasGripped && bIsGripped)
    {
        OnGripStarted.Broadcast(Context);
    }
    else if (bWasGripped && !bIsGripped)
    {
        OnGripEnded.Broadcast(Context);
    }

    OnInteractionContextUpdated.Broadcast(Context);
    EvaluateTriggerLogics(Context);
    if (bWasInteracting != bIsInteracting)
    {
        OnInteractionStateChanged.Broadcast(Context);
    }
}

int32 UVRExpDetectableComponent::FindActiveGripIndex(
    const UGripMotionControllerComponent *GripController,
    uint8 GripID) const
{
    return ActiveGrips.IndexOfByPredicate(
        [GripController, GripID](
            const FVRExpGrabbableActiveGrip &ActiveGrip)
        {
            const bool bControllerMatches =
                GripController
                    ? ActiveGrip.GripController == GripController
                    : !IsValid(ActiveGrip.GripController);
            return bControllerMatches &&
                   ActiveGrip.GripInformation.GripID == GripID;
        });
}

void UVRExpDetectableComponent::HandleMotionRuntimeSnapshotChanged(
    const FVRExpGrabbableMotionRuntimeSnapshot &Snapshot)
{
    if (!ResolvedGrabbableMotionComponent.IsValid() ||
        Snapshot.MotionComponent != ResolvedGrabbableMotionComponent.Get())
    {
        return;
    }

    const bool bWasGripped = bIsGripped;
    const bool bWasInteracting = bIsInteracting;
    CachedMotionSnapshot = Snapshot;
    ActiveGrips = Snapshot.ActiveGrips;
    ActiveGripCount = Snapshot.ActiveGripCount;
    bIsGripped = Snapshot.bIsActivelyGripped;
    bIsInteracting = bIsHeadDetected || bIsGripped;

    const bool bGripChanged = EnumHasAnyFlags(
        Snapshot.ChangeFlags,
        EVRExpGrabbableMotionChangeFlags::Grip);
    const FVRExpGrabbableActiveGrip &ChangedGrip =
        Snapshot.ChangedGrip;
    if (bGripChanged)
    {
        LastGripController = ChangedGrip.GripController;
        LastGripInformation = ChangedGrip.GripInformation;
        bLastGripHadMovementAuthority =
            ChangedGrip.bHasMovementAuthority;
    }
    bLastGripWasSocketed = Snapshot.bWasSocketed;

    const EVRExpDetectableChangePhase ChangePhase =
        Snapshot.GripChangePhase ==
                EVRExpGrabbableGripChangePhase::Began
            ? EVRExpDetectableChangePhase::Began
            : Snapshot.GripChangePhase ==
                      EVRExpGrabbableGripChangePhase::Ended
                  ? EVRExpDetectableChangePhase::Ended
                  : EVRExpDetectableChangePhase::Updated;
    UGripMotionControllerComponent *ContextGripController =
        bGripChanged
            ? ChangedGrip.GripController.Get()
            : nullptr;
    const FBPActorGripInformation *ContextGripInformation =
        bGripChanged
            ? &ChangedGrip.GripInformation
            : nullptr;

    const FVRExpDetectableInteractionContext Context =
        MakeContext(
            bGripChanged
                ? EVRExpDetectableChangeSource::Grip
                : EVRExpDetectableChangeSource::MotionState,
            ChangePhase,
            LastHeadDetector.Get(),
            LastDetectedPrimitive.Get(),
            ContextGripController,
            ContextGripInformation,
            bGripChanged && Snapshot.bWasSocketed,
            bGripChanged &&
                ChangedGrip.bHasMovementAuthority);

    if (!bWasGripped && bIsGripped)
    {
        OnGripStarted.Broadcast(Context);
    }
    else if (bWasGripped && !bIsGripped)
    {
        OnGripEnded.Broadcast(Context);
    }

    OnInteractionContextUpdated.Broadcast(Context);
    EvaluateTriggerLogics(Context);
    if (bWasInteracting != bIsInteracting)
    {
        OnInteractionStateChanged.Broadcast(Context);
    }
}

void UVRExpDetectableComponent::HandleMotionSourceInvalidated(
    UVRExpGrabbableMotionComponent *MotionComponent)
{
    if (MotionComponent != ResolvedGrabbableMotionComponent.Get())
    {
        return;
    }

    AActor *Owner = GetOwner();
    if (!HasBegunPlay() ||
        (IsValid(Owner) && Owner->IsActorBeingDestroyed()))
    {
        UnbindMotionSource();
        return;
    }

    ClearGripState(true);
    const bool bWasInteractingBeforeResolve = bIsInteracting;
    ResolveGripSource(true, MotionComponent);
    BroadcastGripSourceResolution(bWasInteractingBeforeResolve);
}

void UVRExpDetectableComponent::PruneInvalidHeadDetectors()
{
    for (auto Iterator = ActiveHeadDetectors.CreateIterator();
         Iterator;
         ++Iterator)
    {
        if (!Iterator->IsValid())
        {
            Iterator.RemoveCurrent();
        }
    }

    bIsHeadDetected = !ActiveHeadDetectors.IsEmpty();
    bIsInteracting = bIsHeadDetected || bIsGripped;
}

void UVRExpDetectableComponent::PruneInvalidActiveGrips()
{
    for (int32 Index = ActiveGrips.Num() - 1; Index >= 0; --Index)
    {
        if (!IsValid(ActiveGrips[Index].GripController))
        {
            ActiveGrips.RemoveAt(Index);
        }
    }

    ActiveGripCount = ActiveGrips.Num();
    bIsGripped = !ActiveGrips.IsEmpty();
    bIsInteracting = bIsHeadDetected || bIsGripped;
}

void UVRExpDetectableComponent::WarnIfExplicitPrimitiveListIsEmpty() const
{
    if (HeadPrimitiveMode !=
            EVRExpDetectablePrimitiveMode::ExplicitPrimitives ||
        !ExplicitPrimitives.IsEmpty() ||
        bHasWarnedAboutEmptyExplicitPrimitives)
    {
        return;
    }

    bHasWarnedAboutEmptyExplicitPrimitives = true;
    UE_LOG(
        LogVRExpDetectableComponent,
        Warning,
        TEXT("%s uses ExplicitPrimitives but has no primitive component configured; head detection is disabled for this component."),
        *GetPathName());
}
