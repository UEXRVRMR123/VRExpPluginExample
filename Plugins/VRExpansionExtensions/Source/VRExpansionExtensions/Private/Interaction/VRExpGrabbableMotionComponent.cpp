// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/VRExpGrabbableMotionComponent.h"

#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interaction/VRExpGripEventRouterSubsystem.h"
#include "Math/RotationMatrix.h"

DEFINE_LOG_CATEGORY_STATIC(LogVRExpGrabbableMotion, Log, All);

struct FVRExpScopedMotionRuntimeMutation
{
	explicit FVRExpScopedMotionRuntimeMutation(
		UVRExpGrabbableMotionComponent& InComponent)
		: Component(InComponent)
	{
		Component.BeginRuntimeMutation();
	}

	~FVRExpScopedMotionRuntimeMutation()
	{
		Component.EndRuntimeMutation();
	}

	UVRExpGrabbableMotionComponent& Component;
};

UVRExpGrabbableMotionComponent::UVRExpGrabbableMotionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NormalMotionMode(EVRExpGrabbableNormalMotionMode::None)
	, ReleaseMotionMode(EVRExpGrabbableReleaseMotionMode::None)
	, bAutoStartNormalMotion(true)
	, bPauseNormalMotionWhenGrabbed(true)
	, bUpdateRotationDuringMotion(true)
	, AttachedMotionDirectionMode(EVRExpGrabbableAttachedMotionDirectionMode::PreserveWorldDirections)
	, bSweepMovement(false)
	, bStopOnBlockingHit(true)
	, bDisablePhysicsDuringKinematicMotion(true)
	, AttachedFallWithGravityMode(EVRExpGrabbableAttachedFallWithGravityMode::RelativeKinematic)
	, AttachedKinematicFallCollisionMode(EVRExpGrabbableAttachedKinematicFallCollisionMode::SweepAndStop)
	, GripControllerScope(EVRExpGripControllerScope::AllWorldControllers)
	, bMatchOwnerActor(true)
	, bMatchUpdatedComponent(true)
	, bMatchOwnerComponents(true)
	, bUseManualSplineReference(false)
	, bUseSplineActor(true)
	, SplineActor(nullptr)
	, SplineComponentName(NAME_None)
	, SplineToFollow(nullptr)
	, bTeleportToSplineOnStart(true)
	, bUseKinematicGravityOnSplineStart(false)
	, SplineSpeed(100.0f)
	, SplineArrivalThreshold(10.0f)
	, bLoopSpline(true)
	, bReverseDirection(false)
	, TargetToFollow(nullptr)
	, FollowSpeed(200.0f)
	, FollowDistance(100.0f)
	, bOrientToTarget(true)
	, OrbitCenter(nullptr)
	, OrbitRadius(300.0f)
	, OrbitSpeed(50.0f)
	, OrbitAxis(FVector::UpVector)
	, OrbitHeightOffset(0.0f)
	, WanderRadius(500.0f)
	, WanderSpeed(150.0f)
	, WanderChangeInterval(3.0f)
	, bTeleportToSplineOnRelease(false)
	, bUseKinematicGravityOnSplineRelease(false)
	, SplineFallMode(EVRExpGrabbableSplineFallMode::VerticalThenApproach)
	, SplineAboveTargetMode(EVRExpGrabbableSplineAboveTargetMode::DirectApproach)
	, KinematicGravitySource(EVRExpGrabbableKinematicGravitySource::WorldGravity)
	, WorldGravityScale(1.0f)
	, CustomGravityAcceleration(980.0f)
	, MaxFallSpeed(4000.0f)
	, bOrientDuringSplineHeightMotion(false)
	, ReturnSpeed(500.0f)
	, bSmoothReturn(true)
	, ReturnCurve(nullptr)
	, FlyToTargetComponent(nullptr)
	, bReattachOwnerToInitialParentComponentOnRelease(false)
	, bAttachOwnerToSplineOnRelease(false)
	, CurrentMotionState(EVRExpGrabbableMotionState::Idle)
	, MotionStateBeforePause(EVRExpGrabbableMotionState::Idle)
	, InitialTransform(FTransform::Identity)
	, CurrentMotionParentSocketName(NAME_None)
	, CurrentMotionParentWorldTransform(FTransform::Identity)
	, bCurrentMotionSpaceIsRelative(false)
	, InitialAttachSocketName(NAME_None)
	, InitialOwnerRelativeTransform(FTransform::Identity)
	, InitialOwnerRelativeToSplineTransform(FTransform::Identity)
	, WanderOrigin(FVector::ZeroVector)
	, CurrentWanderTarget(FVector::ZeroVector)
	, ReturnStartLocation(FVector::ZeroVector)
	, ReturnStartRotation(FRotator::ZeroRotator)
	, SplineApproachTargetLocation(FVector::ZeroVector)
	, SplineApproachTargetProgress(0.0f)
	, SplineApproachFallSpeed(0.0f)
	, AttachedKinematicFallSpeed(0.0f)
	, SplineApproachPhase(ESplineApproachPhase::None)
	, ReturnProgress(0.0f)
	, ReturnTotalDistance(0.0f)
	, CurrentSplineProgress(0.0f)
	, CurrentOrbitAngle(0.0f)
	, WanderTimer(0.0f)
	, FloatingEffectPhase(0.0f)
	, AppliedFloatingOffset(FVector::ZeroVector)
	, AccumulatedRotationEffect(FQuat::Identity)
	, AppliedRotationEffect(FQuat::Identity)
	, AppliedRotationEffectSpace(EVRExpGrabbableMotionEffectSpace::Local)
	, CachedFloatingEffectSpace(EVRExpGrabbableMotionEffectSpace::World)
	, CachedRotationEffectSpace(EVRExpGrabbableMotionEffectSpace::Local)
	, bIsMovingToSpline(false)
	, bMovementAppliedThisTick(false)
	, bFloatingEffectWasEnabled(false)
	, bRotationEffectWasActive(false)
	, bWarnedAboutInvalidOrientationAxes(false)
	, bWarnedAboutInvalidFloatingAxis(false)
	, bHasInitialParentAttachment(false)
	, bHasInitialRelativeToSplineTransform(false)
	, bHasPendingReleaseAttachment(false)
	, bAttachedKinematicFallActive(false)
	, bPreservePendingReleaseAttachmentDuringMotionStart(false)
	, PendingReleaseAttachmentMode(EVRExpGrabbableReleaseMotionMode::None)
	, bPendingAttachOwnerToSpline(false)
	, bPendingReattachOwnerToInitialParent(false)
	, bGripRegistrationInitialized(false)
	, bRefreshingGripRegistration(false)
	, RuntimeMutationDepth(0)
	, RuntimeSnapshotSequence(0)
	, PendingRuntimeChangeFlags(EVRExpGrabbableMotionChangeFlags::None)
	, PendingGripChangePhase(EVRExpGrabbableGripChangePhase::None)
	, bPendingGripWasSocketed(false)
	, bPendingGripIsFirst(false)
	, bPendingGripIsFinal(false)
	, bPendingLegacyGripBroadcast(false)
	, bPendingLegacyMotionStateBroadcast(false)
	, bWarnedAboutSweepWithoutPrimitive(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bAutoActivate = false;
}

void UVRExpGrabbableMotionComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveUpdatedComponent();
	RefreshCurrentMotionSpace(true);
	InitialTransform = GetUpdatedComponentTransform();
	InitialMotionAnchor = CaptureMotionAnchor(InitialTransform);
	WanderOriginAnchor = InitialMotionAnchor;
	WanderTargetAnchor = InitialMotionAnchor;
	WanderOrigin = ConvertWorldLocationToMotionSpace(
		InitialTransform.GetLocation());
	CurrentWanderTarget = WanderOrigin;
	CaptureInitialReleaseAttachmentState();

	{
		FVRExpScopedMotionRuntimeMutation Mutation(*this);
		if (UWorld* World = GetWorld())
		{
			if (UVRExpGripEventRouterSubsystem* Router =
					World->GetSubsystem<UVRExpGripEventRouterSubsystem>())
			{
				MarkRuntimeChange(
					EVRExpGrabbableMotionChangeFlags::RegistrationRebuilt);
				Router->RegisterMotionComponent(this);
			}
		}
	}
	LastGripRegisteredUpdatedComponent =
		IsValid(UpdatedComponent) ? UpdatedComponent : nullptr;
	bGripRegistrationInitialized = true;

	if (ActiveGrips.IsEmpty() || !bPauseNormalMotionWhenGrabbed)
	{
		if (bAutoStartNormalMotion &&
		(NormalMotionMode != EVRExpGrabbableNormalMotionMode::None || HasConfiguredMotionEffects()))
		{
			StartNormalMotion();
		}
		else
		{
			SetComponentTickEnabled(false);
		}
	}
	else
	{
		SetComponentTickEnabled(false);
	}
}

void UVRExpGrabbableMotionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bGripRegistrationInitialized = false;
	if (UWorld* World = GetWorld())
	{
		if (UVRExpGripEventRouterSubsystem* Router =
				World->GetSubsystem<UVRExpGripEventRouterSubsystem>())
		{
			Router->UnregisterMotionComponent(this);
		}
	}

	{
		FVRExpScopedMotionRuntimeMutation Mutation(*this);
		if (!ActiveGrips.IsEmpty())
		{
			const FVRExpGrabbableActiveGrip ChangedGrip =
				ActiveGrips.Last();
			ActiveGrips.Reset();
			RecordGripRuntimeChange(
				ChangedGrip,
				EVRExpGrabbableGripChangePhase::Ended,
				false,
				false,
				true);
		}
		CancelPendingReleaseAttachment();
		ResetSplineApproach();
		bIsMovingToSpline = false;
		SetMotionState(EVRExpGrabbableMotionState::Idle);
	}

	OnMotionSourceInvalidated.Broadcast(this);
	ActiveGrips.Reset();
	LastGripRegisteredUpdatedComponent.Reset();
	ClearMotionTickPrerequisites();
	CurrentMotionParentComponent.Reset();
	CurrentMotionParentSocketName = NAME_None;
	CurrentMotionParentWorldTransform = FTransform::Identity;
	bCurrentMotionSpaceIsRelative = false;
	InitialOwnerRootComponent.Reset();
	InitialAttachParentComponent.Reset();
	InitialSplineAttachmentComponent.Reset();
	SetComponentTickEnabled(false);

	Super::EndPlay(EndPlayReason);
}

void UVRExpGrabbableMotionComponent::SetUpdatedComponent(
	USceneComponent* NewUpdatedComponent)
{
	if (UpdatedComponent == NewUpdatedComponent)
	{
		return;
	}

	FVRExpScopedMotionRuntimeMutation Mutation(*this);
	ClearMotionTickPrerequisites();
	BakeMotionEffectsIntoBase();
	Super::SetUpdatedComponent(NewUpdatedComponent);
	CurrentMotionParentComponent.Reset();
	CurrentMotionParentSocketName = NAME_None;
	CurrentMotionParentWorldTransform = FTransform::Identity;
	bCurrentMotionSpaceIsRelative = false;
	RefreshCurrentMotionSpace(true);
	MarkRuntimeChange(
		EVRExpGrabbableMotionChangeFlags::UpdatedComponent);
	if (bGripRegistrationInitialized &&
		!bRefreshingGripRegistration &&
		HasBegunPlay())
	{
		RefreshGripRegistration();
	}
}

void UVRExpGrabbableMotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(UpdatedComponent))
	{
		return;
	}

	RefreshCurrentMotionSpace();
	RebuildMotionTickPrerequisites();
	ResolveMotionAnchorWorldTransform(InitialMotionAnchor);
	ResolveMotionAnchorWorldTransform(WanderOriginAnchor);
	ResolveMotionAnchorWorldTransform(WanderTargetAnchor);
	PruneInvalidActiveGrips();
	bMovementAppliedThisTick = false;
	RefreshMotionEffectRuntimeState();
	AdvanceMotionEffects(DeltaTime);

	switch (CurrentMotionState)
	{
	case EVRExpGrabbableMotionState::NormalMotion:
		TickNormalMotion(DeltaTime);
		break;
	case EVRExpGrabbableMotionState::Releasing:
		TickReleaseMotion(DeltaTime);
		break;
	default:
		break;
	}

	if (IsMotionEffectState(CurrentMotionState) && HasActiveMotionEffects() && !bMovementAppliedThisTick)
	{
		MoveUpdatedComponentTo(GetMotionBaseLocation(), GetMotionBaseQuat(), DeltaTime);
	}
}

void UVRExpGrabbableMotionComponent::SetNormalMotionMode(EVRExpGrabbableNormalMotionMode NewMode)
{
	if (NormalMotionMode == NewMode)
	{
		return;
	}

	FVRExpScopedMotionRuntimeMutation Mutation(*this);
	NormalMotionMode = NewMode;
	MarkRuntimeChange(
		EVRExpGrabbableMotionChangeFlags::NormalMotionMode);

	if (CurrentMotionState == EVRExpGrabbableMotionState::NormalMotion)
	{
		StartNormalMotion();
	}
}

void UVRExpGrabbableMotionComponent::SetReleaseMotionMode(EVRExpGrabbableReleaseMotionMode NewMode)
{
	if (ReleaseMotionMode == NewMode)
	{
		return;
	}

	FVRExpScopedMotionRuntimeMutation Mutation(*this);
	ReleaseMotionMode = NewMode;
	MarkRuntimeChange(
		EVRExpGrabbableMotionChangeFlags::ReleaseMotionMode);
}

void UVRExpGrabbableMotionComponent::StartMotion()
{
	if (ActiveGrips.Num() > 0 && bPauseNormalMotionWhenGrabbed)
	{
		return;
	}

	StartNormalMotion();
}

void UVRExpGrabbableMotionComponent::StopMotion()
{
	if (!bPreservePendingReleaseAttachmentDuringMotionStart)
	{
		CancelPendingReleaseAttachment();
	}
	ResetSplineApproach();
	bIsMovingToSpline = false;
	bAttachedKinematicFallActive = false;
	AttachedKinematicFallSpeed = 0.0f;
	Velocity = FVector::ZeroVector;
	UpdateComponentVelocity();
	SetMotionState(EVRExpGrabbableMotionState::Idle);
	SetComponentTickEnabled(false);
	ClearMotionTickPrerequisites();
}

void UVRExpGrabbableMotionComponent::PauseMotion()
{
	if (CurrentMotionState == EVRExpGrabbableMotionState::NormalMotion ||
		CurrentMotionState == EVRExpGrabbableMotionState::Releasing)
	{
		MotionStateBeforePause = CurrentMotionState;
		SetMotionState(EVRExpGrabbableMotionState::Paused);
		SetComponentTickEnabled(false);
	}
}

void UVRExpGrabbableMotionComponent::ResumeMotion()
{
	if (CurrentMotionState != EVRExpGrabbableMotionState::Paused)
	{
		return;
	}

	if (MotionStateBeforePause == EVRExpGrabbableMotionState::Releasing)
	{
		SetMotionState(EVRExpGrabbableMotionState::Releasing);
		SetComponentTickEnabled(true);
	}
	else
	{
		StartNormalMotion();
	}
}

