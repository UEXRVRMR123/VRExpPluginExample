// Fill out your copyright notice in the Description page of Project Settings.

#include "VRExpPuzzleInsertionComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ShapeComponent.h"
#include "GameFramework/Actor.h"
#include "GripMotionControllerComponent.h"
#include "Grippables/GrippableStaticMeshActor.h"
#include "VRGripInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogVRExpPuzzleInsertion, Log, All);

namespace VRExpPuzzleInsertion
{
	struct FValidatedGrip
	{
		TWeakObjectPtr<UGripMotionControllerComponent> Controller;
		FBPActorGripInformation GripInformation;
	};

	bool IsAnimatedState(const EVRExpPuzzleInsertionState State)
	{
		return State == EVRExpPuzzleInsertionState::Approaching ||
			State == EVRExpPuzzleInsertionState::AligningUp ||
			State == EVRExpPuzzleInsertionState::Inserting;
	}

	FVector GetLocalAxisVector(const EVRExpGrabbableMotionAxis Axis)
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
			return FVector::UpVector;
		}
	}
}

UVRExpPuzzleInsertionComponent::UVRExpPuzzleInsertionComponent()
	: PuzzleActor(nullptr)
	, ReleaseConfirmationTimeout(0.5f)
	, bAttachToGuideOnComplete(true)
	, bRestartPuzzleMotionOnReset(true)
	, PuzzleMotionComponent(nullptr)
	, ResolvedTriggerComponent(nullptr)
	, PuzzleRootPrimitive(nullptr)
	, CurrentState(EVRExpPuzzleInsertionState::Idle)
	, InitialAttachSocketName(NAME_None)
	, InitialTriggerCollisionEnabled(ECollisionEnabled::NoCollision)
	, bInitialPuzzleSimulatesPhysics(false)
	, bInitialPuzzleDenyGripping(false)
	, bInitialTriggerGenerateOverlapEvents(false)
	, bInitialStateCached(false)
	, bDenyGrippingBeforeAttempt(false)
	, bTriggerOverlapBeforeAttempt(false)
	, bLoggedInitializationFailure(false)
	, bLoggedTriggerConfigurationWarning(false)
	, bLoggedResetWhileHeld(false)
	, ReleaseWaitElapsed(0.0f)
	, PhaseElapsed(0.0f)
	, PhaseStartLocation(FVector::ZeroVector)
	, PhaseStartRotation(FQuat::Identity)
	, ApproachLocation(FVector::ZeroVector)
	, ApproachRotation(FQuat::Identity)
	, FinalInsertionLocation(FVector::ZeroVector)
	, FinalInsertionRotation(FQuat::Identity)
	, InsertionAxisWorld(FVector::ZeroVector)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bAutoActivate = true;
	SetIsReplicatedByDefault(false);
}

void UVRExpPuzzleInsertionComponent::BeginPlay()
{
	Super::BeginPlay();

	SetComponentTickEnabled(false);
	ResolveAndCacheReferences();
}

void UVRExpPuzzleInsertionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(ResolvedTriggerComponent))
	{
		ResolvedTriggerComponent->OnComponentBeginOverlap.RemoveDynamic(this, &UVRExpPuzzleInsertionComponent::HandleTriggerBeginOverlap);
	}

	if (IsValid(PuzzleMotionComponent))
	{
		PuzzleMotionComponent->OnGripReleased.RemoveDynamic(this, &UVRExpPuzzleInsertionComponent::HandlePuzzleGripReleased);
	}

	Super::EndPlay(EndPlayReason);
}

void UVRExpPuzzleInsertionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CurrentState == EVRExpPuzzleInsertionState::WaitingForRelease)
	{
		if (!QueryPuzzleHeld())
		{
			BeginInsertionMotion();
			return;
		}

		ReleaseWaitElapsed += FMath::Max(0.0f, DeltaTime);
		if (ReleaseWaitElapsed >= FMath::Max(0.0f, ReleaseConfirmationTimeout))
		{
			FailInsertion(TEXT("Timed out while waiting for the puzzle's final Grip to release."));
		}
		return;
	}

	if (VRExpPuzzleInsertion::IsAnimatedState(CurrentState))
	{
		AdvanceInsertion(DeltaTime);
	}
}

bool UVRExpPuzzleInsertionComponent::TryStartInsertion()
{
	if (CurrentState != EVRExpPuzzleInsertionState::Idle)
	{
		return false;
	}

	if (!ResolveAndCacheReferences())
	{
		OnInsertionFailed.Broadcast();
		return false;
	}

	if (!QueryPuzzleHeld())
	{
		return false;
	}

	bDenyGrippingBeforeAttempt = IVRGripInterface::Execute_DenyGripping(PuzzleActor, nullptr);
	bTriggerOverlapBeforeAttempt = ResolvedTriggerComponent->GetGenerateOverlapEvents();

	ResolvedTriggerComponent->SetGenerateOverlapEvents(false);
	PuzzleActor->SetDenyGripping(true);
	ReleaseWaitElapsed = 0.0f;
	SetInsertionState(EVRExpPuzzleInsertionState::WaitingForRelease);
	SetComponentTickEnabled(true);

	if (!RequestReleaseAllGrips())
	{
		if (CurrentState == EVRExpPuzzleInsertionState::WaitingForRelease)
		{
			FailInsertion(TEXT("Failed to release every Grip with local Grip Authority."));
			return false;
		}

		return CurrentState != EVRExpPuzzleInsertionState::Idle;
	}

	if (CurrentState == EVRExpPuzzleInsertionState::WaitingForRelease &&
		ReleaseConfirmationTimeout <= 0.0f && QueryPuzzleHeld())
	{
		FailInsertion(TEXT("The puzzle is still held and ReleaseConfirmationTimeout is zero."));
		return false;
	}

	return CurrentState != EVRExpPuzzleInsertionState::Idle;
}

bool UVRExpPuzzleInsertionComponent::ResetInsertion()
{
	if (!ResolveAndCacheReferences())
	{
		return false;
	}

	if (QueryPuzzleHeld())
	{
		if (!bLoggedResetWhileHeld)
		{
			UE_LOG(LogVRExpPuzzleInsertion, Warning,
				TEXT("%s: ResetInsertion was ignored because puzzle '%s' is still held."),
				*GetNameSafe(GetOwner()), *GetNameSafe(PuzzleActor));
			bLoggedResetWhileHeld = true;
		}
		return false;
	}

	bLoggedResetWhileHeld = false;
	SetComponentTickEnabled(false);
	ReleaseWaitElapsed = 0.0f;
	PhaseElapsed = 0.0f;

	PuzzleMotionComponent->StopMotion();
	StopPuzzlePhysics();
	RestoreInitialAttachmentAndTransform();
	PuzzleActor->SetDenyGripping(bInitialPuzzleDenyGripping);

	ResolvedTriggerComponent->SetCollisionEnabled(InitialTriggerCollisionEnabled);
	ResolvedTriggerComponent->SetGenerateOverlapEvents(bInitialTriggerGenerateOverlapEvents);

	if (bInitialPuzzleSimulatesPhysics && IsValid(PuzzleRootPrimitive))
	{
		PuzzleRootPrimitive->SetSimulatePhysics(true);
	}

	SetInsertionState(EVRExpPuzzleInsertionState::Idle);

	if (bRestartPuzzleMotionOnReset)
	{
		PuzzleMotionComponent->StartMotion();
	}

	OnInsertionReset.Broadcast();
	return true;
}

