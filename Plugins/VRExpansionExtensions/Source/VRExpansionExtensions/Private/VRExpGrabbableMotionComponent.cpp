// Fill out your copyright notice in the Description page of Project Settings.

#include "VRExpGrabbableMotionComponent.h"

#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Math/RotationMatrix.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogVRExpGrabbableMotion, Log, All);

UVRExpGrabbableMotionComponent::UVRExpGrabbableMotionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NormalMotionMode(EVRExpGrabbableNormalMotionMode::None)
	, ReleaseMotionMode(EVRExpGrabbableReleaseMotionMode::None)
	, bAutoStartNormalMotion(true)
	, bPauseNormalMotionWhenGrabbed(true)
	, bUpdateRotationDuringMotion(true)
	, bSweepMovement(false)
	, bStopOnBlockingHit(true)
	, bDisablePhysicsDuringKinematicMotion(true)
	, bAutoBindGripControllers(true)
	, bMatchOwnerActor(true)
	, bMatchUpdatedComponent(true)
	, bMatchOwnerComponents(true)
	, bUseManualSplineReference(false)
	, bUseSplineActor(true)
	, SplineActor(nullptr)
	, SplineComponentName(NAME_None)
	, SplineToFollow(nullptr)
	, bTeleportToSplineOnStart(true)
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
	, ReturnSpeed(500.0f)
	, bSmoothReturn(true)
	, ReturnCurve(nullptr)
	, FlyToTargetComponent(nullptr)
	, CurrentMotionState(EVRExpGrabbableMotionState::Idle)
	, MotionStateBeforePause(EVRExpGrabbableMotionState::Idle)
	, InitialTransform(FTransform::Identity)
	, WanderOrigin(FVector::ZeroVector)
	, CurrentWanderTarget(FVector::ZeroVector)
	, ReturnStartLocation(FVector::ZeroVector)
	, ReturnStartRotation(FRotator::ZeroRotator)
	, ReturnToSplineTargetLocation(FVector::ZeroVector)
	, ReturnToSplineTargetProgress(0.0f)
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
	InitialTransform = GetUpdatedComponentTransform();
	WanderOrigin = GetUpdatedComponentLocation();

	if (bAutoBindGripControllers)
	{
		RefreshGripControllerBindings();
	}
	else
	{
		UnbindGripControllers();
		for (UGripMotionControllerComponent* GripController : ManualGripControllers)
		{
			BindGripController(GripController);
		}
	}

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

void UVRExpGrabbableMotionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindGripControllers();
	ActiveGrips.Reset();
	SetComponentTickEnabled(false);

	Super::EndPlay(EndPlayReason);
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
	NormalMotionMode = NewMode;

	if (CurrentMotionState == EVRExpGrabbableMotionState::NormalMotion)
	{
		StartNormalMotion();
	}
}