void UVRExpGrabbableMotionComponent::RefreshGripRegistration()
{
	FVRExpScopedMotionRuntimeMutation Mutation(*this);
	TGuardValue<bool> RefreshGuard(
		bRefreshingGripRegistration,
		true);
	MarkRuntimeChange(
		EVRExpGrabbableMotionChangeFlags::RegistrationRebuilt);
	ResolveUpdatedComponent();
	if (!bGripRegistrationInitialized || !HasBegunPlay())
	{
		return;
	}

	const bool bWasActivelyGripped = !ActiveGrips.IsEmpty();
	ActiveGrips.Reset();

	if (UWorld* World = GetWorld())
	{
		if (UVRExpGripEventRouterSubsystem* Router =
				World->GetSubsystem<UVRExpGripEventRouterSubsystem>())
		{
			Router->RefreshMotionComponent(this);
		}
	}
	LastGripRegisteredUpdatedComponent =
		IsValid(UpdatedComponent) ? UpdatedComponent : nullptr;

	RecordGripRuntimeChange(
		FVRExpGrabbableActiveGrip(),
		EVRExpGrabbableGripChangePhase::None,
		false,
		false,
		false);

	if (bWasActivelyGripped && ActiveGrips.IsEmpty() &&
		CurrentMotionState == EVRExpGrabbableMotionState::Grabbed)
	{
		if (bAutoStartNormalMotion &&
			(NormalMotionMode != EVRExpGrabbableNormalMotionMode::None ||
				HasConfiguredMotionEffects()))
		{
			StartNormalMotion();
		}
		else
		{
			StopMotion();
		}
	}
}

void UVRExpGrabbableMotionComponent::ResolveUpdatedComponent()
{
	if (IsValid(UpdatedComponent))
	{
		return;
	}

	if (AActor* Owner = GetOwner())
	{
		USceneComponent* OwnerRoot = Owner->GetRootComponent();
		SetUpdatedComponent(IsValid(OwnerRoot) ? OwnerRoot : nullptr);
	}
}

FVRExpGrabbableGripSnapshot UVRExpGrabbableMotionComponent::GetGripSnapshot() const
{
	return MakeGripSnapshot(
		FVRExpGrabbableActiveGrip(),
		EVRExpGrabbableGripChangePhase::None,
		false,
		false,
		false);
}

FVRExpGrabbableMotionRuntimeSnapshot
UVRExpGrabbableMotionComponent::GetRuntimeSnapshot() const
{
	return MakeRuntimeSnapshot(
		nullptr,
		EVRExpGrabbableMotionChangeFlags::None,
		FVRExpGrabbableActiveGrip(),
		EVRExpGrabbableGripChangePhase::None,
		false,
		RuntimeSnapshotSequence);
}

void UVRExpGrabbableMotionComponent::GetGripRoutingTargets(TArray<UObject*>& OutGripTargets) const
{
	OutGripTargets.Reset();

	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	if (bMatchOwnerActor)
	{
		OutGripTargets.Add(Owner);
	}

	if (bMatchUpdatedComponent && IsValid(UpdatedComponent))
	{
		OutGripTargets.AddUnique(UpdatedComponent);
	}

	if (bMatchOwnerComponents)
	{
		TInlineComponentArray<UActorComponent*> OwnerComponents(Owner);
		for (UActorComponent* OwnerComponent : OwnerComponents)
		{
			if (IsValid(OwnerComponent))
			{
				OutGripTargets.AddUnique(OwnerComponent);
			}
		}
	}
}

bool UVRExpGrabbableMotionComponent::AcceptsGripController(
	const UGripMotionControllerComponent* GripController) const
{
	if (!IsValid(GripController))
	{
		return false;
	}

	if (GripControllerScope == EVRExpGripControllerScope::AllWorldControllers)
	{
		return GripController->GetWorld() == GetWorld();
	}

	return ManualGripControllers.ContainsByPredicate(
		[GripController](const TObjectPtr<UGripMotionControllerComponent>& Candidate)
		{
			return Candidate.Get() == GripController;
		});
}

bool UVRExpGrabbableMotionComponent::HasGripRegistrationTargetChanged() const
{
	const USceneComponent* CurrentUpdatedComponent =
		IsValid(UpdatedComponent) ? UpdatedComponent : nullptr;
	return LastGripRegisteredUpdatedComponent.IsStale() ||
		LastGripRegisteredUpdatedComponent.Get() !=
			CurrentUpdatedComponent;
}

bool UVRExpGrabbableMotionComponent::DoesGripMatchTarget(const FBPActorGripInformation& GripInformation) const
{
	if (!GripInformation.IsValid())
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	AActor* GrippedActor = GripInformation.GetGrippedActor();
	UPrimitiveComponent* GrippedComponent = GripInformation.GetGrippedComponent();

	if (bMatchOwnerActor && GrippedActor == Owner)
	{
		return true;
	}

	if (bMatchUpdatedComponent && IsValid(UpdatedComponent))
	{
		if (GrippedComponent == UpdatedComponent || GripInformation.GrippedObject == UpdatedComponent)
		{
			return true;
		}
	}

	if (bMatchOwnerComponents)
	{
		if (GrippedComponent && GrippedComponent->GetOwner() == Owner)
		{
			return true;
		}

		if (UActorComponent* GrippedActorComponent = Cast<UActorComponent>(GripInformation.GrippedObject))
		{
			return GrippedActorComponent->GetOwner() == Owner;
		}
	}

	return false;
}

void UVRExpGrabbableMotionComponent::NotifyRoutedGripBegin(
	UGripMotionControllerComponent* GripController,
	const FBPActorGripInformation& GripInformation)
{
	if (!IsValid(GripController) ||
		!GripInformation.IsValid() ||
		!AcceptsGripController(GripController) ||
		!DoesGripMatchTarget(GripInformation) ||
		FindActiveGripIndex(GripController, GripInformation.GripID) != INDEX_NONE)
	{
		return;
	}

	FVRExpScopedMotionRuntimeMutation Mutation(*this);
	const bool bIsFirstGrip = ActiveGrips.IsEmpty();
	CancelPendingReleaseAttachment();
	FVRExpGrabbableActiveGrip NewActiveGrip;
	NewActiveGrip.GripController = GripController;
	NewActiveGrip.GripInformation = GripInformation;
	NewActiveGrip.bHasMovementAuthority =
		GripController->HasGripMovementAuthority(GripInformation);
	ActiveGrips.Add(NewActiveGrip);

	if (bPauseNormalMotionWhenGrabbed)
	{
		ResetSplineApproach();
		bIsMovingToSpline = false;
		bAttachedKinematicFallActive = false;
		AttachedKinematicFallSpeed = 0.0f;
		SetMotionState(
			EVRExpGrabbableMotionState::Grabbed);
		SetComponentTickEnabled(false);
	}

	RecordGripRuntimeChange(
		NewActiveGrip,
		EVRExpGrabbableGripChangePhase::Began,
		false,
		bIsFirstGrip,
		false);
}

void UVRExpGrabbableMotionComponent::NotifyRoutedGripEnd(
	UGripMotionControllerComponent* GripController,
	const FBPActorGripInformation& GripInformation,
	bool bWasSocketed)
{
	const int32 ActiveGripIndex =
		FindActiveGripIndex(GripController, GripInformation.GripID);
	if (ActiveGripIndex == INDEX_NONE)
	{
		return;
	}

	bool bHadMovementAuthority = false;
	for (const FVRExpGrabbableActiveGrip& ActiveGrip : ActiveGrips)
	{
		bHadMovementAuthority =
			bHadMovementAuthority || ActiveGrip.bHasMovementAuthority;
	}

	bool bIsFinalGrip = false;
	{
		FVRExpScopedMotionRuntimeMutation Mutation(*this);
		FVRExpGrabbableActiveGrip ChangedGrip =
			ActiveGrips[ActiveGripIndex];
		ChangedGrip.GripInformation = GripInformation;
		ActiveGrips.RemoveAt(ActiveGripIndex);
		bIsFinalGrip = ActiveGrips.IsEmpty();

		if (ShouldStartReleaseMotionFromGripEnd(
				bIsFinalGrip,
				bWasSocketed,
				bHadMovementAuthority))
		{
			const EVRExpGrabbableReleaseMotionMode ReleasedMode =
				ReleaseMotionMode;
			PrepareReleaseAttachmentBeforeMotion(ReleasedMode);
			bPreservePendingReleaseAttachmentDuringMotionStart = true;
			StartReleaseMotionFromDrop();
			bPreservePendingReleaseAttachmentDuringMotionStart = false;
			FinalizeReleaseAttachmentAfterMotionStart();
		}
		else if (bIsFinalGrip)
		{
			StopMotion();
		}

		RecordGripRuntimeChange(
			ChangedGrip,
			EVRExpGrabbableGripChangePhase::Ended,
			bWasSocketed,
			false,
			bIsFinalGrip);

		FVRExpPendingGripReleasedEvent& PendingRelease =
			PendingGripReleasedEvents.AddDefaulted_GetRef();
		PendingRelease.GripInformation = GripInformation;
		PendingRelease.bWasSocketed = bWasSocketed;
		PendingRelease.bIsFinalGrip = bIsFinalGrip;
		PendingRelease.bHadMovementAuthority =
			bHadMovementAuthority;
	}
}

bool UVRExpGrabbableMotionComponent::ShouldStartReleaseMotionFromGripEnd(
	bool bIsFinalGrip,
	bool bWasSocketed,
	bool bHadMovementAuthority)
{
	return bIsFinalGrip && !bWasSocketed && bHadMovementAuthority;
}

int32 UVRExpGrabbableMotionComponent::FindActiveGripIndex(
	const UGripMotionControllerComponent* GripController,
	uint8 GripID) const
{
	return ActiveGrips.IndexOfByPredicate(
		[GripController, GripID](const FVRExpGrabbableActiveGrip& ActiveGrip)
		{
			const bool bControllerMatches =
				GripController
					? ActiveGrip.GripController == GripController
					: !IsValid(ActiveGrip.GripController);
			return bControllerMatches &&
				ActiveGrip.GripInformation.GripID == GripID;
		});
}

FVRExpGrabbableGripSnapshot UVRExpGrabbableMotionComponent::MakeGripSnapshot(
	const FVRExpGrabbableActiveGrip& ChangedGrip,
	EVRExpGrabbableGripChangePhase ChangePhase,
	bool bWasSocketed,
	bool bIsFirstGrip,
	bool bIsFinalGrip) const
{
	FVRExpGrabbableGripSnapshot Snapshot;
	Snapshot.ActiveGrips = ActiveGrips;
	Snapshot.ChangedGrip = ChangedGrip;
	Snapshot.ChangePhase = ChangePhase;
	Snapshot.bWasSocketed = bWasSocketed;
	Snapshot.bIsFirstGrip = bIsFirstGrip;
	Snapshot.bIsFinalGrip = bIsFinalGrip;
	return Snapshot;
}

FVRExpGrabbableMotionRuntimeSnapshot
UVRExpGrabbableMotionComponent::MakeRuntimeSnapshot(
	const FVRExpGrabbableMotionRuntimeSnapshot* PreviousSnapshot,
	EVRExpGrabbableMotionChangeFlags ChangeFlags,
	const FVRExpGrabbableActiveGrip& ChangedGrip,
	EVRExpGrabbableGripChangePhase GripChangePhase,
	bool bWasSocketed,
	int64 Sequence) const
{
	FVRExpGrabbableMotionRuntimeSnapshot Snapshot;
	Snapshot.MotionComponent =
		const_cast<UVRExpGrabbableMotionComponent*>(this);
	Snapshot.UpdatedComponent = UpdatedComponent;
	Snapshot.MotionState = CurrentMotionState;
	Snapshot.PreviousMotionState =
		PreviousSnapshot
			? PreviousSnapshot->MotionState
			: CurrentMotionState;
	Snapshot.MotionPhase = ResolveVRExpGrabbableMotionPhase(
		CurrentMotionState,
		ActiveGrips.Num());
	Snapshot.PreviousMotionPhase =
		PreviousSnapshot
			? PreviousSnapshot->MotionPhase
			: Snapshot.MotionPhase;
	Snapshot.NormalMotionMode = NormalMotionMode;
	Snapshot.PreviousNormalMotionMode =
		PreviousSnapshot
			? PreviousSnapshot->NormalMotionMode
			: NormalMotionMode;
	Snapshot.ReleaseMotionMode = ReleaseMotionMode;
	Snapshot.PreviousReleaseMotionMode =
		PreviousSnapshot
			? PreviousSnapshot->ReleaseMotionMode
			: ReleaseMotionMode;
	Snapshot.ActiveGrips = ActiveGrips;
	Snapshot.ActiveGripCount = ActiveGrips.Num();
	Snapshot.bIsActivelyGripped = !ActiveGrips.IsEmpty();
	Snapshot.ChangedGrip = ChangedGrip;
	Snapshot.GripChangePhase = GripChangePhase;
	Snapshot.bWasSocketed = bWasSocketed;
	Snapshot.ChangeFlags = ChangeFlags;
	Snapshot.Sequence = Sequence;

	for (const FVRExpGrabbableActiveGrip& ActiveGrip : ActiveGrips)
	{
		Snapshot.bHasAnyMovementAuthority =
			Snapshot.bHasAnyMovementAuthority ||
			ActiveGrip.bHasMovementAuthority;
	}

	return Snapshot;
}

void UVRExpGrabbableMotionComponent::BeginRuntimeMutation()
{
	if (RuntimeMutationDepth++ > 0)
	{
		return;
	}

	RuntimeMutationStartSnapshot = GetRuntimeSnapshot();
	PendingRuntimeChangeFlags =
		EVRExpGrabbableMotionChangeFlags::None;
	PendingChangedGrip = FVRExpGrabbableActiveGrip();
	PendingGripChangePhase =
		EVRExpGrabbableGripChangePhase::None;
	bPendingGripWasSocketed = false;
	bPendingGripIsFirst = false;
	bPendingGripIsFinal = false;
	bPendingLegacyGripBroadcast = false;
	bPendingLegacyMotionStateBroadcast = false;
	PendingGripReleasedEvents.Reset();
}