void UVRExpPuzzleInsertionComponent::SetPuzzleActor(AGrippableStaticMeshActor* NewPuzzleActor)
{
	if (CurrentState != EVRExpPuzzleInsertionState::Idle)
	{
		UE_LOG(LogVRExpPuzzleInsertion, Warning,
			TEXT("%s: SetPuzzleActor was ignored while an insertion is active or completed. Call ResetInsertion first."),
			*GetNameSafe(GetOwner()));
		return;
	}

	if (PuzzleActor != NewPuzzleActor)
	{
		if (IsValid(PuzzleMotionComponent))
		{
			PuzzleMotionComponent->OnGripReleased.RemoveDynamic(this, &UVRExpPuzzleInsertionComponent::HandlePuzzleGripReleased);
		}

		PuzzleActor = NewPuzzleActor;
		PuzzleMotionComponent = nullptr;
		PuzzleRootPrimitive = nullptr;
		bInitialStateCached = false;
		bLoggedInitializationFailure = false;
		bLoggedResetWhileHeld = false;
	}

	if (HasBegunPlay() && IsValid(PuzzleActor))
	{
		ResolveAndCacheReferences();
	}
}

void UVRExpPuzzleInsertionComponent::HandleTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (OverlappedComponent != ResolvedTriggerComponent ||
		OtherActor != PuzzleActor ||
		CurrentState != EVRExpPuzzleInsertionState::Idle)
	{
		return;
	}

	// 未抓取的拼图可能因浮动或外力进入触发体，此时不接管。
	if (!QueryPuzzleHeld())
	{
		return;
	}

	TryStartInsertion();
}

void UVRExpPuzzleInsertionComponent::HandlePuzzleGripReleased(
	const FBPActorGripInformation& GripInformation,
	bool bWasSocketed,
	bool bIsFinalGrip,
	bool bHadMovementAuthority)
{
	if (CurrentState == EVRExpPuzzleInsertionState::WaitingForRelease &&
		bIsFinalGrip && !QueryPuzzleHeld())
	{
		// VRExpGrabbableMotionComponent 已先处理其 ReleaseMotionMode；这里随后 StopMotion 并接管。
		BeginInsertionMotion();
	}
}