void UVRExpGrabbableMotionComponent::SetReleaseMotionMode(EVRExpGrabbableReleaseMotionMode NewMode)
{
	ReleaseMotionMode = NewMode;
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
	Velocity = FVector::ZeroVector;
	UpdateComponentVelocity();
	SetMotionState(EVRExpGrabbableMotionState::Idle);
	SetComponentTickEnabled(false);
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

void UVRExpGrabbableMotionComponent::RefreshGripControllerBindings()
{
	UnbindGripControllers();

	for (UGripMotionControllerComponent* GripController : ManualGripControllers)
	{
		BindGripController(GripController);
	}

	if (!bAutoBindGripControllers)
	{
		RebuildActiveGripsFromControllers();
		if (ActiveGrips.Num() > 0 && bPauseNormalMotionWhenGrabbed)
		{
			SetMotionState(EVRExpGrabbableMotionState::Grabbed);
			SetComponentTickEnabled(false);
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		RebuildActiveGripsFromControllers();
		return;
	}

	for (TObjectIterator<UGripMotionControllerComponent> It; It; ++It)
	{
		UGripMotionControllerComponent* GripController = *It;
		if (!IsValid(GripController) ||
			GripController->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) ||
			GripController->GetWorld() != World)
		{
			continue;
		}

		BindGripController(GripController);
	}

	RebuildActiveGripsFromControllers();
	if (ActiveGrips.Num() > 0 && bPauseNormalMotionWhenGrabbed)
	{
		SetMotionState(EVRExpGrabbableMotionState::Grabbed);
		SetComponentTickEnabled(false);
	}
}

void UVRExpGrabbableMotionComponent::HandleControllerGrip(const FBPActorGripInformation& GripInformation)
{
	if (!DoesGripMatchTarget(GripInformation))
	{
		return;
	}

	RebuildActiveGripsFromControllers();

	if (ActiveGrips.Num() > 0 && bPauseNormalMotionWhenGrabbed)
	{
		SetMotionState(EVRExpGrabbableMotionState::Grabbed);
		SetComponentTickEnabled(false);
	}
}

void UVRExpGrabbableMotionComponent::HandleControllerDrop(const FBPActorGripInformation& GripInformation, bool bWasSocketed)
{
	if (!DoesGripMatchTarget(GripInformation))
	{
		return;
	}

	bool bHadMovementAuthority = false;
	for (const FVRExpActiveGrip& ActiveGrip : ActiveGrips)
	{
		if (ActiveGrip.GripID == GripInformation.GripID || ActiveGrip.bHadMovementAuthority)
		{
			bHadMovementAuthority = bHadMovementAuthority || ActiveGrip.bHadMovementAuthority;
		}
	}

	RebuildActiveGripsFromControllers();
	const bool bIsFinalGrip = ActiveGrips.Num() == 0;

	if (bIsFinalGrip)
	{
		if (bWasSocketed || !bHadMovementAuthority)
		{
			StopMotion();
		}
		else
		{
			StartReleaseMotionFromDrop();
		}
	}

	// 蓝图释放逻辑最后执行，避免 ReleaseMotionMode::None 的 StopMotion 覆盖蓝图自定义行为。
	OnGripReleased.Broadcast(GripInformation, bWasSocketed, bIsFinalGrip, bHadMovementAuthority);
}

void UVRExpGrabbableMotionComponent::ResolveUpdatedComponent()
{
	if (IsValid(UpdatedComponent))
	{
		return;
	}

	if (AActor* Owner = GetOwner())
	{
		SetUpdatedComponent(Owner->GetRootComponent());
	}
}

void UVRExpGrabbableMotionComponent::BindGripController(UGripMotionControllerComponent* GripController)
{
	if (!IsValid(GripController) || BoundGripControllers.Contains(GripController))
	{
		return;
	}

	GripController->OnGrippedObject.AddDynamic(this, &UVRExpGrabbableMotionComponent::HandleControllerGrip);
	GripController->OnDroppedObject.AddDynamic(this, &UVRExpGrabbableMotionComponent::HandleControllerDrop);
	BoundGripControllers.Add(GripController);
}

void UVRExpGrabbableMotionComponent::UnbindGripControllers()
{
	for (UGripMotionControllerComponent* GripController : BoundGripControllers)
	{
		if (!IsValid(GripController))
		{
			continue;
		}

		GripController->OnGrippedObject.RemoveDynamic(this, &UVRExpGrabbableMotionComponent::HandleControllerGrip);
		GripController->OnDroppedObject.RemoveDynamic(this, &UVRExpGrabbableMotionComponent::HandleControllerDrop);
	}

	BoundGripControllers.Reset();
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

void UVRExpGrabbableMotionComponent::AddActiveGrip(UGripMotionControllerComponent* GripController, const FBPActorGripInformation& GripInformation, bool bHasMovementAuthority)
{
	if (!IsValid(GripController) || GripInformation.GripID == INVALID_VRGRIP_ID)
	{
		return;
	}

	for (FVRExpActiveGrip& ActiveGrip : ActiveGrips)
	{
		if (ActiveGrip.Controller == GripController && ActiveGrip.GripID == GripInformation.GripID)
		{
			ActiveGrip.bHadMovementAuthority = ActiveGrip.bHadMovementAuthority || bHasMovementAuthority;
			return;
		}
	}

	FVRExpActiveGrip NewActiveGrip;
	NewActiveGrip.Controller = GripController;
	NewActiveGrip.GripID = GripInformation.GripID;
	NewActiveGrip.bHadMovementAuthority = bHasMovementAuthority;
	ActiveGrips.Add(NewActiveGrip);
}

void UVRExpGrabbableMotionComponent::RebuildActiveGripsFromControllers()
{
	ActiveGrips.Reset();

	for (UGripMotionControllerComponent* GripController : BoundGripControllers)
	{
		if (!IsValid(GripController))
		{
			continue;
		}

		TArray<FBPActorGripInformation> ControllerGrips;
		GripController->GetAllGrips(ControllerGrips);
		for (const FBPActorGripInformation& ControllerGrip : ControllerGrips)
		{
			if (DoesGripMatchTarget(ControllerGrip))
			{
				AddActiveGrip(GripController, ControllerGrip, GripController->HasGripMovementAuthority(ControllerGrip));
			}
		}
	}
}

void UVRExpGrabbableMotionComponent::PruneInvalidActiveGrips()
{
	for (int32 Index = ActiveGrips.Num() - 1; Index >= 0; --Index)
	{
		if (!ActiveGrips[Index].Controller.IsValid())
		{
			ActiveGrips.RemoveAtSwap(Index);
		}
	}
}

void UVRExpGrabbableMotionComponent::SetMotionState(EVRExpGrabbableMotionState NewState)
{
	if (CurrentMotionState == NewState)
	{
		return;
	}

	const bool bWasEffectState = IsMotionEffectState(CurrentMotionState);
	const bool bWillBeEffectState = IsMotionEffectState(NewState);
	if (bWasEffectState != bWillBeEffectState)
	{
		BakeMotionEffectsIntoBase();
	}

	CurrentMotionState = NewState;
	OnMotionStateChanged.Broadcast(NewState);
}

void UVRExpGrabbableMotionComponent::StartNormalMotion()
{
	ResolveUpdatedComponent();
	if (!IsValid(UpdatedComponent))
	{
		StopMotion();
		return;
	}

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

		if (bTeleportToSplineOnStart)
		{
			CurrentSplineProgress = bReverseDirection ? SplineToFollow->GetSplineLength() : 0.0f;
			const FVector StartLocation = SplineToFollow->GetLocationAtDistanceAlongSpline(CurrentSplineProgress, ESplineCoordinateSpace::World);
			const FQuat StartRotation = GetSplineMotionRotation(CurrentSplineProgress);
			MoveUpdatedComponentTo(StartLocation, StartRotation, 0.0f, true, ETeleportType::TeleportPhysics);
			bIsMovingToSpline = false;
		}
		else
		{
			bIsMovingToSpline = true;
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
		WanderOrigin = GetMotionBaseLocation();
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
	ResolveUpdatedComponent();
	if (!IsValid(UpdatedComponent))
	{
		StopMotion();
		return;
	}

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

		if (IsValid(SplineToFollow))
		{
			const FVector CurrentLocation = GetMotionBaseLocation();
			const float ClosestInputKey = SplineToFollow->FindInputKeyClosestToWorldLocation(CurrentLocation);
			ReturnToSplineTargetLocation = SplineToFollow->GetLocationAtSplineInputKey(ClosestInputKey, ESplineCoordinateSpace::World);
			ReturnToSplineTargetProgress = SplineToFollow->GetDistanceAlongSplineAtSplineInputKey(ClosestInputKey);
		}
		StartReleaseMotion();
		break;
	case EVRExpGrabbableReleaseMotionMode::ContinueMotion:
		StartNormalMotion();
		break;
	case EVRExpGrabbableReleaseMotionMode::FallWithGravity:
		SetUpdatedPrimitiveSimulatePhysics(true);
		StopMotion();
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
	OnReturnMotionCompleted.Broadcast();

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
	switch (ReleaseMotionMode)
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
	default:
		break;
	}
}

void UVRExpGrabbableMotionComponent::TickSplineMotion(float DeltaTime)
{
	if (!IsValid(SplineToFollow))
	{
		return;
	}

	const FVector CurrentLocation = GetMotionBaseLocation();

	if (bIsMovingToSpline)
	{
		const float ClosestInputKey = SplineToFollow->FindInputKeyClosestToWorldLocation(CurrentLocation);
		const FVector ClosestLocation = SplineToFollow->GetLocationAtSplineInputKey(ClosestInputKey, ESplineCoordinateSpace::World);
		const float Distance = FVector::Dist(CurrentLocation, ClosestLocation);

		if (Distance <= SplineArrivalThreshold)
		{
			CurrentSplineProgress = SplineToFollow->GetDistanceAlongSplineAtSplineInputKey(ClosestInputKey);
			bIsMovingToSpline = false;
			MoveUpdatedComponentTo(ClosestLocation, GetMotionBaseQuat(), DeltaTime);
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
			const FVector EndLocation = SplineToFollow->GetLocationAtDistanceAlongSpline(CurrentSplineProgress, ESplineCoordinateSpace::World);
			const FQuat EndRotation = GetSplineMotionRotation(CurrentSplineProgress);
			MoveUpdatedComponentTo(EndLocation, EndRotation, DeltaTime);
			StopMotion();
			return;
		}
	}

	const FVector NewLocation = SplineToFollow->GetLocationAtDistanceAlongSpline(CurrentSplineProgress, ESplineCoordinateSpace::World);
	const FQuat NewRotation = GetSplineMotionRotation(CurrentSplineProgress);
	MoveUpdatedComponentTo(NewLocation, NewRotation, DeltaTime);
}

void UVRExpGrabbableMotionComponent::TickFollowMotion(float DeltaTime)
{
	if (!IsValid(TargetToFollow))
	{
		return;
	}

	const FVector CurrentLocation = GetMotionBaseLocation();
	const FVector TargetLocation = TargetToFollow->GetComponentLocation();
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
		return;
	}

	CurrentOrbitAngle += OrbitSpeed * DeltaTime;
	if (CurrentOrbitAngle >= 360.0f || CurrentOrbitAngle <= -360.0f)
	{
		CurrentOrbitAngle = FMath::Fmod(CurrentOrbitAngle, 360.0f);
	}

	const float RadAngle = FMath::DegreesToRadians(CurrentOrbitAngle);
	FVector Offset(FMath::Cos(RadAngle) * OrbitRadius, FMath::Sin(RadAngle) * OrbitRadius, OrbitHeightOffset);

	const FVector SafeOrbitAxis = OrbitAxis.GetSafeNormal();
	if (!SafeOrbitAxis.IsNearlyZero() && !SafeOrbitAxis.Equals(FVector::UpVector))
	{
		const FQuat AxisRotation = FQuat::FindBetweenNormals(FVector::UpVector, SafeOrbitAxis);
		Offset = AxisRotation.RotateVector(Offset);
	}

	const FVector NewLocation = OrbitCenter->GetComponentLocation() + Offset;
	const FVector DirectionToCenter = (OrbitCenter->GetComponentLocation() - NewLocation).GetSafeNormal();
	const FQuat NewRotation = DirectionToCenter.IsNearlyZero() ? GetMotionBaseQuat() : ApplyOrientationAdjustment(DirectionToCenter.ToOrientationQuat());
	MoveUpdatedComponentTo(NewLocation, NewRotation, DeltaTime);
}

void UVRExpGrabbableMotionComponent::TickWanderMotion(float DeltaTime)
{
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
	const FVector TargetLocation = InitialTransform.GetLocation();
	const float Distance = FVector::Dist(CurrentLocation, TargetLocation);

	if (Distance < 1.0f)
	{
		MoveUpdatedComponentTo(TargetLocation, InitialTransform.GetRotation(), DeltaTime, true, ETeleportType::TeleportPhysics);
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

	const FQuat TargetRotation = InitialTransform.GetRotation();
	const FQuat NewRotation = bSmoothReturn
		? FMath::QInterpTo(GetMotionBaseQuat(), TargetRotation, DeltaTime, InterpSpeed)
		: FMath::QInterpConstantTo(GetMotionBaseQuat(), TargetRotation, DeltaTime, ReturnSpeed);

	MoveUpdatedComponentTo(NewLocation, NewRotation, DeltaTime);
}

void UVRExpGrabbableMotionComponent::TickReturnToOrigin(float DeltaTime)
{
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
		StartNormalMotion();
		return;
	}

	if (bTeleportToSplineOnRelease)
	{
		CurrentSplineProgress = ReturnToSplineTargetProgress;
		bIsMovingToSpline = false;
		MoveUpdatedComponentTo(ReturnToSplineTargetLocation, GetMotionBaseQuat(), DeltaTime, true, ETeleportType::TeleportPhysics);
		FinishReleaseMotion(true);
		return;
	}

	const FVector CurrentLocation = GetMotionBaseLocation();
	const float Distance = FVector::Dist(CurrentLocation, ReturnToSplineTargetLocation);

	if (Distance <= SplineArrivalThreshold)
	{
		CurrentSplineProgress = ReturnToSplineTargetProgress;
		bIsMovingToSpline = false;
		MoveUpdatedComponentTo(ReturnToSplineTargetLocation, GetMotionBaseQuat(), DeltaTime, true, ETeleportType::TeleportPhysics);
		FinishReleaseMotion(true);
		return;
	}

	const FVector NewLocation = bSmoothReturn
		? FMath::VInterpTo(CurrentLocation, ReturnToSplineTargetLocation, DeltaTime, ReturnSpeed / 100.0f)
		: FMath::VInterpConstantTo(CurrentLocation, ReturnToSplineTargetLocation, DeltaTime, ReturnSpeed);

	const FVector Direction = (ReturnToSplineTargetLocation - CurrentLocation).GetSafeNormal();
	const FQuat NewRotation = Direction.IsNearlyZero() ? GetMotionBaseQuat() : ApplyOrientationAdjustment(Direction.ToOrientationQuat());
	MoveUpdatedComponentTo(NewLocation, NewRotation, DeltaTime);
}

void UVRExpGrabbableMotionComponent::TickFlyToTarget(float DeltaTime)
{
	if (!IsValid(FlyToTargetComponent))
	{
		StopMotion();
		return;
	}

	const FVector CurrentLocation = GetMotionBaseLocation();
	const FVector TargetLocation = FlyToTargetComponent->GetComponentLocation();
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

bool UVRExpGrabbableMotionComponent::InitializeSplineReference()
{
	SplineToFollow = nullptr;

	if (bUseManualSplineReference)
	{
		if (bUseSplineActor)
		{
			if (IsValid(SplineActor))
			{
				SplineToFollow = SplineActor->FindComponentByClass<USplineComponent>();
			}
		}
		else if (SplineComponentName != NAME_None)
		{
			if (AActor* Owner = GetOwner())
			{
				TArray<USplineComponent*> SplineComponents;
				Owner->GetComponents<USplineComponent>(SplineComponents);
				for (USplineComponent* SplineComponent : SplineComponents)
				{
					if (IsValid(SplineComponent) && SplineComponent->GetFName() == SplineComponentName)
					{
						SplineToFollow = SplineComponent;
						break;
					}
				}
			}
		}

		return IsValid(SplineToFollow);
	}

	if (AActor* Owner = GetOwner())
	{
		SplineToFollow = Owner->FindComponentByClass<USplineComponent>();
	}

	return IsValid(SplineToFollow);
}

void UVRExpGrabbableMotionComponent::GenerateNewWanderTarget()
{
	const FVector RandomDirection = FMath::VRand();
	const float RandomDistance = FMath::FRandRange(0.0f, WanderRadius);
	CurrentWanderTarget = WanderOrigin + RandomDirection * RandomDistance;
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

bool UVRExpGrabbableMotionComponent::ShouldSweepMovement() const
{
	if (!bSweepMovement)
	{
		return false;
	}

	if (UpdatedPrimitive)
	{
		return true;
	}

	if (!bWarnedAboutSweepWithoutPrimitive)
	{
		UE_LOG(LogVRExpGrabbableMotion, Warning, TEXT("VRExpGrabbableMotionComponent: bSweepMovement requires UpdatedComponent to be a PrimitiveComponent. Falling back to non-swept movement."));
		bWarnedAboutSweepWithoutPrimitive = true;
	}

	return false;
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

	const FVector WorldAxis = FloatingEffect.Space == EVRExpGrabbableMotionEffectSpace::Local
		? FinalRotation.RotateVector(SafeAxis)
		: SafeAxis;
	const float SafeAmplitude = FMath::Max(FloatingEffect.Amplitude, 0.0f);
	return WorldAxis * (FMath::Sin(FloatingEffectPhase) * SafeAmplitude);
}

FQuat UVRExpGrabbableMotionComponent::ApplyRotationEffect(const FQuat& BaseRotation) const
{
	if (!IsRotationEffectActive())
	{
		return BaseRotation;
	}

	return RotationEffect.Space == EVRExpGrabbableMotionEffectSpace::Local
		? (BaseRotation * AccumulatedRotationEffect).GetNormalized()
		: (AccumulatedRotationEffect * BaseRotation).GetNormalized();
}

FQuat UVRExpGrabbableMotionComponent::GetSplineMotionRotation(float DistanceAlongSpline)
{
	FQuat SplineRotation = SplineToFollow->GetQuaternionAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World);
	if (bReverseDirection)
	{
		// 绕 Spline 本地上轴旋转 180 度，使 +X 前向与实际反向移动方向一致，同时保留 Spline 的俯仰和横滚。
		SplineRotation = (SplineRotation * FQuat(FVector::UpVector, PI)).GetNormalized();
	}

	return ApplyOrientationAdjustment(SplineRotation);
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

bool UVRExpGrabbableMotionComponent::MoveUpdatedComponentTo(const FVector& TargetLocation, const FQuat& TargetRotation, float DeltaTime, bool bForceNoSweep, ETeleportType Teleport)
{
	if (!IsValid(UpdatedComponent))
	{
		return false;
	}

	RefreshMotionEffectRuntimeState();
	const bool bApplyMotionEffects = IsMotionEffectState(CurrentMotionState);
	const FVector StartLocation = UpdatedComponent->GetComponentLocation();
	const FVector PreviousFloatingOffset = AppliedFloatingOffset;
	const FQuat BaseCurrentRotation = GetMotionBaseQuat();
	const FQuat BaseTargetRotation = bUpdateRotationDuringMotion ? TargetRotation : BaseCurrentRotation;
	const FQuat EffectiveTargetRotation = bApplyMotionEffects ? ApplyRotationEffect(BaseTargetRotation) : BaseTargetRotation;
	const FVector NewFloatingOffset = bApplyMotionEffects ? CalculateFloatingOffset(EffectiveTargetRotation) : FVector::ZeroVector;
	const FVector EffectiveTargetLocation = TargetLocation + NewFloatingOffset;
	const FVector Delta = EffectiveTargetLocation - StartLocation;
	const bool bUseSweep = !bForceNoSweep && ShouldSweepMovement();

	bMovementAppliedThisTick = true;
	if (bApplyMotionEffects && IsRotationEffectActive())
	{
		AppliedRotationEffect = AccumulatedRotationEffect;
		AppliedRotationEffectSpace = RotationEffect.Space;
	}
	else
	{
		AppliedRotationEffect = FQuat::Identity;
	}

	FHitResult Hit;
	const bool bMoved = bUseSweep
		? SafeMoveUpdatedComponent(Delta, EffectiveTargetRotation, true, Hit, Teleport)
		: MoveUpdatedComponent(Delta, EffectiveTargetRotation, false, &Hit, Teleport);

	const FVector ActualDelta = UpdatedComponent->GetComponentLocation() - StartLocation;
	Velocity = DeltaTime > KINDA_SMALL_NUMBER ? ActualDelta / DeltaTime : FVector::ZeroVector;
	UpdateComponentVelocity();

	// Sweep 被阻挡时只记录实际完成的浮动份额，避免下一帧从实际位置减去完整目标偏移。
	const float CompletedMovementFraction = bUseSweep && Hit.IsValidBlockingHit()
		? FMath::Clamp(Hit.Time, 0.0f, 1.0f)
		: 1.0f;
	AppliedFloatingOffset = FMath::Lerp(PreviousFloatingOffset, NewFloatingOffset, CompletedMovementFraction);

	if (bUseSweep && Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Delta);
		OnMovementBlocked.Broadcast(Hit);

		if (bStopOnBlockingHit)
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
	return IsValid(UpdatedComponent)
		? UpdatedComponent->GetComponentLocation() - AppliedFloatingOffset
		: FVector::ZeroVector;
}

FQuat UVRExpGrabbableMotionComponent::GetMotionBaseQuat() const
{
	if (!IsValid(UpdatedComponent))
	{
		return FQuat::Identity;
	}

	const FQuat ActualRotation = UpdatedComponent->GetComponentQuat();
	const FQuat InverseEffect = AppliedRotationEffect.Inverse();
	return AppliedRotationEffectSpace == EVRExpGrabbableMotionEffectSpace::Local
		? (ActualRotation * InverseEffect).GetNormalized()
		: (InverseEffect * ActualRotation).GetNormalized();
}