void UVRExpGrabbableMotionComponent::EndRuntimeMutation()
{
	check(RuntimeMutationDepth > 0);
	if (--RuntimeMutationDepth > 0)
	{
		return;
	}

	if (PendingRuntimeChangeFlags ==
		EVRExpGrabbableMotionChangeFlags::None)
	{
		return;
	}

	++RuntimeSnapshotSequence;
	const FVRExpGrabbableMotionRuntimeSnapshot RuntimeSnapshot =
		MakeRuntimeSnapshot(
			&RuntimeMutationStartSnapshot,
			PendingRuntimeChangeFlags,
			PendingChangedGrip,
			PendingGripChangePhase,
			bPendingGripWasSocketed,
			RuntimeSnapshotSequence);
	const bool bBroadcastLegacyGrip =
		bPendingLegacyGripBroadcast;
	const bool bBroadcastLegacyMotionState =
		bPendingLegacyMotionStateBroadcast;
	const FVRExpGrabbableGripSnapshot GripSnapshot =
		MakeGripSnapshot(
			PendingChangedGrip,
			PendingGripChangePhase,
			bPendingGripWasSocketed,
			bPendingGripIsFirst,
			bPendingGripIsFinal);
	const TArray<FVRExpPendingGripReleasedEvent>
		GripReleasedEvents = PendingGripReleasedEvents;

	PendingRuntimeChangeFlags =
		EVRExpGrabbableMotionChangeFlags::None;
	PendingChangedGrip = FVRExpGrabbableActiveGrip();
	PendingGripChangePhase =
		EVRExpGrabbableGripChangePhase::None;
	bPendingGripWasSocketed = false;
	bPendingGripIsFirst = false;
	bPendingGripIsFinal = false;
	bPendingLegacyGripBroadcast = false;
	bPendingLegacyMotionStateBroadcast = false;
	PendingGripReleasedEvents.Reset();

	OnRuntimeSnapshotChanged.Broadcast(RuntimeSnapshot);
	if (bBroadcastLegacyGrip)
	{
		OnGripStateChanged.Broadcast(GripSnapshot);
	}
	if (bBroadcastLegacyMotionState)
	{
		OnMotionStateChanged.Broadcast(RuntimeSnapshot.MotionState);
	}
	for (const FVRExpPendingGripReleasedEvent& ReleasedEvent :
		 GripReleasedEvents)
	{
		// 蓝图释放逻辑最后执行，避免内置 ReleaseMotionMode 被外部逻辑覆盖。
		OnGripReleased.Broadcast(
			ReleasedEvent.GripInformation,
			ReleasedEvent.bWasSocketed,
			ReleasedEvent.bIsFinalGrip,
			ReleasedEvent.bHadMovementAuthority);
	}
}

void UVRExpGrabbableMotionComponent::MarkRuntimeChange(
	EVRExpGrabbableMotionChangeFlags ChangeFlags)
{
	check(RuntimeMutationDepth > 0);
	PendingRuntimeChangeFlags |= ChangeFlags;
}

void UVRExpGrabbableMotionComponent::RecordGripRuntimeChange(
	const FVRExpGrabbableActiveGrip& ChangedGrip,
	EVRExpGrabbableGripChangePhase ChangePhase,
	bool bWasSocketed,
	bool bIsFirstGrip,
	bool bIsFinalGrip)
{
	MarkRuntimeChange(EVRExpGrabbableMotionChangeFlags::Grip);
	PendingChangedGrip = ChangedGrip;
	PendingGripChangePhase = ChangePhase;
	bPendingGripWasSocketed = bWasSocketed;
	bPendingGripIsFirst = bIsFirstGrip;
	bPendingGripIsFinal = bIsFinalGrip;
	bPendingLegacyGripBroadcast = true;
}

void UVRExpGrabbableMotionComponent::PruneInvalidActiveGrips()
{
	FVRExpScopedMotionRuntimeMutation Mutation(*this);
	bool bRemovedGrip = false;
	FVRExpGrabbableActiveGrip LastRemovedGrip;
	for (int32 Index = ActiveGrips.Num() - 1; Index >= 0; --Index)
	{
		if (!IsValid(ActiveGrips[Index].GripController))
		{
			LastRemovedGrip = ActiveGrips[Index];
			ActiveGrips.RemoveAt(Index);
			bRemovedGrip = true;
		}
	}

	if (!bRemovedGrip)
	{
		return;
	}

	const bool bIsFinalGrip = ActiveGrips.IsEmpty();
	if (bIsFinalGrip &&
		CurrentMotionState == EVRExpGrabbableMotionState::Grabbed)
	{
		StopMotion();
	}

	RecordGripRuntimeChange(
		LastRemovedGrip,
		EVRExpGrabbableGripChangePhase::Ended,
		false,
		false,
		bIsFinalGrip);
}

void UVRExpGrabbableMotionComponent::SetMotionState(
	EVRExpGrabbableMotionState NewState,
	bool bBroadcastChange)
{
	if (CurrentMotionState == NewState)
	{
		return;
	}

	FVRExpScopedMotionRuntimeMutation Mutation(*this);
	const bool bWasEffectState = IsMotionEffectState(CurrentMotionState);
	const bool bWillBeEffectState = IsMotionEffectState(NewState);
	if (bWasEffectState != bWillBeEffectState)
	{
		BakeMotionEffectsIntoBase();
	}

	CurrentMotionState = NewState;
	MarkRuntimeChange(
		EVRExpGrabbableMotionChangeFlags::MotionState);
	if (bBroadcastChange)
	{
		bPendingLegacyMotionStateBroadcast = true;
	}
}

void UVRExpGrabbableMotionComponent::StartNormalMotion()
{
	FVRExpScopedMotionRuntimeMutation Mutation(*this);
	ResetSplineApproach();
	bIsMovingToSpline = false;
	bAttachedKinematicFallActive = false;
	AttachedKinematicFallSpeed = 0.0f;
	ResolveUpdatedComponent();
	if (!IsValid(UpdatedComponent))
	{
		StopMotion();
		return;
	}

	RefreshCurrentMotionSpace();
	RebuildMotionTickPrerequisites();
	RefreshMotionEffectRuntimeState();

	if (NormalMotionMode == EVRExpGrabbableNormalMotionMode::None && !HasConfiguredMotionEffects())
	{
		StopMotion();
		return;
	}

	PrepareForKinematicMotion();

	switch (NormalMotionMode)
	{
	case EVRExpGrabbableNormalMotionMode::FollowSpline:
		if (!InitializeSplineReference())
		{
			UE_LOG(LogVRExpGrabbableMotion, Warning, TEXT("VRExpGrabbableMotionComponent: no valid spline component was found."));
			StopMotion();
			return;
		}
		RebuildMotionTickPrerequisites();

		if (bTeleportToSplineOnStart)
		{
			CurrentSplineProgress = bReverseDirection ? SplineToFollow->GetSplineLength() : 0.0f;
			const FVector StartLocation = ConvertWorldLocationToMotionSpace(
				SplineToFollow->GetLocationAtDistanceAlongSpline(
					CurrentSplineProgress,
					ESplineCoordinateSpace::World));
			const FQuat StartRotation = GetSplineMotionRotation(CurrentSplineProgress);
			MoveUpdatedComponentTo(StartLocation, StartRotation, 0.0f, true, ETeleportType::TeleportPhysics);
			bIsMovingToSpline = false;
		}
		else
		{
			bIsMovingToSpline = true;
			const FVector CurrentLocation = GetMotionBaseLocation();
			if (!CaptureSplineApproachTarget(CurrentLocation))
			{
				StopMotion();
				return;
			}
			if (bUseKinematicGravityOnSplineStart)
			{
				InitializeKinematicSplineApproach(CurrentLocation);
			}
		}
		break;
	case EVRExpGrabbableNormalMotionMode::FollowTarget:
		if (!IsValid(TargetToFollow))
		{
			UE_LOG(LogVRExpGrabbableMotion, Warning, TEXT("VRExpGrabbableMotionComponent: TargetToFollow is not set."));
			StopMotion();
			return;
		}
		break;
	case EVRExpGrabbableNormalMotionMode::OrbitTarget:
		if (!IsValid(OrbitCenter))
		{
			UE_LOG(LogVRExpGrabbableMotion, Warning, TEXT("VRExpGrabbableMotionComponent: OrbitCenter is not set."));
			StopMotion();
			return;
		}
		CurrentOrbitAngle = 0.0f;
		break;
	case EVRExpGrabbableNormalMotionMode::RandomWander:
		WanderOriginAnchor = CaptureMotionAnchor(
			GetUpdatedComponentTransform());
		WanderOrigin = ResolveMotionAnchorTransform(
			WanderOriginAnchor).GetLocation();
		GenerateNewWanderTarget();
		WanderTimer = 0.0f;
		break;
	default:
		break;
	}

	SetMotionState(EVRExpGrabbableMotionState::NormalMotion);
	SetComponentTickEnabled(true);
}

void UVRExpGrabbableMotionComponent::StartReleaseMotion()
{
	FVRExpScopedMotionRuntimeMutation Mutation(*this);
	bAttachedKinematicFallActive = false;
	AttachedKinematicFallSpeed = 0.0f;
	ResolveUpdatedComponent();
	if (!IsValid(UpdatedComponent))
	{
		StopMotion();
		return;
	}

	RefreshCurrentMotionSpace();
	RebuildMotionTickPrerequisites();
	RefreshMotionEffectRuntimeState();

	PrepareForKinematicMotion();

	ReturnStartLocation = GetMotionBaseLocation();
	ReturnStartRotation = GetMotionBaseQuat().Rotator();
	ReturnProgress = 0.0f;
	ReturnTotalDistance = 0.0f;

	SetMotionState(EVRExpGrabbableMotionState::Releasing);
	SetComponentTickEnabled(true);
}

void UVRExpGrabbableMotionComponent::StartReleaseMotionFromDrop()
{
	FVRExpScopedMotionRuntimeMutation Mutation(*this);
	ResetSplineApproach();
	bIsMovingToSpline = false;
	switch (ReleaseMotionMode)
	{
	case EVRExpGrabbableReleaseMotionMode::None:
		StopMotion();
		break;
	case EVRExpGrabbableReleaseMotionMode::ReturnToStart:
	case EVRExpGrabbableReleaseMotionMode::ReturnToOrigin:
		StartReleaseMotion();
		break;
	case EVRExpGrabbableReleaseMotionMode::ReturnToSpline:
		if (!IsValid(SplineToFollow) && !InitializeSplineReference())
		{
			UE_LOG(LogVRExpGrabbableMotion, Warning, TEXT("VRExpGrabbableMotionComponent: ReturnToSpline needs a valid spline."));
			StartNormalMotion();
			return;
		}
		RebuildMotionTickPrerequisites();

		if (IsValid(SplineToFollow))
		{
			const FVector CurrentLocation = GetMotionBaseLocation();
			CaptureSplineApproachTarget(CurrentLocation);
			if (bUseKinematicGravityOnSplineRelease && !bTeleportToSplineOnRelease)
			{
				InitializeKinematicSplineApproach(CurrentLocation);
			}
		}
		StartReleaseMotion();
		break;
	case EVRExpGrabbableReleaseMotionMode::ContinueMotion:
		StartNormalMotion();
		break;
	case EVRExpGrabbableReleaseMotionMode::FallWithGravity:
		RefreshCurrentMotionSpace();
		if (IsUsingParentRelativeMotionSpace() &&
			AttachedFallWithGravityMode ==
				EVRExpGrabbableAttachedFallWithGravityMode::RelativeKinematic)
		{
			PrepareForKinematicMotion();
			AttachedKinematicFallSpeed = 0.0f;
			bAttachedKinematicFallActive = true;
			SetMotionState(EVRExpGrabbableMotionState::Releasing);
			SetComponentTickEnabled(true);
			RebuildMotionTickPrerequisites();
		}
		else
		{
			if (IsUsingParentRelativeMotionSpace())
			{
				UpdatedComponent->DetachFromComponent(
					FDetachmentTransformRules::KeepWorldTransform);
				RefreshCurrentMotionSpace(true);
			}
			SetUpdatedPrimitiveSimulatePhysics(true);
			StopMotion();
		}
		break;
	case EVRExpGrabbableReleaseMotionMode::FlyToTarget:
		if (!IsValid(FlyToTargetComponent))
		{
			UE_LOG(LogVRExpGrabbableMotion, Warning, TEXT("VRExpGrabbableMotionComponent: FlyToTarget needs a valid target component."));
			StopMotion();
			return;
		}
		StartReleaseMotion();
		break;
	default:
		StopMotion();
		break;
	}
}

void UVRExpGrabbableMotionComponent::FinishReleaseMotion(bool bResumeNormalMotion)
{
	{
		FVRExpScopedMotionRuntimeMutation Mutation(*this);
		CompletePendingReleaseAttachment();
		// NormalMotionMode::None 也可以作为仅效果模式，返回完成后应继续浮动或自转。
		if (bResumeNormalMotion &&
			(NormalMotionMode != EVRExpGrabbableNormalMotionMode::None || HasConfiguredMotionEffects()))
		{
			StartNormalMotion();
		}
		else
		{
			StopMotion();
		}
	}

	OnReturnMotionCompleted.Broadcast();
}

void UVRExpGrabbableMotionComponent::CaptureInitialReleaseAttachmentState()
{
	CancelPendingReleaseAttachment();
	ResolveUpdatedComponent();
	RefreshCurrentMotionSpace(true);
	InitialTransform = GetUpdatedComponentTransform();
	InitialMotionAnchor = CaptureMotionAnchor(InitialTransform);
	InitialOwnerRootComponent.Reset();
	InitialAttachParentComponent.Reset();
	InitialSplineAttachmentComponent.Reset();
	InitialAttachSocketName = NAME_None;
	InitialOwnerRelativeTransform = FTransform::Identity;
	InitialOwnerRelativeToSplineTransform = FTransform::Identity;
	bHasInitialParentAttachment = false;
	bHasInitialRelativeToSplineTransform = false;

	AActor* Owner = GetOwner();
	USceneComponent* OwnerRoot = IsValid(Owner)
		? Owner->GetRootComponent()
		: nullptr;
	if (!IsValid(OwnerRoot))
	{
		return;
	}

	InitialOwnerRootComponent = OwnerRoot;
	if (USceneComponent* InitialParent = OwnerRoot->GetAttachParent())
	{
		if (IsValid(InitialParent))
		{
			InitialAttachParentComponent = InitialParent;
			InitialAttachSocketName = OwnerRoot->GetAttachSocketName();
			InitialOwnerRelativeTransform = OwnerRoot->GetRelativeTransform();
			bHasInitialParentAttachment = true;
		}
	}

	USplineComponent* InitialSpline = IsValid(SplineToFollow)
		? SplineToFollow.Get()
		: ResolveConfiguredSplineComponent();
	if (IsValid(InitialSpline) &&
		IsValidReleaseAttachmentTarget(OwnerRoot, InitialSpline))
	{
		InitialSplineAttachmentComponent = InitialSpline;
		InitialOwnerRelativeToSplineTransform =
			OwnerRoot->GetComponentTransform().GetRelativeTransform(
				InitialSpline->GetComponentTransform());
		bHasInitialRelativeToSplineTransform = true;
	}
}

USplineComponent* UVRExpGrabbableMotionComponent::ResolveConfiguredSplineComponent() const
{
	if (bUseManualSplineReference)
	{
		if (bUseSplineActor)
		{
			return IsValid(SplineActor)
				? SplineActor->FindComponentByClass<USplineComponent>()
				: nullptr;
		}

		if (SplineComponentName == NAME_None)
		{
			return nullptr;
		}

		if (AActor* Owner = GetOwner())
		{
			TArray<USplineComponent*> SplineComponents;
			Owner->GetComponents<USplineComponent>(SplineComponents);
			for (USplineComponent* SplineComponent : SplineComponents)
			{
				if (IsValid(SplineComponent) &&
					SplineComponent->GetFName() == SplineComponentName)
				{
					return SplineComponent;
				}
			}
		}

		return nullptr;
	}

	if (AActor* Owner = GetOwner())
	{
		return Owner->FindComponentByClass<USplineComponent>();
	}

	return nullptr;
}