bool UVRExpPuzzleInsertionComponent::ResolveAndCacheReferences()
{
	if (!IsValid(PuzzleActor))
	{
		if (!bLoggedInitializationFailure)
		{
			UE_LOG(LogVRExpPuzzleInsertion, Warning,
				TEXT("%s: PuzzleActor is not assigned."), *GetNameSafe(GetOwner()));
			bLoggedInitializationFailure = true;
		}
		return false;
	}

	UVRExpGrabbableMotionComponent* FoundMotionComponent = PuzzleActor->FindComponentByClass<UVRExpGrabbableMotionComponent>();
	if (!IsValid(FoundMotionComponent))
	{
		if (!bLoggedInitializationFailure)
		{
			UE_LOG(LogVRExpPuzzleInsertion, Warning,
				TEXT("%s: puzzle '%s' has no VRExpGrabbableMotionComponent."),
				*GetNameSafe(GetOwner()), *GetNameSafe(PuzzleActor));
			bLoggedInitializationFailure = true;
		}
		return false;
	}

	UPrimitiveComponent* FoundPuzzleRoot = Cast<UPrimitiveComponent>(PuzzleActor->GetRootComponent());
	if (!IsValid(FoundPuzzleRoot))
	{
		if (!bLoggedInitializationFailure)
		{
			UE_LOG(LogVRExpPuzzleInsertion, Warning,
				TEXT("%s: puzzle '%s' does not have a PrimitiveComponent root."),
				*GetNameSafe(GetOwner()), *GetNameSafe(PuzzleActor));
			bLoggedInitializationFailure = true;
		}
		return false;
	}

	UPrimitiveComponent* FoundTrigger = nullptr;
	if (AActor* GuideOwner = GetOwner())
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		GuideOwner->GetComponents(PrimitiveComponents);

		const auto IsConfiguredOverlapTrigger = [FoundPuzzleRoot](const UPrimitiveComponent* Candidate)
		{
			return IsValid(Candidate) &&
				Candidate->GetCollisionEnabled() == ECollisionEnabled::QueryOnly &&
				Candidate->GetGenerateOverlapEvents() &&
				Candidate->GetCollisionResponseToChannel(FoundPuzzleRoot->GetCollisionObjectType()) == ECR_Overlap;
		};

		// 优先选取已经正确配置的 Box、Sphere 或 Capsule 等形状碰撞组件。
		for (UPrimitiveComponent* Candidate : PrimitiveComponents)
		{
			if (IsValid(Candidate) && Candidate->IsA<UShapeComponent>() && IsConfiguredOverlapTrigger(Candidate))
			{
				FoundTrigger = Candidate;
				break;
			}
		}

		// 如果碰撞尚未配置完整，仍优先选第一个形状碰撞组件，以便输出明确的配置警告。
		if (!IsValid(FoundTrigger))
		{
			for (UPrimitiveComponent* Candidate : PrimitiveComponents)
			{
				if (IsValid(Candidate) && Candidate->IsA<UShapeComponent>())
				{
					FoundTrigger = Candidate;
					break;
				}
			}
		}

		if (!IsValid(FoundTrigger))
		{
			for (UPrimitiveComponent* Candidate : PrimitiveComponents)
			{
				if (IsConfiguredOverlapTrigger(Candidate))
				{
					FoundTrigger = Candidate;
					break;
				}
			}
		}

		if (!IsValid(FoundTrigger))
		{
			for (UPrimitiveComponent* Candidate : PrimitiveComponents)
			{
				if (IsValid(Candidate))
				{
					FoundTrigger = Candidate;
					break;
				}
			}
		}
	}

	if (!IsValid(FoundTrigger))
	{
		if (!bLoggedInitializationFailure)
		{
			UE_LOG(LogVRExpPuzzleInsertion, Warning,
				TEXT("%s: no trigger PrimitiveComponent was found on the guide Actor."), *GetNameSafe(GetOwner()));
			bLoggedInitializationFailure = true;
		}
		return false;
	}

	if (ResolvedTriggerComponent != FoundTrigger)
	{
		if (IsValid(ResolvedTriggerComponent))
		{
			ResolvedTriggerComponent->OnComponentBeginOverlap.RemoveDynamic(this, &UVRExpPuzzleInsertionComponent::HandleTriggerBeginOverlap);
		}

		ResolvedTriggerComponent = FoundTrigger;
		ResolvedTriggerComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &UVRExpPuzzleInsertionComponent::HandleTriggerBeginOverlap);
	}

	if (PuzzleMotionComponent != FoundMotionComponent)
	{
		if (IsValid(PuzzleMotionComponent))
		{
			PuzzleMotionComponent->OnGripReleased.RemoveDynamic(this, &UVRExpPuzzleInsertionComponent::HandlePuzzleGripReleased);
		}

		PuzzleMotionComponent = FoundMotionComponent;
		PuzzleMotionComponent->OnGripReleased.AddUniqueDynamic(this, &UVRExpPuzzleInsertionComponent::HandlePuzzleGripReleased);
	}

	PuzzleRootPrimitive = FoundPuzzleRoot;
	bLoggedInitializationFailure = false;

	if (!bInitialStateCached)
	{
		InitialPuzzleWorldTransform = PuzzleActor->GetActorTransform();
		InitialPuzzleRelativeTransform = PuzzleActor->GetRootComponent()->GetRelativeTransform();
		InitialAttachParent = PuzzleActor->GetRootComponent()->GetAttachParent();
		InitialAttachSocketName = PuzzleActor->GetRootComponent()->GetAttachSocketName();
		bInitialPuzzleSimulatesPhysics = PuzzleRootPrimitive->IsSimulatingPhysics();
		bInitialPuzzleDenyGripping = IVRGripInterface::Execute_DenyGripping(PuzzleActor, nullptr);
		InitialTriggerCollisionEnabled = ResolvedTriggerComponent->GetCollisionEnabled();
		bInitialTriggerGenerateOverlapEvents = ResolvedTriggerComponent->GetGenerateOverlapEvents();
		bInitialStateCached = true;
	}

	if (CurrentState == EVRExpPuzzleInsertionState::Idle &&
		(ResolvedTriggerComponent->GetCollisionEnabled() != ECollisionEnabled::QueryOnly ||
		!ResolvedTriggerComponent->GetGenerateOverlapEvents() ||
		ResolvedTriggerComponent->GetCollisionResponseToChannel(PuzzleRootPrimitive->GetCollisionObjectType()) != ECR_Overlap) &&
		!bLoggedTriggerConfigurationWarning)
	{
		UE_LOG(LogVRExpPuzzleInsertion, Warning,
			TEXT("%s: trigger '%s' should use QueryOnly, GenerateOverlapEvents, and Overlap the puzzle collision channel."),
			*GetNameSafe(GetOwner()), *GetNameSafe(ResolvedTriggerComponent));
		bLoggedTriggerConfigurationWarning = true;
	}

	return true;
}

bool UVRExpPuzzleInsertionComponent::QueryPuzzleHeld(TArray<FBPGripPair>* OutHoldingControllers) const
{
	if (!IsValid(PuzzleActor))
	{
		if (OutHoldingControllers)
		{
			OutHoldingControllers->Reset();
		}
		return false;
	}

	TArray<FBPGripPair> HoldingControllers;
	bool bIsHeld = false;
	IVRGripInterface::Execute_IsHeld(PuzzleActor, HoldingControllers, bIsHeld);

	if (OutHoldingControllers)
	{
		*OutHoldingControllers = MoveTemp(HoldingControllers);
	}

	return bIsHeld;
}

bool UVRExpPuzzleInsertionComponent::RequestReleaseAllGrips()
{
	TArray<FBPGripPair> HoldingControllers;
	if (!QueryPuzzleHeld(&HoldingControllers) || HoldingControllers.IsEmpty())
	{
		BeginInsertionMotion();
		return true;
	}

	TArray<VRExpPuzzleInsertion::FValidatedGrip> ValidatedGrips;
	ValidatedGrips.Reserve(HoldingControllers.Num());

	for (FBPGripPair& GripPair : HoldingControllers)
	{
		if (!GripPair.IsValid())
		{
			return false;
		}

		UGripMotionControllerComponent* Controller = GripPair.HoldingController.Get();
		if (!IsValid(Controller))
		{
			return false;
		}

		FBPActorGripInformation GripInformation;
		EBPVRResultSwitch Result = EBPVRResultSwitch::OnFailed;
		Controller->GetGripByID(GripInformation, GripPair.GripID, Result);
		if (Result != EBPVRResultSwitch::OnSucceeded || !Controller->BP_HasGripAuthority(GripInformation))
		{
			return false;
		}

		VRExpPuzzleInsertion::FValidatedGrip& ValidatedGrip = ValidatedGrips.AddDefaulted_GetRef();
		ValidatedGrip.Controller = Controller;
		ValidatedGrip.GripInformation = GripInformation;
	}

	bool bAllDropCallsSucceeded = true;
	for (const VRExpPuzzleInsertion::FValidatedGrip& ValidatedGrip : ValidatedGrips)
	{
		UGripMotionControllerComponent* Controller = ValidatedGrip.Controller.Get();
		if (!IsValid(Controller) ||
			!Controller->DropGrip(ValidatedGrip.GripInformation, false, FVector::ZeroVector, FVector::ZeroVector))
		{
			bAllDropCallsSucceeded = false;
		}
	}

	if (CurrentState == EVRExpPuzzleInsertionState::WaitingForRelease && !QueryPuzzleHeld())
	{
		BeginInsertionMotion();
	}

	return bAllDropCallsSucceeded;
}