const FVRExpGrabbableReleaseAttachmentModeRule&
UVRExpGrabbableMotionComponent::GetReleaseAttachmentRule(
	EVRExpGrabbableReleaseMotionMode Mode) const
{
	switch (Mode)
	{
	case EVRExpGrabbableReleaseMotionMode::ReturnToStart:
		return ReleaseAttachmentRules.ReturnToStartRule;
	case EVRExpGrabbableReleaseMotionMode::ReturnToSpline:
		return ReleaseAttachmentRules.ReturnToSplineRule;
	case EVRExpGrabbableReleaseMotionMode::ReturnToOrigin:
		return ReleaseAttachmentRules.ReturnToOriginRule;
	case EVRExpGrabbableReleaseMotionMode::ContinueMotion:
		return ReleaseAttachmentRules.ContinueMotionRule;
	case EVRExpGrabbableReleaseMotionMode::FallWithGravity:
		return ReleaseAttachmentRules.FallWithGravityRule;
	case EVRExpGrabbableReleaseMotionMode::FlyToTarget:
		return ReleaseAttachmentRules.FlyToTargetRule;
	case EVRExpGrabbableReleaseMotionMode::None:
	default:
		return ReleaseAttachmentRules.NoneRule;
	}
}

void UVRExpGrabbableMotionComponent::HandleReleaseAttachmentAfterDrop(
	EVRExpGrabbableReleaseMotionMode ReleasedMode)
{
	PrepareReleaseAttachmentBeforeMotion(ReleasedMode);
	FinalizeReleaseAttachmentAfterMotionStart();
}

void UVRExpGrabbableMotionComponent::PrepareReleaseAttachmentBeforeMotion(
	EVRExpGrabbableReleaseMotionMode ReleasedMode)
{
	CancelPendingReleaseAttachment();
	const bool bAllowSplineAttachment = bAttachOwnerToSplineOnRelease;
	const bool bAllowInitialParentAttachment =
		bReattachOwnerToInitialParentComponentOnRelease;
	if (!bAllowSplineAttachment && !bAllowInitialParentAttachment)
	{
		return;
	}

	const FVRExpGrabbableReleaseAttachmentModeRule Rule =
		GetReleaseAttachmentRule(ReleasedMode);
	if (!Rule.bEnableAttachment)
	{
		return;
	}

	bHasPendingReleaseAttachment = true;
	PendingReleaseAttachmentMode = ReleasedMode;
	PendingReleaseAttachmentRule = Rule;
	bPendingAttachOwnerToSpline = bAllowSplineAttachment;
	bPendingReattachOwnerToInitialParent =
		bAllowInitialParentAttachment;

	// OnRelease 必须在初始化释放运动之前完成附加，确保第一帧直接使用新父组件空间。
	if (Rule.Timing == EVRExpGrabbableReleaseAttachmentTiming::OnRelease)
	{
		CompletePendingReleaseAttachment();
	}
}

void UVRExpGrabbableMotionComponent::FinalizeReleaseAttachmentAfterMotionStart()
{
	if (bHasPendingReleaseAttachment &&
		CurrentMotionState != EVRExpGrabbableMotionState::Releasing)
	{
		// 没有持续释放阶段的模式无法等待完成，保持原有立即执行语义。
		CompletePendingReleaseAttachment();
	}
}

void UVRExpGrabbableMotionComponent::CancelPendingReleaseAttachment()
{
	bHasPendingReleaseAttachment = false;
	PendingReleaseAttachmentMode =
		EVRExpGrabbableReleaseMotionMode::None;
	bPendingAttachOwnerToSpline = false;
	bPendingReattachOwnerToInitialParent = false;
}

void UVRExpGrabbableMotionComponent::CompletePendingReleaseAttachment()
{
	if (!bHasPendingReleaseAttachment)
	{
		return;
	}

	const EVRExpGrabbableReleaseMotionMode ReleasedMode =
		PendingReleaseAttachmentMode;
	const FVRExpGrabbableReleaseAttachmentModeRule Rule =
		PendingReleaseAttachmentRule;
	const bool bAllowSplineAttachment =
		bPendingAttachOwnerToSpline;
	const bool bAllowInitialParentAttachment =
		bPendingReattachOwnerToInitialParent;
	CancelPendingReleaseAttachment();

	TryApplyReleaseAttachment(
		ReleasedMode,
		Rule,
		bAllowSplineAttachment,
		bAllowInitialParentAttachment);
}