void UVRExpPuzzleInsertionComponent::BeginInsertionMotion()
{
	if (CurrentState != EVRExpPuzzleInsertionState::WaitingForRelease || QueryPuzzleHeld() ||
		!IsValid(PuzzleActor) || !IsValid(PuzzleMotionComponent) || !IsValid(GetOwner()))
	{
		return;
	}

	// OnGripReleased 在原运动组件处理 ReleaseMotionMode 后广播，这里停止它即可避免 Transform 争夺。
	PuzzleMotionComponent->StopMotion();
	StopPuzzlePhysics();

	PhaseStartLocation = PuzzleActor->GetActorLocation();
	PhaseStartRotation = PuzzleActor->GetActorQuat().GetNormalized();

	const FQuat GuideRotation = GetOwner()->GetActorQuat().GetNormalized();
	const FVector LocalInsertionAxis = VRExpPuzzleInsertion::GetLocalAxisVector(InsertionSettings.InsertionAxis);
	FinalInsertionLocation = GetOwner()->GetActorLocation();
	FinalInsertionRotation = GuideRotation;
	InsertionAxisWorld = GuideRotation.RotateVector(LocalInsertionAxis).GetSafeNormal();
	ApproachLocation = FinalInsertionLocation + InsertionAxisWorld * FMath::Max(0.0f, InsertionSettings.ApproachDistance);

	// 第一段只把拼图配置的本地轴对齐到指引的同一本地轴，不主动改变绕该轴的 Twist。
	const FVector CurrentInsertionAxis = PhaseStartRotation.RotateVector(LocalInsertionAxis).GetSafeNormal();
	const float AxisAlignmentDot = FMath::Clamp(
		FVector::DotProduct(CurrentInsertionAxis, InsertionAxisWorld),
		-1.0f,
		1.0f);

	FQuat SwingRotation = FQuat::Identity;
	if (AxisAlignmentDot <= -1.0f + KINDA_SMALL_NUMBER)
	{
		FVector LocalPerpendicularAxis;
		FVector UnusedLocalAxis;
		LocalInsertionAxis.FindBestAxisVectors(LocalPerpendicularAxis, UnusedLocalAxis);
		const FVector StableRotationAxis = PhaseStartRotation.RotateVector(LocalPerpendicularAxis).GetSafeNormal();
		SwingRotation = FQuat(StableRotationAxis, PI);
	}
	else if (AxisAlignmentDot < 1.0f - KINDA_SMALL_NUMBER)
	{
		SwingRotation = FQuat::FindBetweenNormals(CurrentInsertionAxis, InsertionAxisWorld);
	}

	ApproachRotation = (SwingRotation * PhaseStartRotation).GetNormalized();
	PhaseElapsed = 0.0f;
	SetInsertionState(EVRExpPuzzleInsertionState::Approaching);
	if (CurrentState != EVRExpPuzzleInsertionState::Approaching)
	{
		return;
	}

	OnInsertionStarted.Broadcast();

	// 立即处理零时长阶段；正常阶段在本帧保持动画起点。
	if (CurrentState == EVRExpPuzzleInsertionState::Approaching)
	{
		AdvanceInsertion(0.0f);
	}
}

void UVRExpPuzzleInsertionComponent::AdvanceInsertion(float DeltaTime)
{
	float RemainingTime = FMath::Max(0.0f, DeltaTime);

	// 最多跨越 Approaching、AligningUp、Inserting 三个阶段以及完成节点。
	for (int32 Iteration = 0; Iteration < 4 && VRExpPuzzleInsertion::IsAnimatedState(CurrentState); ++Iteration)
	{
		const float Duration = FMath::Max(0.0f, GetCurrentPhaseDuration());
		if (Duration <= KINDA_SMALL_NUMBER)
		{
			ApplyCurrentPhase(1.0f);
			AdvanceToNextPhase();
			continue;
		}

		if (RemainingTime <= 0.0f)
		{
			ApplyCurrentPhase(EvaluateInterpolationAlpha(PhaseElapsed / Duration));
			break;
		}

		const float TimeUntilPhaseEnd = FMath::Max(0.0f, Duration - PhaseElapsed);
		const float ConsumedTime = FMath::Min(RemainingTime, TimeUntilPhaseEnd);
		PhaseElapsed += ConsumedTime;
		RemainingTime -= ConsumedTime;

		const float LinearAlpha = FMath::Clamp(PhaseElapsed / Duration, 0.0f, 1.0f);
		ApplyCurrentPhase(EvaluateInterpolationAlpha(LinearAlpha));

		if (PhaseElapsed >= Duration - KINDA_SMALL_NUMBER)
		{
			ApplyCurrentPhase(1.0f);
			AdvanceToNextPhase();
			continue;
		}

		break;
	}
}

void UVRExpPuzzleInsertionComponent::ApplyCurrentPhase(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

	switch (CurrentState)
	{
	case EVRExpPuzzleInsertionState::Approaching:
		SetPuzzleLocationAndRotation(
			FMath::Lerp(PhaseStartLocation, ApproachLocation, ClampedAlpha),
			FQuat::Slerp(PhaseStartRotation, ApproachRotation, ClampedAlpha).GetNormalized());
		break;

	case EVRExpPuzzleInsertionState::AligningUp:
		SetPuzzleLocationAndRotation(
			ApproachLocation,
			FQuat::Slerp(ApproachRotation, FinalInsertionRotation, ClampedAlpha).GetNormalized());
		break;

	case EVRExpPuzzleInsertionState::Inserting:
		SetPuzzleLocationAndRotation(
			FMath::Lerp(ApproachLocation, FinalInsertionLocation, ClampedAlpha),
			FinalInsertionRotation);
		break;

	default:
		break;
	}
}

void UVRExpPuzzleInsertionComponent::AdvanceToNextPhase()
{
	PhaseElapsed = 0.0f;

	switch (CurrentState)
	{
	case EVRExpPuzzleInsertionState::Approaching:
		SetInsertionState(EVRExpPuzzleInsertionState::AligningUp);
		break;

	case EVRExpPuzzleInsertionState::AligningUp:
		SetInsertionState(EVRExpPuzzleInsertionState::Inserting);
		break;

	case EVRExpPuzzleInsertionState::Inserting:
		CompleteInsertion();
		break;

	default:
		break;
	}
}

void UVRExpPuzzleInsertionComponent::CompleteInsertion()
{
	if (!IsValid(PuzzleActor) || !IsValid(PuzzleMotionComponent))
	{
		FailInsertion(TEXT("PuzzleActor or its motion component became invalid before insertion completed."));
		return;
	}

	SetPuzzleLocationAndRotation(FinalInsertionLocation, FinalInsertionRotation);
	PuzzleMotionComponent->StopMotion();
	StopPuzzlePhysics();
	PuzzleActor->SetDenyGripping(true);

	if (IsValid(ResolvedTriggerComponent))
	{
		ResolvedTriggerComponent->SetGenerateOverlapEvents(false);
	}

	if (bAttachToGuideOnComplete && IsValid(GetOwner()))
	{
		PuzzleActor->AttachToActor(GetOwner(), FAttachmentTransformRules::KeepWorldTransform);
	}

	SetComponentTickEnabled(false);
	SetInsertionState(EVRExpPuzzleInsertionState::Completed);
	if (CurrentState == EVRExpPuzzleInsertionState::Completed)
	{
		OnInsertionCompleted.Broadcast();
	}
}

void UVRExpPuzzleInsertionComponent::FailInsertion(const FString& Reason)
{
	UE_LOG(LogVRExpPuzzleInsertion, Warning,
		TEXT("%s: puzzle insertion failed: %s"), *GetNameSafe(GetOwner()), *Reason);

	RestoreAfterFailedRelease();
	OnInsertionFailed.Broadcast();
}