bool UVRExpGrabbableMotionComponent::TryApplyReleaseAttachment(
	EVRExpGrabbableReleaseMotionMode ReleasedMode,
	const FVRExpGrabbableReleaseAttachmentModeRule& Rule,
	bool bAllowSplineAttachment,
	bool bAllowInitialParentAttachment)
{
	AActor* Owner = GetOwner();
	USceneComponent* OwnerRoot = InitialOwnerRootComponent.Get();
	if (!IsValid(Owner) || !IsValid(OwnerRoot) ||
		Owner->GetRootComponent() != OwnerRoot)
	{
		UE_LOG(
			LogVRExpGrabbableMotion,
			Warning,
			TEXT("VRExpGrabbableMotionComponent: release attachment skipped because the cached owner root is no longer valid."));
		return false;
	}

	USceneComponent* AttachmentTarget = nullptr;
	USplineComponent* SplineTarget = nullptr;
	FName SocketName = NAME_None;
	if (!ResolveReleaseAttachmentTarget(
			OwnerRoot,
			bAllowSplineAttachment,
			bAllowInitialParentAttachment,
			AttachmentTarget,
			SocketName,
			SplineTarget))
	{
		return false;
	}

	UPrimitiveComponent* RootPrimitive =
		Cast<UPrimitiveComponent>(OwnerRoot);
	const bool bWasSimulatingPhysics =
		RootPrimitive && RootPrimitive->IsSimulatingPhysics();
	if (Rule.PhysicsPolicy ==
			EVRExpGrabbableAttachmentPhysicsPolicy::PreservePhysicsAndSkipIfSimulating &&
		bWasSimulatingPhysics)
	{
		return false;
	}

	FVector SavedLinearVelocity = FVector::ZeroVector;
	FVector SavedAngularVelocity = FVector::ZeroVector;
	bool bDisabledPhysicsForAttachment = false;
	if (Rule.PhysicsPolicy ==
			EVRExpGrabbableAttachmentPhysicsPolicy::DisablePhysicsAndAttach &&
		bWasSimulatingPhysics)
	{
		SavedLinearVelocity = RootPrimitive->GetPhysicsLinearVelocity();
		SavedAngularVelocity =
			RootPrimitive->GetPhysicsAngularVelocityInRadians();
		RootPrimitive->SetSimulatePhysics(false);
		bDisabledPhysicsForAttachment = true;
	}

	FVector SnapLocation = FVector::ZeroVector;
	FQuat SnapRotation = OwnerRoot->GetComponentQuat();
	bool bSnapToSpline = false;
	if (IsValid(SplineTarget) &&
		Rule.SplineTransformRule ==
			EVRExpGrabbableSplineAttachmentTransformRule::SnapToClosestSplinePoint)
	{
		const float ClosestInputKey =
			SplineTarget->FindInputKeyClosestToWorldLocation(
				OwnerRoot->GetComponentLocation());
		SnapLocation = SplineTarget->GetLocationAtSplineInputKey(
			ClosestInputKey,
			ESplineCoordinateSpace::World);
		const float DistanceAlongSpline =
			SplineTarget->GetDistanceAlongSplineAtSplineInputKey(
				ClosestInputKey);
		SnapRotation = GetSplineAttachmentRotation(
			SplineTarget,
			DistanceAlongSpline,
			OwnerRoot->GetComponentQuat());
		bSnapToSpline = true;
	}

	const bool bAttached = OwnerRoot->AttachToComponent(
		AttachmentTarget,
		FAttachmentTransformRules::KeepWorldTransform,
		SocketName);
	if (!bAttached)
	{
		if (bDisabledPhysicsForAttachment && IsValid(RootPrimitive))
		{
			RootPrimitive->SetSimulatePhysics(true);
			RootPrimitive->SetPhysicsLinearVelocity(SavedLinearVelocity);
			RootPrimitive->SetPhysicsAngularVelocityInRadians(
				SavedAngularVelocity);
			RootPrimitive->WakeAllRigidBodies();
		}

		UE_LOG(
			LogVRExpGrabbableMotion,
			Warning,
			TEXT("VRExpGrabbableMotionComponent: release attachment failed for mode %d."),
			static_cast<int32>(ReleasedMode));
		return false;
	}

	if (IsValid(SplineTarget))
	{
		if (bSnapToSpline)
		{
			const FTransform SocketWorldTransform =
				AttachmentTarget->GetSocketTransform(
					SocketName,
					ERelativeTransformSpace::RTS_World);
			const FTransform SnapRelativeTransform =
				FTransform(
					SnapRotation,
					SnapLocation,
					OwnerRoot->GetComponentScale())
				.GetRelativeTransform(SocketWorldTransform);
			OwnerRoot->SetRelativeLocationAndRotation(
				SnapRelativeTransform.GetLocation(),
				SnapRelativeTransform.GetRotation(),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}
		else if (Rule.SplineTransformRule ==
			EVRExpGrabbableSplineAttachmentTransformRule::RestoreInitialRelativeToSpline)
		{
			if (AttachmentTarget == InitialAttachParentComponent.Get() &&
				bHasInitialParentAttachment)
			{
				OwnerRoot->SetRelativeTransform(
					InitialOwnerRelativeTransform,
					false,
					nullptr,
					ETeleportType::TeleportPhysics);
			}
			else if (AttachmentTarget ==
					InitialSplineAttachmentComponent.Get() &&
				bHasInitialRelativeToSplineTransform)
			{
				OwnerRoot->SetRelativeTransform(
					InitialOwnerRelativeToSplineTransform,
					false,
					nullptr,
					ETeleportType::TeleportPhysics);
			}
		}
	}
	else if (Rule.InitialParentTransformRule ==
			 EVRExpGrabbableInitialParentTransformRule::RestoreInitialRelative &&
		bHasInitialParentAttachment)
	{
		OwnerRoot->SetRelativeTransform(
			InitialOwnerRelativeTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}

	RefreshCurrentMotionSpace(true);
	RebuildMotionTickPrerequisites();

	return true;
}

bool UVRExpGrabbableMotionComponent::ResolveReleaseAttachmentTarget(
	USceneComponent* OwnerRoot,
	bool bAllowSplineAttachment,
	bool bAllowInitialParentAttachment,
	USceneComponent*& OutTarget,
	FName& OutSocketName,
	USplineComponent*& OutSplineTarget)
{
	OutTarget = nullptr;
	OutSocketName = NAME_None;
	OutSplineTarget = nullptr;

	if (bAllowSplineAttachment)
	{
		USplineComponent* ConfiguredSpline = IsValid(SplineToFollow)
			? SplineToFollow.Get()
			: ResolveConfiguredSplineComponent();
		if (IsValid(ConfiguredSpline) &&
			IsValidReleaseAttachmentTarget(OwnerRoot, ConfiguredSpline))
		{
			OutTarget = ConfiguredSpline;
			OutSplineTarget = ConfiguredSpline;
			if (ConfiguredSpline == InitialAttachParentComponent.Get())
			{
				OutSocketName = InitialAttachSocketName;
			}
			return true;
		}

		USplineComponent* InitialParentSpline =
			Cast<USplineComponent>(InitialAttachParentComponent.Get());
		if (IsValid(InitialParentSpline) &&
			IsValidReleaseAttachmentTarget(OwnerRoot, InitialParentSpline))
		{
			OutTarget = InitialParentSpline;
			OutSocketName = InitialAttachSocketName;
			OutSplineTarget = InitialParentSpline;
			return true;
		}
	}

	USceneComponent* InitialParent =
		InitialAttachParentComponent.Get();
	if (bAllowInitialParentAttachment &&
		bHasInitialParentAttachment &&
		IsValidReleaseAttachmentTarget(OwnerRoot, InitialParent))
	{
		OutTarget = InitialParent;
		OutSocketName = InitialAttachSocketName;
		return true;
	}

	return false;
}

bool UVRExpGrabbableMotionComponent::IsValidReleaseAttachmentTarget(
	const USceneComponent* OwnerRoot,
	const USceneComponent* Target) const
{
	if (!IsValid(OwnerRoot) || !IsValid(Target) || OwnerRoot == Target)
	{
		return false;
	}

	const AActor* Owner = GetOwner();
	if (Target->GetOwner() == Owner || Target->IsAttachedTo(OwnerRoot))
	{
		return false;
	}

	return true;
}

FQuat UVRExpGrabbableMotionComponent::GetSplineAttachmentRotation(
	const USplineComponent* SplineComponent,
	float DistanceAlongSpline,
	const FQuat& CurrentRootRotation)
{
	if (!bUpdateRotationDuringMotion || !IsValid(SplineComponent))
	{
		return CurrentRootRotation;
	}

	FQuat SplineRotation =
		SplineComponent->GetQuaternionAtDistanceAlongSpline(
			DistanceAlongSpline,
			ESplineCoordinateSpace::World);
	if (bReverseDirection)
	{
		SplineRotation =
			(SplineRotation * FQuat(FVector::UpVector, PI)).GetNormalized();
	}

	const FQuat MotionSpaceRotation =
		ConvertWorldRotationToMotionSpace(SplineRotation);
	return ConvertMotionRotationToWorld(
		ApplyOrientationAdjustment(MotionSpaceRotation));
}

void UVRExpGrabbableMotionComponent::TickNormalMotion(float DeltaTime)
{
	switch (NormalMotionMode)
	{
	case EVRExpGrabbableNormalMotionMode::FollowSpline:
		TickSplineMotion(DeltaTime);
		break;
	case EVRExpGrabbableNormalMotionMode::FollowTarget:
		TickFollowMotion(DeltaTime);
		break;
	case EVRExpGrabbableNormalMotionMode::OrbitTarget:
		TickOrbitMotion(DeltaTime);
		break;
	case EVRExpGrabbableNormalMotionMode::RandomWander:
		TickWanderMotion(DeltaTime);
		break;
	default:
		break;
	}
}

void UVRExpGrabbableMotionComponent::TickReleaseMotion(float DeltaTime)
{
	const EVRExpGrabbableReleaseMotionMode ActiveReleaseMode =
		bHasPendingReleaseAttachment
			? PendingReleaseAttachmentMode
			: ReleaseMotionMode;
	switch (ActiveReleaseMode)
	{
	case EVRExpGrabbableReleaseMotionMode::ReturnToStart:
		TickReturnToStart(DeltaTime);
		break;
	case EVRExpGrabbableReleaseMotionMode::ReturnToOrigin:
		TickReturnToOrigin(DeltaTime);
		break;
	case EVRExpGrabbableReleaseMotionMode::ReturnToSpline:
		TickReturnToSpline(DeltaTime);
		break;
	case EVRExpGrabbableReleaseMotionMode::FlyToTarget:
		TickFlyToTarget(DeltaTime);
		break;
	case EVRExpGrabbableReleaseMotionMode::FallWithGravity:
		if (bAttachedKinematicFallActive)
		{
			TickAttachedKinematicFall(DeltaTime);
		}
		else
		{
			FinalizeReleaseAttachmentAfterMotionStart();
			StopMotion();
		}
		break;
	default:
		CompletePendingReleaseAttachment();
		StopMotion();
		break;
	}
}

void UVRExpGrabbableMotionComponent::TickSplineMotion(float DeltaTime)
{
	if (!IsValid(SplineToFollow))
	{
		StopMotion();
		return;
	}

	const FVector CurrentLocation = GetMotionBaseLocation();

	if (bIsMovingToSpline)
	{
		if (bUseKinematicGravityOnSplineStart)
		{
			if (SplineApproachPhase == ESplineApproachPhase::None &&
				CaptureSplineApproachTarget(CurrentLocation))
			{
				InitializeKinematicSplineApproach(CurrentLocation);
			}

			if (!RefreshSplineApproachTarget())
			{
				StopMotion();
				return;
			}

			if (SplineApproachPhase != ESplineApproachPhase::None &&
				TickKinematicSplineApproach(DeltaTime, SplineSpeed, false))
			{
				const FVector TargetLocation = SplineApproachTargetLocation;
				const float TargetProgress = SplineApproachTargetProgress;
				MoveUpdatedComponentTo(
					TargetLocation,
					GetSplineMotionRotation(TargetProgress),
					DeltaTime);

				if (FVector::Dist(GetMotionBaseLocation(), TargetLocation) <= SplineArrivalThreshold)
				{
					CurrentSplineProgress = TargetProgress;
					bIsMovingToSpline = false;
					ResetSplineApproach();
				}
			}
			return;
		}

		ResetSplineApproach();
		if (!RefreshSplineApproachTarget())
		{
			StopMotion();
			return;
		}
		const FVector ClosestLocation = SplineApproachTargetLocation;
		const float Distance = FVector::Dist(CurrentLocation, ClosestLocation);

		if (Distance <= SplineArrivalThreshold)
		{
			CurrentSplineProgress = SplineApproachTargetProgress;
			bIsMovingToSpline = false;
			MoveUpdatedComponentTo(
				ClosestLocation,
				GetSplineMotionRotation(SplineApproachTargetProgress),
				DeltaTime);
			return;
		}

		const FVector NewLocation = FMath::VInterpConstantTo(CurrentLocation, ClosestLocation, DeltaTime, SplineSpeed);
		const FVector Direction = (ClosestLocation - CurrentLocation).GetSafeNormal();
		const FQuat NewRotation = Direction.IsNearlyZero() ? GetMotionBaseQuat() : ApplyOrientationAdjustment(Direction.ToOrientationQuat());
		MoveUpdatedComponentTo(NewLocation, NewRotation, DeltaTime);
		return;
	}

	const float SplineLength = SplineToFollow->GetSplineLength();
	if (SplineLength <= KINDA_SMALL_NUMBER)
	{
		StopMotion();
		return;
	}

	const float DirectionMultiplier = bReverseDirection ? -1.0f : 1.0f;
	CurrentSplineProgress += SplineSpeed * DeltaTime * DirectionMultiplier;

	if (CurrentSplineProgress > SplineLength || CurrentSplineProgress < 0.0f)
	{
		if (bLoopSpline)
		{
			CurrentSplineProgress = FMath::Fmod(CurrentSplineProgress, SplineLength);
			if (CurrentSplineProgress < 0.0f)
			{
				CurrentSplineProgress += SplineLength;
			}
		}
		else
		{
			CurrentSplineProgress = FMath::Clamp(CurrentSplineProgress, 0.0f, SplineLength);
			const FVector EndLocation = ConvertWorldLocationToMotionSpace(
				SplineToFollow->GetLocationAtDistanceAlongSpline(
					CurrentSplineProgress,
					ESplineCoordinateSpace::World));
			const FQuat EndRotation = GetSplineMotionRotation(CurrentSplineProgress);
			MoveUpdatedComponentTo(EndLocation, EndRotation, DeltaTime);
			StopMotion();
			return;
		}
	}

	const FVector NewLocation = ConvertWorldLocationToMotionSpace(
		SplineToFollow->GetLocationAtDistanceAlongSpline(
			CurrentSplineProgress,
			ESplineCoordinateSpace::World));
	const FQuat NewRotation = GetSplineMotionRotation(CurrentSplineProgress);
	MoveUpdatedComponentTo(NewLocation, NewRotation, DeltaTime);
}

void UVRExpGrabbableMotionComponent::TickFollowMotion(float DeltaTime)
{
	if (!IsValid(TargetToFollow))
	{
		StopMotion();
		return;
	}

	const FVector CurrentLocation = GetMotionBaseLocation();
	const FVector TargetLocation = ConvertWorldLocationToMotionSpace(
		TargetToFollow->GetComponentLocation());
	const FVector Direction = (TargetLocation - CurrentLocation).GetSafeNormal();
	const float Distance = FVector::Dist(CurrentLocation, TargetLocation);

	if (Distance <= FollowDistance)
	{
		return;
	}

	const FVector DesiredLocation = TargetLocation - Direction * FollowDistance;
	const FVector NewLocation = FMath::VInterpTo(CurrentLocation, DesiredLocation, DeltaTime, FollowSpeed / 100.0f);
	const FQuat NewRotation = bOrientToTarget && !Direction.IsNearlyZero() ? ApplyOrientationAdjustment(Direction.ToOrientationQuat()) : GetMotionBaseQuat();
	MoveUpdatedComponentTo(NewLocation, NewRotation, DeltaTime);
}

void UVRExpGrabbableMotionComponent::TickOrbitMotion(float DeltaTime)
{
	if (!IsValid(OrbitCenter))
	{
		StopMotion();
		return;
	}

	CurrentOrbitAngle += OrbitSpeed * DeltaTime;
	if (CurrentOrbitAngle >= 360.0f || CurrentOrbitAngle <= -360.0f)
	{
		CurrentOrbitAngle = FMath::Fmod(CurrentOrbitAngle, 360.0f);
	}

	const float RadAngle = FMath::DegreesToRadians(CurrentOrbitAngle);
	FVector Offset(
		FMath::Cos(RadAngle) * OrbitRadius,
		FMath::Sin(RadAngle) * OrbitRadius,
		OrbitHeightOffset);

	const FVector SafeOrbitAxis = OrbitAxis.GetSafeNormal();
	if (!SafeOrbitAxis.IsNearlyZero() && !SafeOrbitAxis.Equals(FVector::UpVector))
	{
		const FQuat AxisRotation = FQuat::FindBetweenNormals(FVector::UpVector, SafeOrbitAxis);
		Offset = AxisRotation.RotateVector(Offset);
	}
	if (IsUsingParentRelativeMotionSpace() &&
		AttachedMotionDirectionMode ==
			EVRExpGrabbableAttachedMotionDirectionMode::PreserveWorldDirections)
	{
		Offset = ConvertWorldVectorToMotionSpace(Offset);
	}

	const FVector CenterLocation = ConvertWorldLocationToMotionSpace(
		OrbitCenter->GetComponentLocation());
	const FVector NewLocation = CenterLocation + Offset;
	const FVector DirectionToCenter =
		(CenterLocation - NewLocation).GetSafeNormal();
	const FQuat NewRotation = DirectionToCenter.IsNearlyZero() ? GetMotionBaseQuat() : ApplyOrientationAdjustment(DirectionToCenter.ToOrientationQuat());
	MoveUpdatedComponentTo(NewLocation, NewRotation, DeltaTime);
}

void UVRExpGrabbableMotionComponent::TickWanderMotion(float DeltaTime)
{
	WanderOrigin = ResolveMotionAnchorTransform(
		WanderOriginAnchor).GetLocation();
	CurrentWanderTarget = ResolveMotionAnchorTransform(
		WanderTargetAnchor).GetLocation();
	const FVector CurrentLocation = GetMotionBaseLocation();
	const float DistanceToTarget = FVector::Dist(CurrentLocation, CurrentWanderTarget);

	WanderTimer += DeltaTime;
	if (DistanceToTarget < 50.0f || WanderTimer >= WanderChangeInterval)
	{
		GenerateNewWanderTarget();
		WanderTimer = 0.0f;
	}

	const FVector NewLocation = FMath::VInterpTo(CurrentLocation, CurrentWanderTarget, DeltaTime, WanderSpeed / 100.0f);
	const FVector Direction = (CurrentWanderTarget - CurrentLocation).GetSafeNormal();
	FRotator TargetRotation = Direction.Rotation();
	if (!Direction.IsNearlyZero() && (OrientationAdjustment.bEnableRotationOffset || OrientationAdjustment.bEnableAxisAdjustment))
	{
		TargetRotation = ApplyOrientationAdjustment(TargetRotation.Quaternion()).Rotator();
	}
	const FQuat NewRotation = Direction.IsNearlyZero() ? GetMotionBaseQuat() : FMath::RInterpTo(GetMotionBaseQuat().Rotator(), TargetRotation, DeltaTime, 2.0f).Quaternion();
	MoveUpdatedComponentTo(NewLocation, NewRotation, DeltaTime);
}

void UVRExpGrabbableMotionComponent::TickReturnToStart(float DeltaTime)
{
	const FVector CurrentLocation = GetMotionBaseLocation();
	const FTransform TargetTransform = ResolveMotionAnchorTransform(
		InitialMotionAnchor);
	const FVector TargetLocation = TargetTransform.GetLocation();
	const float Distance = FVector::Dist(CurrentLocation, TargetLocation);

	if (Distance < 1.0f)
	{
		MoveUpdatedComponentTo(
			TargetLocation,
			TargetTransform.GetRotation(),
			DeltaTime,
			true,
			ETeleportType::TeleportPhysics);
		FinishReleaseMotion(true);
		return;
	}

	float InterpSpeed = ReturnSpeed / 100.0f;
	if (IsValid(ReturnCurve))
	{
		if (ReturnTotalDistance <= KINDA_SMALL_NUMBER)
		{
			ReturnTotalDistance = FMath::Max(FVector::Dist(ReturnStartLocation, TargetLocation), 1.0f);
		}
		const float ReturnDuration = ReturnSpeed > KINDA_SMALL_NUMBER ? ReturnTotalDistance / ReturnSpeed : 1.0f;
		ReturnProgress = FMath::Clamp(ReturnProgress + DeltaTime / FMath::Max(ReturnDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
		InterpSpeed *= ReturnCurve->GetFloatValue(ReturnProgress);
	}

	const FVector NewLocation = bSmoothReturn
		? FMath::VInterpTo(CurrentLocation, TargetLocation, DeltaTime, InterpSpeed)
		: FMath::VInterpConstantTo(CurrentLocation, TargetLocation, DeltaTime, ReturnSpeed);

	const FQuat TargetRotation = TargetTransform.GetRotation();
	const FQuat NewRotation = bSmoothReturn
		? FMath::QInterpTo(GetMotionBaseQuat(), TargetRotation, DeltaTime, InterpSpeed)
		: FMath::QInterpConstantTo(GetMotionBaseQuat(), TargetRotation, DeltaTime, ReturnSpeed);

	MoveUpdatedComponentTo(NewLocation, NewRotation, DeltaTime);
}

void UVRExpGrabbableMotionComponent::TickReturnToOrigin(float DeltaTime)
{
	WanderOrigin = ResolveMotionAnchorTransform(
		WanderOriginAnchor).GetLocation();
	const FVector CurrentLocation = GetMotionBaseLocation();
	const float Distance = FVector::Dist(CurrentLocation, WanderOrigin);

	if (Distance < 1.0f)
	{
		MoveUpdatedComponentTo(WanderOrigin, GetMotionBaseQuat(), DeltaTime, true, ETeleportType::TeleportPhysics);
		FinishReleaseMotion(true);
		return;
	}

	const FVector NewLocation = bSmoothReturn
		? FMath::VInterpTo(CurrentLocation, WanderOrigin, DeltaTime, ReturnSpeed / 100.0f)
		: FMath::VInterpConstantTo(CurrentLocation, WanderOrigin, DeltaTime, ReturnSpeed);

	MoveUpdatedComponentTo(NewLocation, GetMotionBaseQuat(), DeltaTime);
}

void UVRExpGrabbableMotionComponent::TickReturnToSpline(float DeltaTime)
{
	if (!IsValid(SplineToFollow))
	{
		FinishReleaseMotion(true);
		return;
	}

	if (!RefreshSplineApproachTarget())
	{
		FinishReleaseMotion(true);
		return;
	}

	if (bTeleportToSplineOnRelease)
	{
		CurrentSplineProgress = SplineApproachTargetProgress;
		bIsMovingToSpline = false;
		MoveUpdatedComponentTo(
			SplineApproachTargetLocation,
			GetSplineMotionRotation(SplineApproachTargetProgress),
			DeltaTime,
			true,
			ETeleportType::TeleportPhysics);
		FinishReleaseMotion(true);
		return;
	}

	const FVector CurrentLocation = GetMotionBaseLocation();
	if (bUseKinematicGravityOnSplineRelease)
	{
		if (SplineApproachPhase == ESplineApproachPhase::None)
		{
			InitializeKinematicSplineApproach(CurrentLocation);
		}

		if (TickKinematicSplineApproach(DeltaTime, ReturnSpeed, bSmoothReturn))
		{
			const FVector TargetLocation = SplineApproachTargetLocation;
			const float TargetProgress = SplineApproachTargetProgress;
			MoveUpdatedComponentTo(
				TargetLocation,
				GetSplineMotionRotation(TargetProgress),
				DeltaTime,
				true,
				ETeleportType::TeleportPhysics);
			if (FVector::Dist(GetMotionBaseLocation(), TargetLocation) <= SplineArrivalThreshold)
			{
				CurrentSplineProgress = TargetProgress;
				bIsMovingToSpline = false;
				ResetSplineApproach();
				FinishReleaseMotion(true);
			}
		}
		return;
	}

	ResetSplineApproach();
	const float Distance = FVector::Dist(CurrentLocation, SplineApproachTargetLocation);

	if (Distance <= SplineArrivalThreshold)
	{
		CurrentSplineProgress = SplineApproachTargetProgress;
		bIsMovingToSpline = false;
		MoveUpdatedComponentTo(
			SplineApproachTargetLocation,
			GetSplineMotionRotation(SplineApproachTargetProgress),
			DeltaTime,
			true,
			ETeleportType::TeleportPhysics);
		FinishReleaseMotion(true);
		return;
	}

	const FVector NewLocation = bSmoothReturn
		? FMath::VInterpTo(CurrentLocation, SplineApproachTargetLocation, DeltaTime, ReturnSpeed / 100.0f)
		: FMath::VInterpConstantTo(CurrentLocation, SplineApproachTargetLocation, DeltaTime, ReturnSpeed);

	const FVector Direction = (SplineApproachTargetLocation - CurrentLocation).GetSafeNormal();
	const FQuat NewRotation = Direction.IsNearlyZero() ? GetMotionBaseQuat() : ApplyOrientationAdjustment(Direction.ToOrientationQuat());
	MoveUpdatedComponentTo(NewLocation, NewRotation, DeltaTime);
}

void UVRExpGrabbableMotionComponent::TickFlyToTarget(float DeltaTime)
{
	if (!IsValid(FlyToTargetComponent))
	{
		FinishReleaseMotion(false);
		return;
	}

	const FVector CurrentLocation = GetMotionBaseLocation();
	const FVector TargetLocation = ConvertWorldLocationToMotionSpace(
		FlyToTargetComponent->GetComponentLocation());
	const float Distance = FVector::Dist(CurrentLocation, TargetLocation);

	if (Distance < 10.0f)
	{
		MoveUpdatedComponentTo(TargetLocation, GetMotionBaseQuat(), DeltaTime, true, ETeleportType::TeleportPhysics);
		FinishReleaseMotion(false);
		return;
	}

	const FVector NewLocation = FMath::VInterpConstantTo(CurrentLocation, TargetLocation, DeltaTime, ReturnSpeed);
	const FVector Direction = (TargetLocation - CurrentLocation).GetSafeNormal();
	const FQuat NewRotation = Direction.IsNearlyZero() ? GetMotionBaseQuat() : ApplyOrientationAdjustment(Direction.ToOrientationQuat());
	MoveUpdatedComponentTo(NewLocation, NewRotation, DeltaTime);
}

void UVRExpGrabbableMotionComponent::TickAttachedKinematicFall(
	float DeltaTime)
{
	if (!bAttachedKinematicFallActive ||
		!FMath::IsFinite(DeltaTime) ||
		DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (!IsUsingParentRelativeMotionSpace())
	{
		// 外部系统在下落期间解除附加后，恢复未附加对象的真实物理语义。
		bAttachedKinematicFallActive = false;
		SetUpdatedPrimitiveSimulatePhysics(true);
		StopMotion();
		return;
	}

	const float GravityAcceleration = ResolveKinematicGravityAcceleration();
	const float EffectiveMaxFallSpeed =
		FMath::IsFinite(MaxFallSpeed)
			? FMath::Max(MaxFallSpeed, 0.0f)
			: 0.0f;
	if (GravityAcceleration <= KINDA_SMALL_NUMBER ||
		EffectiveMaxFallSpeed <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float PreviousFallSpeed = FMath::Clamp(
		AttachedKinematicFallSpeed,
		0.0f,
		EffectiveMaxFallSpeed);
	AttachedKinematicFallSpeed = FMath::Min(
		PreviousFallSpeed + GravityAcceleration * DeltaTime,
		EffectiveMaxFallSpeed);
	const float FallDistance =
		0.5f * (PreviousFallSpeed + AttachedKinematicFallSpeed) *
		DeltaTime;
	FVector FallDelta =
		-GetKinematicUpAxisInMotionSpace() * FallDistance;
	if (AttachedMotionDirectionMode ==
		EVRExpGrabbableAttachedMotionDirectionMode::PreserveWorldDirections)
	{
		FallDelta = ConvertWorldVectorToMotionSpace(
			-FVector::UpVector * FallDistance);
	}
	const FVector TargetLocation =
		GetMotionBaseLocation() + FallDelta;

	bool bForceNoSweep = false;
	bool bForceSweep = false;
	bool bCompleteOnBlock = true;
	switch (AttachedKinematicFallCollisionMode)
	{
	case EVRExpGrabbableAttachedKinematicFallCollisionMode::SweepAndStop:
		bForceSweep = true;
		break;
	case EVRExpGrabbableAttachedKinematicFallCollisionMode::NoSweep:
		bForceNoSweep = true;
		bCompleteOnBlock = false;
		break;
	case EVRExpGrabbableAttachedKinematicFallCollisionMode::UseGlobalMovementSettings:
	default:
		bCompleteOnBlock = bStopOnBlockingHit;
		break;
	}

	MoveUpdatedComponentTo(
		TargetLocation,
		GetMotionBaseQuat(),
		DeltaTime,
		bForceNoSweep,
		ETeleportType::None,
		bForceSweep,
		bCompleteOnBlock);
}

bool UVRExpGrabbableMotionComponent::InitializeSplineReference()
{
	SplineToFollow = ResolveConfiguredSplineComponent();
	return IsValid(SplineToFollow);
}

bool UVRExpGrabbableMotionComponent::CaptureSplineApproachTarget(
	const FVector& CurrentLocation)
{
	if (!IsValid(SplineToFollow))
	{
		return false;
	}

	const float ClosestInputKey =
		SplineToFollow->FindInputKeyClosestToWorldLocation(
			ConvertMotionLocationToWorld(CurrentLocation));
	SplineApproachTargetProgress =
		SplineToFollow->GetDistanceAlongSplineAtSplineInputKey(
			ClosestInputKey);
	return RefreshSplineApproachTarget();
}

bool UVRExpGrabbableMotionComponent::RefreshSplineApproachTarget()
{
	if (!IsValid(SplineToFollow))
	{
		return false;
	}

	SplineApproachTargetLocation = ConvertWorldLocationToMotionSpace(
		SplineToFollow->GetLocationAtDistanceAlongSpline(
			SplineApproachTargetProgress,
			ESplineCoordinateSpace::World));
	return true;
}

void UVRExpGrabbableMotionComponent::InitializeKinematicSplineApproach(
	const FVector& CurrentLocation)
{
	SplineApproachFallSpeed = 0.0f;
	const FVector UpAxis = GetKinematicUpAxisInMotionSpace();
	const float HeightDelta = FVector::DotProduct(
		SplineApproachTargetLocation - CurrentLocation,
		UpAxis);

	if (HeightDelta < -KINDA_SMALL_NUMBER)
	{
		const float GravityAcceleration =
			ResolveKinematicGravityAcceleration();
		const float EffectiveMaxFallSpeed =
			FMath::IsFinite(MaxFallSpeed)
				? FMath::Max(MaxFallSpeed, 0.0f)
				: 0.0f;
		SplineApproachPhase =
			GravityAcceleration > KINDA_SMALL_NUMBER &&
				EffectiveMaxFallSpeed > KINDA_SMALL_NUMBER
				? ESplineApproachPhase::Falling
				: ESplineApproachPhase::Approaching;
		return;
	}

	if (HeightDelta > KINDA_SMALL_NUMBER &&
		SplineAboveTargetMode ==
			EVRExpGrabbableSplineAboveTargetMode::RiseVerticallyThenApproach)
	{
		SplineApproachPhase = ESplineApproachPhase::Rising;
		return;
	}

	SplineApproachPhase = ESplineApproachPhase::Approaching;
}

void UVRExpGrabbableMotionComponent::ResetSplineApproach()
{
	SplineApproachFallSpeed = 0.0f;
	SplineApproachPhase = ESplineApproachPhase::None;
}

bool UVRExpGrabbableMotionComponent::TickKinematicSplineApproach(
	float DeltaTime,
	float ApproachSpeed,
	bool bSmoothApproach)
{
	if (SplineApproachPhase == ESplineApproachPhase::None ||
		!FMath::IsFinite(DeltaTime) ||
		DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float ArrivalThreshold =
		FMath::IsFinite(SplineArrivalThreshold)
			? FMath::Max(SplineArrivalThreshold, KINDA_SMALL_NUMBER)
			: KINDA_SMALL_NUMBER;
	const FVector CurrentLocation = GetMotionBaseLocation();
	const FVector UpAxis = GetKinematicUpAxisInMotionSpace();
	if (FVector::Dist(CurrentLocation, SplineApproachTargetLocation) <=
		ArrivalThreshold)
	{
		return true;
	}

	if (SplineApproachPhase == ESplineApproachPhase::Falling)
	{
		const float GravityAcceleration =
			ResolveKinematicGravityAcceleration();
		const float EffectiveMaxFallSpeed =
			FMath::IsFinite(MaxFallSpeed)
				? FMath::Max(MaxFallSpeed, 0.0f)
				: 0.0f;
		if (GravityAcceleration <= KINDA_SMALL_NUMBER ||
			EffectiveMaxFallSpeed <= KINDA_SMALL_NUMBER)
		{
			SplineApproachFallSpeed = 0.0f;
			SplineApproachPhase = ESplineApproachPhase::Approaching;
		}
		else
		{
			const float PreviousFallSpeed =
				FMath::Clamp(
					SplineApproachFallSpeed,
					0.0f,
					EffectiveMaxFallSpeed);
			float NewFallSpeed = PreviousFallSpeed;
			float FallDistance = 0.0f;

			if (PreviousFallSpeed >= EffectiveMaxFallSpeed)
			{
				FallDistance = EffectiveMaxFallSpeed * DeltaTime;
			}
			else
			{
				const float TimeToMaxSpeed =
					(EffectiveMaxFallSpeed - PreviousFallSpeed) /
					GravityAcceleration;
				if (TimeToMaxSpeed >= DeltaTime)
				{
					NewFallSpeed =
						PreviousFallSpeed + GravityAcceleration * DeltaTime;
					FallDistance =
						PreviousFallSpeed * DeltaTime +
						0.5f * GravityAcceleration * FMath::Square(DeltaTime);
				}
				else
				{
					NewFallSpeed = EffectiveMaxFallSpeed;
					FallDistance =
						PreviousFallSpeed * TimeToMaxSpeed +
						0.5f * GravityAcceleration * FMath::Square(TimeToMaxSpeed) +
						EffectiveMaxFallSpeed * (DeltaTime - TimeToMaxSpeed);
				}
			}

			SplineApproachFallSpeed = NewFallSpeed;
			FVector NewLocation = CurrentLocation;
			const float HeightToTarget = FVector::DotProduct(
				CurrentLocation - SplineApproachTargetLocation,
				UpAxis);
			if (SplineFallMode ==
				EVRExpGrabbableSplineFallMode::ApproachWhileFalling)
			{
				const FVector PlanarTarget =
					SplineApproachTargetLocation +
					UpAxis * HeightToTarget;
				const FVector PlanarLocation =
					InterpolateSplineApproach(
						CurrentLocation,
						PlanarTarget,
						DeltaTime,
						ApproachSpeed,
						bSmoothApproach);
				NewLocation = PlanarLocation;
			}
			float MotionSpaceFallDistance = FallDistance;
			if (IsUsingParentRelativeMotionSpace() &&
				AttachedMotionDirectionMode ==
					EVRExpGrabbableAttachedMotionDirectionMode::PreserveWorldDirections)
			{
				MotionSpaceFallDistance = ConvertWorldVectorToMotionSpace(
					FVector::UpVector * FallDistance).Size();
			}
			NewLocation -= UpAxis * FMath::Min(
				MotionSpaceFallDistance,
				FMath::Max(HeightToTarget, 0.0f));

			const FQuat NewRotation =
				ResolveSplineApproachRotation(
					CurrentLocation,
					NewLocation,
					true);
			MoveUpdatedComponentTo(
				NewLocation,
				NewRotation,
				DeltaTime);

			if (SplineApproachPhase == ESplineApproachPhase::None)
			{
				return false;
			}

			const FVector ActualLocation = GetMotionBaseLocation();
			if (FVector::DotProduct(
					ActualLocation - SplineApproachTargetLocation,
					UpAxis) <= KINDA_SMALL_NUMBER)
			{
				SplineApproachFallSpeed = 0.0f;
				SplineApproachPhase = ESplineApproachPhase::Approaching;
			}
			return FVector::Dist(
				ActualLocation,
				SplineApproachTargetLocation) <= ArrivalThreshold;
		}
	}

	if (SplineApproachPhase == ESplineApproachPhase::Rising)
	{
		const float HeightDelta = FVector::DotProduct(
			SplineApproachTargetLocation - CurrentLocation,
			UpAxis);
		const FVector VerticalTarget =
			CurrentLocation + UpAxis * HeightDelta;
		FVector NewLocation =
			InterpolateSplineApproach(
				CurrentLocation,
				VerticalTarget,
				DeltaTime,
				ApproachSpeed,
				bSmoothApproach);
		if (FVector::DotProduct(
				SplineApproachTargetLocation - NewLocation,
				UpAxis) <= ArrivalThreshold)
		{
			NewLocation = VerticalTarget;
		}

		const FQuat NewRotation =
			ResolveSplineApproachRotation(
				CurrentLocation,
				NewLocation,
				true);
		MoveUpdatedComponentTo(
			NewLocation,
			NewRotation,
			DeltaTime);

		if (SplineApproachPhase == ESplineApproachPhase::None)
		{
			return false;
		}

		const FVector ActualLocation = GetMotionBaseLocation();
		if (FVector::DotProduct(
				SplineApproachTargetLocation - ActualLocation,
				UpAxis) <= KINDA_SMALL_NUMBER)
		{
			SplineApproachPhase = ESplineApproachPhase::Approaching;
		}
		return FVector::Dist(
			ActualLocation,
			SplineApproachTargetLocation) <= ArrivalThreshold;
	}

	const FVector NewLocation =
		InterpolateSplineApproach(
			CurrentLocation,
			SplineApproachTargetLocation,
			DeltaTime,
			ApproachSpeed,
			bSmoothApproach);
	const FQuat NewRotation =
		ResolveSplineApproachRotation(
			CurrentLocation,
			NewLocation,
			false);
	MoveUpdatedComponentTo(
		NewLocation,
		NewRotation,
		DeltaTime);
	if (SplineApproachPhase == ESplineApproachPhase::None)
	{
		return false;
	}

	return FVector::Dist(
		GetMotionBaseLocation(),
		SplineApproachTargetLocation) <= ArrivalThreshold;
}

float UVRExpGrabbableMotionComponent::ResolveKinematicGravityAcceleration() const
{
	float GravityAcceleration = 0.0f;
	if (KinematicGravitySource ==
		EVRExpGrabbableKinematicGravitySource::WorldGravity)
	{
		const float SafeGravityScale =
			FMath::IsFinite(WorldGravityScale)
				? FMath::Max(WorldGravityScale, 0.0f)
				: 0.0f;
		GravityAcceleration = FMath::Abs(GetGravityZ()) * SafeGravityScale;
	}
	else
	{
		GravityAcceleration =
			FMath::IsFinite(CustomGravityAcceleration)
				? FMath::Max(CustomGravityAcceleration, 0.0f)
				: 0.0f;
	}

	return FMath::IsFinite(GravityAcceleration)
		? GravityAcceleration
		: 0.0f;
}

FVector UVRExpGrabbableMotionComponent::InterpolateSplineApproach(
	const FVector& CurrentLocation,
	const FVector& TargetLocation,
	float DeltaTime,
	float ApproachSpeed,
	bool bSmoothApproach) const
{
	const float SafeApproachSpeed =
		FMath::IsFinite(ApproachSpeed)
			? FMath::Max(ApproachSpeed, 0.0f)
			: 0.0f;
	return bSmoothApproach
		? FMath::VInterpTo(
			CurrentLocation,
			TargetLocation,
			DeltaTime,
			SafeApproachSpeed / 100.0f)
		: FMath::VInterpConstantTo(
			CurrentLocation,
			TargetLocation,
			DeltaTime,
			SafeApproachSpeed);
}

FQuat UVRExpGrabbableMotionComponent::ResolveSplineApproachRotation(
	const FVector& CurrentLocation,
	const FVector& TargetLocation,
	bool bIsHeightPhase)
{
	if (bIsHeightPhase && !bOrientDuringSplineHeightMotion)
	{
		return GetMotionBaseQuat();
	}

	const FVector Direction =
		(TargetLocation - CurrentLocation).GetSafeNormal();
	return Direction.IsNearlyZero()
		? GetMotionBaseQuat()
		: ApplyOrientationAdjustment(Direction.ToOrientationQuat());
}

void UVRExpGrabbableMotionComponent::GenerateNewWanderTarget()
{
	const FVector RandomDirection = FMath::VRand();
	const float RandomDistance = FMath::FRandRange(0.0f, WanderRadius);
	FTransform OriginWorld =
		ResolveMotionAnchorWorldTransform(WanderOriginAnchor);
	FVector WorldOffset = RandomDirection * RandomDistance;
	if (WanderOriginAnchor.bRelativeToReferenceParent &&
		AttachedMotionDirectionMode ==
			EVRExpGrabbableAttachedMotionDirectionMode::FollowAttachmentParent)
	{
		if (USceneComponent* ReferenceParent =
				WanderOriginAnchor.ReferenceParent.Get())
		{
			WorldOffset = ReferenceParent->GetComponentTransform()
				.TransformVectorNoScale(WorldOffset);
		}
	}

	FTransform TargetWorld = OriginWorld;
	TargetWorld.SetLocation(OriginWorld.GetLocation() + WorldOffset);
	if (WanderOriginAnchor.bRelativeToReferenceParent &&
		WanderOriginAnchor.ReferenceParent.IsValid())
	{
		WanderTargetAnchor = CaptureMotionAnchor(
			TargetWorld,
			WanderOriginAnchor.ReferenceParent.Get(),
			WanderOriginAnchor.ReferenceSocketName);
	}
	else
	{
		WanderTargetAnchor = FMotionAnchor();
		WanderTargetAnchor.RelativeTransform = TargetWorld;
		WanderTargetAnchor.LastValidWorldTransform = TargetWorld;
	}
	WanderOrigin = ResolveMotionAnchorTransform(
		WanderOriginAnchor).GetLocation();
	CurrentWanderTarget = ResolveMotionAnchorTransform(
		WanderTargetAnchor).GetLocation();
}

void UVRExpGrabbableMotionComponent::RefreshCurrentMotionSpace(
	bool bForceRefresh)
{
	USceneComponent* NewParent = IsValid(UpdatedComponent)
		? UpdatedComponent->GetAttachParent()
		: nullptr;
	if (!IsValid(NewParent))
	{
		NewParent = nullptr;
	}
	const FName NewSocketName = IsValid(NewParent) && IsValid(UpdatedComponent)
		? UpdatedComponent->GetAttachSocketName()
		: NAME_None;

	const bool bParentChanged =
		bCurrentMotionSpaceIsRelative != IsValid(NewParent) ||
		CurrentMotionParentComponent.IsStale() ||
		CurrentMotionParentComponent.Get() != NewParent ||
		CurrentMotionParentSocketName != NewSocketName;
	if (bParentChanged)
	{
		// 当前最终姿态已经包含旧空间的效果偏移；清空记录后以该姿态作为新空间基础，避免切换父级时跳变。
		BakeMotionEffectsIntoBase();
		CurrentMotionParentComponent = NewParent;
		CurrentMotionParentSocketName = NewSocketName;
		bCurrentMotionSpaceIsRelative = IsValid(NewParent);
	}

	CurrentMotionParentWorldTransform = IsValid(NewParent)
		? NewParent->GetSocketTransform(
			CurrentMotionParentSocketName,
			ERelativeTransformSpace::RTS_World)
		: FTransform::Identity;

	if (bParentChanged || bForceRefresh)
	{
		RebuildMotionTickPrerequisites();
	}
}

void UVRExpGrabbableMotionComponent::RebuildMotionTickPrerequisites()
{
	TArray<UActorComponent*> DesiredComponents;
	TArray<AActor*> DesiredActors;

	auto AddSourceChain =
		[this, &DesiredComponents, &DesiredActors](USceneComponent* Source)
		{
			while (IsValid(Source))
			{
				// UpdatedComponent 的后代依赖本组件，反向添加会形成 Tick 循环。
				if (Source == UpdatedComponent ||
					(IsValid(UpdatedComponent) && Source->IsAttachedTo(UpdatedComponent)))
				{
					break;
				}

				DesiredComponents.AddUnique(Source);
				AActor* SourceOwner = Source->GetOwner();
				if (IsValid(SourceOwner) && SourceOwner != GetOwner())
				{
					DesiredActors.AddUnique(SourceOwner);
				}
				Source = Source->GetAttachParent();
			}
		};

	AddSourceChain(CurrentMotionParentComponent.Get());
	AddSourceChain(SplineToFollow.Get());
	AddSourceChain(TargetToFollow.Get());
	AddSourceChain(OrbitCenter.Get());
	AddSourceChain(FlyToTargetComponent.Get());
	AddSourceChain(InitialMotionAnchor.ReferenceParent.Get());
	AddSourceChain(WanderOriginAnchor.ReferenceParent.Get());
	AddSourceChain(WanderTargetAnchor.ReferenceParent.Get());

	bool bMatchesCurrent =
		MotionTickPrerequisiteComponents.Num() == DesiredComponents.Num() &&
		MotionTickPrerequisiteActors.Num() == DesiredActors.Num();
	if (bMatchesCurrent)
	{
		for (int32 Index = 0; Index < DesiredComponents.Num(); ++Index)
		{
			if (MotionTickPrerequisiteComponents[Index].Get() !=
				DesiredComponents[Index])
			{
				bMatchesCurrent = false;
				break;
			}
		}
	}
	if (bMatchesCurrent)
	{
		for (int32 Index = 0; Index < DesiredActors.Num(); ++Index)
		{
			if (MotionTickPrerequisiteActors[Index].Get() != DesiredActors[Index])
			{
				bMatchesCurrent = false;
				break;
			}
		}
	}
	if (bMatchesCurrent)
	{
		return;
	}

	ClearMotionTickPrerequisites();
	for (UActorComponent* Prerequisite : DesiredComponents)
	{
		AddTickPrerequisiteComponent(Prerequisite);
		MotionTickPrerequisiteComponents.Add(Prerequisite);
	}
	for (AActor* PrerequisiteActor : DesiredActors)
	{
		AddTickPrerequisiteActor(PrerequisiteActor);
		MotionTickPrerequisiteActors.Add(PrerequisiteActor);
	}
}

void UVRExpGrabbableMotionComponent::ClearMotionTickPrerequisites()
{
	for (const TWeakObjectPtr<UActorComponent>& Prerequisite :
		MotionTickPrerequisiteComponents)
	{
		if (UActorComponent* Component = Prerequisite.Get())
		{
			RemoveTickPrerequisiteComponent(Component);
		}
	}
	for (const TWeakObjectPtr<AActor>& Prerequisite :
		MotionTickPrerequisiteActors)
	{
		if (AActor* Actor = Prerequisite.Get())
		{
			RemoveTickPrerequisiteActor(Actor);
		}
	}
	MotionTickPrerequisiteComponents.Reset();
	MotionTickPrerequisiteActors.Reset();
}

bool UVRExpGrabbableMotionComponent::IsUsingParentRelativeMotionSpace() const
{
	return bCurrentMotionSpaceIsRelative &&
		IsValid(UpdatedComponent) &&
		CurrentMotionParentComponent.IsValid() &&
		UpdatedComponent->GetAttachParent() ==
			CurrentMotionParentComponent.Get() &&
		UpdatedComponent->GetAttachSocketName() ==
			CurrentMotionParentSocketName;
}

FVector UVRExpGrabbableMotionComponent::ConvertWorldLocationToMotionSpace(
	const FVector& WorldLocation) const
{
	return IsUsingParentRelativeMotionSpace()
		? CurrentMotionParentWorldTransform.InverseTransformPosition(
			WorldLocation)
		: WorldLocation;
}

FQuat UVRExpGrabbableMotionComponent::ConvertWorldRotationToMotionSpace(
	const FQuat& WorldRotation) const
{
	return IsUsingParentRelativeMotionSpace()
		? (CurrentMotionParentWorldTransform.GetRotation().Inverse() *
			WorldRotation).GetNormalized()
		: WorldRotation;
}

FVector UVRExpGrabbableMotionComponent::ConvertWorldVectorToMotionSpace(
	const FVector& WorldVector) const
{
	return IsUsingParentRelativeMotionSpace()
		? CurrentMotionParentWorldTransform.InverseTransformVector(WorldVector)
		: WorldVector;
}

FVector UVRExpGrabbableMotionComponent::ConvertMotionLocationToWorld(
	const FVector& MotionLocation) const
{
	return IsUsingParentRelativeMotionSpace()
		? CurrentMotionParentWorldTransform.TransformPosition(MotionLocation)
		: MotionLocation;
}

FQuat UVRExpGrabbableMotionComponent::ConvertMotionRotationToWorld(
	const FQuat& MotionRotation) const
{
	return IsUsingParentRelativeMotionSpace()
		? (CurrentMotionParentWorldTransform.GetRotation() *
			MotionRotation).GetNormalized()
		: MotionRotation;
}

FVector UVRExpGrabbableMotionComponent::GetConfiguredDirectionInMotionSpace(
	const FVector& Direction) const
{
	if (IsUsingParentRelativeMotionSpace() &&
		AttachedMotionDirectionMode ==
			EVRExpGrabbableAttachedMotionDirectionMode::PreserveWorldDirections)
	{
		return ConvertWorldVectorToMotionSpace(Direction).GetSafeNormal();
	}
	return Direction.GetSafeNormal();
}

FVector UVRExpGrabbableMotionComponent::GetKinematicUpAxisInMotionSpace() const
{
	return GetConfiguredDirectionInMotionSpace(FVector::UpVector);
}

UVRExpGrabbableMotionComponent::FMotionAnchor
UVRExpGrabbableMotionComponent::CaptureMotionAnchor(
	const FTransform& WorldTransform,
	USceneComponent* ExplicitReferenceParent,
	FName ExplicitReferenceSocketName) const
{
	FMotionAnchor Anchor;
	Anchor.LastValidWorldTransform = WorldTransform;
	const bool bUseUpdatedComponentParent =
		!IsValid(ExplicitReferenceParent);
	USceneComponent* ReferenceParent = IsValid(ExplicitReferenceParent)
		? ExplicitReferenceParent
		: (IsValid(UpdatedComponent)
			? UpdatedComponent->GetAttachParent()
			: nullptr);
	if (IsValid(ReferenceParent))
	{
		Anchor.ReferenceParent = ReferenceParent;
		Anchor.ReferenceSocketName = bUseUpdatedComponentParent &&
			IsValid(UpdatedComponent)
			? UpdatedComponent->GetAttachSocketName()
			: ExplicitReferenceSocketName;
		const FTransform ReferenceWorldTransform =
			ReferenceParent->GetSocketTransform(
				Anchor.ReferenceSocketName,
				ERelativeTransformSpace::RTS_World);
		Anchor.RelativeTransform = WorldTransform.GetRelativeTransform(
			ReferenceWorldTransform);
		Anchor.bRelativeToReferenceParent = true;
	}
	else
	{
		Anchor.RelativeTransform = WorldTransform;
	}
	return Anchor;
}

FTransform UVRExpGrabbableMotionComponent::ResolveMotionAnchorWorldTransform(
	FMotionAnchor& Anchor)
{
	if (Anchor.bRelativeToReferenceParent)
	{
		if (USceneComponent* ReferenceParent = Anchor.ReferenceParent.Get())
		{
			Anchor.LastValidWorldTransform =
				Anchor.RelativeTransform *
				ReferenceParent->GetSocketTransform(
					Anchor.ReferenceSocketName,
					ERelativeTransformSpace::RTS_World);
		}
		return Anchor.LastValidWorldTransform;
	}

	Anchor.LastValidWorldTransform = Anchor.RelativeTransform;
	return Anchor.LastValidWorldTransform;
}

FTransform UVRExpGrabbableMotionComponent::ResolveMotionAnchorTransform(
	FMotionAnchor& Anchor)
{
	const FTransform WorldTransform =
		ResolveMotionAnchorWorldTransform(Anchor);
	return FTransform(
		ConvertWorldRotationToMotionSpace(WorldTransform.GetRotation()),
		ConvertWorldLocationToMotionSpace(WorldTransform.GetLocation()),
		WorldTransform.GetScale3D());
}

void UVRExpGrabbableMotionComponent::PrepareForKinematicMotion()
{
	if (bDisablePhysicsDuringKinematicMotion)
	{
		SetUpdatedPrimitiveSimulatePhysics(false);
	}
}

void UVRExpGrabbableMotionComponent::SetUpdatedPrimitiveSimulatePhysics(bool bSimulate)
{
	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(UpdatedComponent);
	if (!Primitive)
	{
		return;
	}

	if (Primitive->IsSimulatingPhysics() != bSimulate)
	{
		Primitive->SetSimulatePhysics(bSimulate);
	}

	if (bSimulate)
	{
		Primitive->WakeAllRigidBodies();
	}
}

bool UVRExpGrabbableMotionComponent::HasConfiguredMotionEffects() const
{
	return FloatingEffect.bEnableFloating ||
		(bUpdateRotationDuringMotion && RotationEffect.bEnableRotationEffect);
}

bool UVRExpGrabbableMotionComponent::IsMotionEffectState(EVRExpGrabbableMotionState State)
{
	return State == EVRExpGrabbableMotionState::NormalMotion ||
		State == EVRExpGrabbableMotionState::Releasing;
}

bool UVRExpGrabbableMotionComponent::IsFloatingEffectActive() const
{
	return IsMotionEffectState(CurrentMotionState) && FloatingEffect.bEnableFloating;
}

bool UVRExpGrabbableMotionComponent::IsRotationEffectActive() const
{
	return IsMotionEffectState(CurrentMotionState) && bUpdateRotationDuringMotion && RotationEffect.bEnableRotationEffect;
}

bool UVRExpGrabbableMotionComponent::HasActiveMotionEffects() const
{
	return IsFloatingEffectActive() || IsRotationEffectActive();
}

void UVRExpGrabbableMotionComponent::RefreshMotionEffectRuntimeState()
{
	const bool bFloatingEnabled = IsFloatingEffectActive();
	if (bFloatingEffectWasEnabled != bFloatingEnabled ||
		(bFloatingEnabled && CachedFloatingEffectSpace != FloatingEffect.Space))
	{
		ResetFloatingEffectTracking();
	}

	const bool bRotationActive = IsRotationEffectActive();
	if (bRotationEffectWasActive != bRotationActive ||
		(bRotationActive && CachedRotationEffectSpace != RotationEffect.Space))
	{
		ResetRotationEffectTracking();
	}

	bFloatingEffectWasEnabled = bFloatingEnabled;
	bRotationEffectWasActive = bRotationActive;
	CachedFloatingEffectSpace = FloatingEffect.Space;
	CachedRotationEffectSpace = RotationEffect.Space;
}

void UVRExpGrabbableMotionComponent::AdvanceMotionEffects(float DeltaTime)
{
	if (!IsMotionEffectState(CurrentMotionState))
	{
		return;
	}

	if (IsFloatingEffectActive())
	{
		const float SafeFrequency = FMath::Max(FloatingEffect.Frequency, 0.0f);
		FloatingEffectPhase = FMath::Fmod(FloatingEffectPhase + 2.0f * PI * SafeFrequency * DeltaTime, 2.0f * PI);
	}

	if (IsRotationEffectActive())
	{
		const FQuat DeltaRotation = (RotationEffect.RotationRate * DeltaTime).Quaternion();
		if (RotationEffect.Space == EVRExpGrabbableMotionEffectSpace::Local)
		{
			AccumulatedRotationEffect = (AccumulatedRotationEffect * DeltaRotation).GetNormalized();
		}
		else
		{
			AccumulatedRotationEffect = (DeltaRotation * AccumulatedRotationEffect).GetNormalized();
		}
	}
}

void UVRExpGrabbableMotionComponent::ResetFloatingEffectTracking()
{
	FloatingEffectPhase = 0.0f;
	AppliedFloatingOffset = FVector::ZeroVector;
}

void UVRExpGrabbableMotionComponent::ResetRotationEffectTracking()
{
	AccumulatedRotationEffect = FQuat::Identity;
	AppliedRotationEffect = FQuat::Identity;
	AppliedRotationEffectSpace = RotationEffect.Space;
}

void UVRExpGrabbableMotionComponent::BakeMotionEffectsIntoBase()
{
	// 不改动 UpdatedComponent；清空效果记录后，当前最终姿态会成为新的逻辑基础姿态。
	ResetFloatingEffectTracking();
	ResetRotationEffectTracking();
	bFloatingEffectWasEnabled = false;
	bRotationEffectWasActive = false;
	CachedFloatingEffectSpace = FloatingEffect.Space;
	CachedRotationEffectSpace = RotationEffect.Space;
}

FVector UVRExpGrabbableMotionComponent::CalculateFloatingOffset(const FQuat& FinalRotation)
{
	if (!IsFloatingEffectActive())
	{
		return FVector::ZeroVector;
	}

	const FVector SafeAxis = FloatingEffect.Axis.GetSafeNormal();
	if (SafeAxis.IsNearlyZero())
	{
		if (!bWarnedAboutInvalidFloatingAxis)
		{
			UE_LOG(LogVRExpGrabbableMotion, Warning, TEXT("VRExpGrabbableMotionComponent: FloatingEffect.Axis must not be a zero vector. Floating was skipped."));
			bWarnedAboutInvalidFloatingAxis = true;
		}
		return FVector::ZeroVector;
	}

	const float SafeAmplitude = FMath::Max(FloatingEffect.Amplitude, 0.0f);
	const float OffsetDistance =
		FMath::Sin(FloatingEffectPhase) * SafeAmplitude;
	if (FloatingEffect.Space == EVRExpGrabbableMotionEffectSpace::Local)
	{
		return FinalRotation.RotateVector(SafeAxis) * OffsetDistance;
	}
	if (IsUsingParentRelativeMotionSpace() &&
		AttachedMotionDirectionMode ==
			EVRExpGrabbableAttachedMotionDirectionMode::PreserveWorldDirections)
	{
		return ConvertWorldVectorToMotionSpace(SafeAxis * OffsetDistance);
	}
	return SafeAxis * OffsetDistance;
}

FQuat UVRExpGrabbableMotionComponent::ApplyRotationEffect(const FQuat& BaseRotation) const
{
	if (!IsRotationEffectActive())
	{
		return BaseRotation;
	}

	if (RotationEffect.Space == EVRExpGrabbableMotionEffectSpace::Local)
	{
		return (BaseRotation * AccumulatedRotationEffect).GetNormalized();
	}
	if (IsUsingParentRelativeMotionSpace() &&
		AttachedMotionDirectionMode ==
			EVRExpGrabbableAttachedMotionDirectionMode::PreserveWorldDirections)
	{
		const FQuat WorldBaseRotation =
			ConvertMotionRotationToWorld(BaseRotation);
		return ConvertWorldRotationToMotionSpace(
			(AccumulatedRotationEffect * WorldBaseRotation)
				.GetNormalized());
	}
	return (AccumulatedRotationEffect * BaseRotation).GetNormalized();
}

FQuat UVRExpGrabbableMotionComponent::GetSplineMotionRotation(float DistanceAlongSpline)
{
	if (!bUpdateRotationDuringMotion || !IsValid(SplineToFollow))
	{
		return GetMotionBaseQuat();
	}

	FQuat WorldRotation =
		SplineToFollow->GetQuaternionAtDistanceAlongSpline(
			DistanceAlongSpline,
			ESplineCoordinateSpace::World);
	if (bReverseDirection)
	{
		WorldRotation =
			(WorldRotation * FQuat(FVector::UpVector, PI)).GetNormalized();
	}
	return ApplyOrientationAdjustment(
		ConvertWorldRotationToMotionSpace(WorldRotation));
}

FQuat UVRExpGrabbableMotionComponent::ApplyOrientationAdjustment(const FQuat& BaseRotation)
{
	if (!bUpdateRotationDuringMotion)
	{
		return GetMotionBaseQuat();
	}

	if (!OrientationAdjustment.bEnableRotationOffset && !OrientationAdjustment.bEnableAxisAdjustment)
	{
		return BaseRotation;
	}

	FQuat ResultRotation = BaseRotation;
	if (OrientationAdjustment.bEnableRotationOffset)
	{
		ResultRotation = (ResultRotation * OrientationAdjustment.RotationOffset.Quaternion()).GetNormalized();
	}

	if (OrientationAdjustment.bEnableAxisAdjustment)
	{
		FQuat AxisCorrection = FQuat::Identity;
		if (TryBuildAxisCorrection(OrientationAdjustment, AxisCorrection))
		{
			ResultRotation = (ResultRotation * AxisCorrection).GetNormalized();
		}
		else if (!bWarnedAboutInvalidOrientationAxes)
		{
			UE_LOG(LogVRExpGrabbableMotion, Warning, TEXT("VRExpGrabbableMotionComponent: ForwardAxis and UpAxis must not be parallel or opposite. Axis adjustment was skipped."));
			bWarnedAboutInvalidOrientationAxes = true;
		}
	}

	return ResultRotation;
}

FVector UVRExpGrabbableMotionComponent::GetAxisVector(EVRExpGrabbableMotionAxis Axis)
{
	switch (Axis)
	{
	case EVRExpGrabbableMotionAxis::X:
		return FVector::ForwardVector;
	case EVRExpGrabbableMotionAxis::Y:
		return FVector::RightVector;
	case EVRExpGrabbableMotionAxis::Z:
		return FVector::UpVector;
	case EVRExpGrabbableMotionAxis::NegativeX:
		return -FVector::ForwardVector;
	case EVRExpGrabbableMotionAxis::NegativeY:
		return -FVector::RightVector;
	case EVRExpGrabbableMotionAxis::NegativeZ:
		return -FVector::UpVector;
	default:
		return FVector::ForwardVector;
	}
}

bool UVRExpGrabbableMotionComponent::TryBuildAxisCorrection(const FVRExpGrabbableOrientationAdjustment& Adjustment, FQuat& OutAxisCorrection)
{
	const FVector ForwardVector = GetAxisVector(Adjustment.ForwardAxis);
	const FVector UpVector = GetAxisVector(Adjustment.UpAxis);
	if (FMath::Abs(FVector::DotProduct(ForwardVector, UpVector)) > 1.0f - KINDA_SMALL_NUMBER)
	{
		OutAxisCorrection = FQuat::Identity;
		return false;
	}

	const FQuat ModelBasisRotation = FRotationMatrix::MakeFromXZ(ForwardVector, UpVector).ToQuat();
	OutAxisCorrection = ModelBasisRotation.Inverse().GetNormalized();
	return true;
}

bool UVRExpGrabbableMotionComponent::MoveUpdatedComponentTo(
	const FVector& TargetLocation,
	const FQuat& TargetRotation,
	float DeltaTime,
	bool bForceNoSweep,
	ETeleportType Teleport,
	bool bForceSweep,
	bool bCompleteReleaseOnBlockingHit)
{
	if (!IsValid(UpdatedComponent))
	{
		return false;
	}

	const bool bWasReleaseMotion =
		CurrentMotionState == EVRExpGrabbableMotionState::Releasing;

	// 调用者按缓存空间生成了目标；若计算与写入之间父级发生变化，先保留目标世界姿态，再转入最新父级空间。
	const bool bHadCachedRelativeSpace =
		bCurrentMotionSpaceIsRelative;
	const FVector RequestedWorldLocation = bHadCachedRelativeSpace
		? CurrentMotionParentWorldTransform.TransformPosition(TargetLocation)
		: TargetLocation;
	const FQuat RequestedWorldRotation = bHadCachedRelativeSpace
		? (CurrentMotionParentWorldTransform.GetRotation() * TargetRotation)
			.GetNormalized()
		: TargetRotation;
	// 即使父指针未变，也重新读取移动父级或 Socket 的最新世界变换。
	RefreshCurrentMotionSpace();
	FVector ResolvedTargetLocation =
		ConvertWorldLocationToMotionSpace(RequestedWorldLocation);
	FQuat ResolvedTargetRotation =
		ConvertWorldRotationToMotionSpace(RequestedWorldRotation);

	RefreshMotionEffectRuntimeState();
	const bool bApplyMotionEffects = IsMotionEffectState(CurrentMotionState);
	const bool bUseRelativeSpace = IsUsingParentRelativeMotionSpace();
	const FVector StartWorldLocation =
		UpdatedComponent->GetComponentLocation();
	const FVector PreviousFloatingOffset = AppliedFloatingOffset;
	const FQuat BaseCurrentRotation = GetMotionBaseQuat();
	const FQuat BaseTargetRotation = bUpdateRotationDuringMotion
		? ResolvedTargetRotation
		: BaseCurrentRotation;
	const FQuat EffectiveTargetRotation = bApplyMotionEffects ? ApplyRotationEffect(BaseTargetRotation) : BaseTargetRotation;
	const FVector NewFloatingOffset = bApplyMotionEffects ? CalculateFloatingOffset(EffectiveTargetRotation) : FVector::ZeroVector;
	const FVector EffectiveTargetLocation =
		ResolvedTargetLocation + NewFloatingOffset;
	const FVector EffectiveWorldLocation =
		ConvertMotionLocationToWorld(EffectiveTargetLocation);
	const FQuat EffectiveWorldRotation =
		ConvertMotionRotationToWorld(EffectiveTargetRotation);
	const FVector WorldDelta =
		EffectiveWorldLocation - StartWorldLocation;
	const bool bSweepRequested =
		!bForceNoSweep && (bForceSweep || bSweepMovement);
	const bool bUseSweep = bSweepRequested && UpdatedPrimitive != nullptr;
	if (bSweepRequested && !UpdatedPrimitive && !bWarnedAboutSweepWithoutPrimitive)
	{
		UE_LOG(
			LogVRExpGrabbableMotion,
			Warning,
			TEXT("VRExpGrabbableMotionComponent: swept movement requires UpdatedComponent to be a PrimitiveComponent. Falling back to non-swept movement."));
		bWarnedAboutSweepWithoutPrimitive = true;
	}

	bMovementAppliedThisTick = true;
	if (bApplyMotionEffects && IsRotationEffectActive())
	{
		AppliedRotationEffectSpace = RotationEffect.Space;
		AppliedRotationEffect = RotationEffect.Space ==
			EVRExpGrabbableMotionEffectSpace::Local
			? (BaseTargetRotation.Inverse() * EffectiveTargetRotation)
				.GetNormalized()
			: (EffectiveTargetRotation * BaseTargetRotation.Inverse())
				.GetNormalized();
	}
	else
	{
		AppliedRotationEffect = FQuat::Identity;
	}

	FHitResult Hit;
	bool bMoved = false;
	if (bUseSweep)
	{
		// SafeMoveUpdatedComponent 仅接受世界增量；命中后的实际 RelativeTransform 由 SceneComponent 自动维护。
		bMoved = SafeMoveUpdatedComponent(
			WorldDelta,
			EffectiveWorldRotation,
			true,
			Hit,
			Teleport);
	}
	else if (bUseRelativeSpace)
	{
		UpdatedComponent->SetRelativeLocationAndRotation(
			EffectiveTargetLocation,
			EffectiveTargetRotation,
			false,
			&Hit,
			Teleport);
		bMoved = true;
	}
	else
	{
		bMoved = MoveUpdatedComponent(
			WorldDelta,
			EffectiveWorldRotation,
			false,
			&Hit,
			Teleport);
	}

	const FVector ActualWorldDelta =
		UpdatedComponent->GetComponentLocation() - StartWorldLocation;
	Velocity = DeltaTime > KINDA_SMALL_NUMBER
		? ActualWorldDelta / DeltaTime
		: FVector::ZeroVector;
	UpdateComponentVelocity();

	// Sweep 被阻挡时只记录实际完成的浮动份额，避免下一帧从实际位置减去完整目标偏移。
	const float CompletedMovementFraction = bUseSweep && Hit.IsValidBlockingHit()
		? FMath::Clamp(Hit.Time, 0.0f, 1.0f)
		: 1.0f;
	AppliedFloatingOffset = FMath::Lerp(PreviousFloatingOffset, NewFloatingOffset, CompletedMovementFraction);

	if (bUseSweep && Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, WorldDelta);
		OnMovementBlocked.Broadcast(Hit);

		if (bWasReleaseMotion && bCompleteReleaseOnBlockingHit)
		{
			FinishReleaseMotion(false);
		}
		else if (bStopOnBlockingHit)
		{
			StopMotion();
		}
	}

	return bMoved;
}

FTransform UVRExpGrabbableMotionComponent::GetUpdatedComponentTransform() const
{
	return IsValid(UpdatedComponent) ? UpdatedComponent->GetComponentTransform() : FTransform::Identity;
}

FVector UVRExpGrabbableMotionComponent::GetUpdatedComponentLocation() const
{
	return IsValid(UpdatedComponent) ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
}

FQuat UVRExpGrabbableMotionComponent::GetUpdatedComponentQuat() const
{
	return IsValid(UpdatedComponent) ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
}

FVector UVRExpGrabbableMotionComponent::GetMotionBaseLocation() const
{
	if (!IsValid(UpdatedComponent))
	{
		return FVector::ZeroVector;
	}
	const FVector ActualLocation = IsUsingParentRelativeMotionSpace()
		? UpdatedComponent->GetRelativeLocation()
		: UpdatedComponent->GetComponentLocation();
	return ActualLocation - AppliedFloatingOffset;
}

FQuat UVRExpGrabbableMotionComponent::GetMotionBaseQuat() const
{
	if (!IsValid(UpdatedComponent))
	{
		return FQuat::Identity;
	}

	const FQuat ActualRotation = IsUsingParentRelativeMotionSpace()
		? UpdatedComponent->GetRelativeTransform().GetRotation()
		: UpdatedComponent->GetComponentQuat();
	const FQuat InverseEffect = AppliedRotationEffect.Inverse();
	return AppliedRotationEffectSpace == EVRExpGrabbableMotionEffectSpace::Local
		? (ActualRotation * InverseEffect).GetNormalized()
		: (InverseEffect * ActualRotation).GetNormalized();
}