void UVRExpPuzzleInsertionComponent::RestoreAfterFailedRelease()
{
	SetComponentTickEnabled(false);
	ReleaseWaitElapsed = 0.0f;
	PhaseElapsed = 0.0f;

	if (IsValid(PuzzleActor))
	{
		PuzzleActor->SetDenyGripping(bDenyGrippingBeforeAttempt);
	}

	if (IsValid(ResolvedTriggerComponent))
	{
		ResolvedTriggerComponent->SetGenerateOverlapEvents(bTriggerOverlapBeforeAttempt);
	}

	SetInsertionState(EVRExpPuzzleInsertionState::Idle);

	if (IsValid(PuzzleMotionComponent) && !QueryPuzzleHeld())
	{
		PuzzleMotionComponent->StartMotion();
	}
}

void UVRExpPuzzleInsertionComponent::SetInsertionState(EVRExpPuzzleInsertionState NewState)
{
	if (CurrentState == NewState)
	{
		return;
	}

	CurrentState = NewState;
	OnInsertionStateChanged.Broadcast(NewState);
}

float UVRExpPuzzleInsertionComponent::GetCurrentPhaseDuration() const
{
	switch (CurrentState)
	{
	case EVRExpPuzzleInsertionState::Approaching:
		return InsertionSettings.ApproachDuration;
	case EVRExpPuzzleInsertionState::AligningUp:
		return InsertionSettings.UpAlignmentDuration;
	case EVRExpPuzzleInsertionState::Inserting:
		return InsertionSettings.InsertionDuration;
	default:
		return 0.0f;
	}
}

float UVRExpPuzzleInsertionComponent::EvaluateInterpolationAlpha(float LinearAlpha) const
{
	const float ClampedLinearAlpha = FMath::Clamp(LinearAlpha, 0.0f, 1.0f);
	if (IsValid(InsertionSettings.InterpolationCurve))
	{
		return FMath::Clamp(InsertionSettings.InterpolationCurve->GetFloatValue(ClampedLinearAlpha), 0.0f, 1.0f);
	}

	return FMath::InterpEaseInOut(
		0.0f,
		1.0f,
		ClampedLinearAlpha,
		FMath::Max(0.01f, InsertionSettings.EaseExponent));
}

void UVRExpPuzzleInsertionComponent::SetPuzzleLocationAndRotation(const FVector& Location, const FQuat& Rotation)
{
	if (IsValid(PuzzleActor))
	{
		PuzzleActor->SetActorLocationAndRotation(
			Location,
			Rotation.GetNormalized(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
}

void UVRExpPuzzleInsertionComponent::StopPuzzlePhysics()
{
	if (!IsValid(PuzzleRootPrimitive))
	{
		return;
	}

	if (PuzzleRootPrimitive->IsSimulatingPhysics())
	{
		PuzzleRootPrimitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
		PuzzleRootPrimitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		PuzzleRootPrimitive->SetSimulatePhysics(false);
	}
}

void UVRExpPuzzleInsertionComponent::RestoreInitialAttachmentAndTransform()
{
	if (!IsValid(PuzzleActor) || !IsValid(PuzzleActor->GetRootComponent()))
	{
		return;
	}

	USceneComponent* PuzzleRoot = PuzzleActor->GetRootComponent();
	PuzzleActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (InitialAttachParent.IsValid() &&
		PuzzleRoot->AttachToComponent(InitialAttachParent.Get(), FAttachmentTransformRules::KeepRelativeTransform, InitialAttachSocketName))
	{
		PuzzleRoot->SetRelativeTransform(
			InitialPuzzleRelativeTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		return;
	}

	PuzzleActor->SetActorTransform(
		InitialPuzzleWorldTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}
