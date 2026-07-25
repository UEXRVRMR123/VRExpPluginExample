// Fill out your copyright notice in the Description page of Project Settings.

#include "VRExpHandTrackingComponent.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAsset.h"
#include "UObject/UnrealType.h"
#include "VRExpHandTrackingFunctionLibrary.h"
#include "VRExpHandTrackingSubsystem.h"

UVRExpHandTrackingComponent::UVRExpHandTrackingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SkeletonMeshType(EVRExpHandType::HandLeft)
	, ApplyLocationToEveryBone(false)
	, AutoHide(false)
	, AutoScaleComponent(false)
	, bEnableGestureRecognition(true)
	, bEnablePalmFacingRecognition(true)
	, PinchClosedDistanceScale(0.25f)
	, PinchOpenDistanceScale(0.85f)
	, FingerClosedAngleDegrees(95.0f)
	, FingerOpenAngleDegrees(25.0f)
	, GestureActiveThreshold(0.75f)
	, GestureInactiveThreshold(0.35f)
	, GestureAxisBroadcastDelta(0.02f)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	for (int32 KeypointIndex = 0; KeypointIndex < EHandKeypointCount; ++KeypointIndex)
	{
		const EHandKeypoint HandKeypoint = static_cast<EHandKeypoint>(KeypointIndex);
		BoneMappings.FindOrAdd(HandKeypoint);
	}
}

void UVRExpHandTrackingComponent::OnRegister()
{
	Super::OnRegister();

	ApplyEffectiveMeshScale(1.0f);
}

#if WITH_EDITOR
void UVRExpHandTrackingComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplyEffectiveMeshScale(1.0f);
}
#endif

float UVRExpHandTrackingComponent::GetGestureAxis(EVRExpHandGesture Gesture) const
{
	if (!bEnableGestureRecognition)
	{
		return 0.0f;
	}

	if (const UVRExpHandTrackingSubsystem* Subsystem = GetHandTrackingSubsystem())
	{
		return Subsystem->GetGestureAxis(SkeletonMeshType, Gesture);
	}

	return 0.0f;
}

FVRExpHandGestureAxisValues UVRExpHandTrackingComponent::GetGestureAxisValues() const
{
	if (!bEnableGestureRecognition)
	{
		return FVRExpHandGestureAxisValues();
	}

	if (const UVRExpHandTrackingSubsystem* Subsystem = GetHandTrackingSubsystem())
	{
		return Subsystem->GetGestureAxisValues(SkeletonMeshType);
	}

	return FVRExpHandGestureAxisValues();
}

float UVRExpHandTrackingComponent::GetPalmFacingAxis(EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction) const
{
	if (!bEnablePalmFacingRecognition)
	{
		return 0.0f;
	}

	if (const UVRExpHandTrackingSubsystem* Subsystem = GetHandTrackingSubsystem())
	{
		return Subsystem->GetPalmFacingAxis(SkeletonMeshType, Space, Direction);
	}

	return 0.0f;
}

FVRExpPalmFacingAxisValues UVRExpHandTrackingComponent::GetPalmFacingAxisValues(EVRExpHandTrackingSpace Space) const
{
	if (!bEnablePalmFacingRecognition)
	{
		return FVRExpPalmFacingAxisValues();
	}

	if (const UVRExpHandTrackingSubsystem* Subsystem = GetHandTrackingSubsystem())
	{
		return Subsystem->GetPalmFacingAxisValues(SkeletonMeshType, Space);
	}

	return FVRExpPalmFacingAxisValues();
}

bool UVRExpHandTrackingComponent::IsPalmFacingActive(EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction) const
{
	if (!bEnablePalmFacingRecognition)
	{
		return false;
	}

	if (const UVRExpHandTrackingSubsystem* Subsystem = GetHandTrackingSubsystem())
	{
		return Subsystem->IsPalmFacingActive(SkeletonMeshType, Space, Direction);
	}

	return false;
}

void UVRExpHandTrackingComponent::BeginPlay()
{
	Super::BeginPlay();
	BindHandTrackingSubsystemDelegates();

	if (AutoHide)
	{
		SetHiddenInGame(true, true);
	}
}

void UVRExpHandTrackingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindHandTrackingSubsystemDelegates();
	Super::EndPlay(EndPlayReason);
}

void UVRExpHandTrackingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool bHidden = true;

	FVRExpTrackedHandState HandState;
	const UVRExpHandTrackingSubsystem* Subsystem = GetHandTrackingSubsystem();
	const bool bHasValidHandData = Subsystem != nullptr && Subsystem->GetTrackedHandState(SkeletonMeshType, HandState);
	if (GetSkinnedAsset())
	{
		ApplyEffectiveMeshScale(1.0f);

		if (bHasValidHandData)
		{
			bHidden = !ApplyTrackedHandPose(HandState.HandKeyPositions, HandState.HandKeyRotations);
		}
	}

	if (AutoHide && bHidden != bHiddenInGame)
	{
		SetHiddenInGame(bHidden, true);
	}
}

void UVRExpHandTrackingComponent::HandleSubsystemGestureStarted(EVRExpHandType HandType, EVRExpHandGesture Gesture, float AxisValue)
{
	if (HandType == SkeletonMeshType && bEnableGestureRecognition)
	{
		OnGestureStarted.Broadcast(Gesture, AxisValue);
	}
}

void UVRExpHandTrackingComponent::HandleSubsystemGestureEnded(EVRExpHandType HandType, EVRExpHandGesture Gesture, float AxisValue)
{
	if (HandType == SkeletonMeshType && bEnableGestureRecognition)
	{
		OnGestureEnded.Broadcast(Gesture, AxisValue);
	}
}

void UVRExpHandTrackingComponent::HandleSubsystemGestureAxisChanged(EVRExpHandType HandType, EVRExpHandGesture Gesture, float AxisValue)
{
	if (HandType == SkeletonMeshType && bEnableGestureRecognition)
	{
		OnGestureAxisChanged.Broadcast(Gesture, AxisValue);
	}
}

void UVRExpHandTrackingComponent::HandleSubsystemPalmFacingStarted(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction, float AxisValue)
{
	if (HandType == SkeletonMeshType && bEnablePalmFacingRecognition)
	{
		OnPalmFacingStarted.Broadcast(Space, Direction, AxisValue);
	}
}

void UVRExpHandTrackingComponent::HandleSubsystemPalmFacingEnded(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction, float AxisValue)
{
	if (HandType == SkeletonMeshType && bEnablePalmFacingRecognition)
	{
		OnPalmFacingEnded.Broadcast(Space, Direction, AxisValue);
	}
}

void UVRExpHandTrackingComponent::HandleSubsystemPalmFacingAxisChanged(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction, float AxisValue)
{
	if (HandType == SkeletonMeshType && bEnablePalmFacingRecognition)
	{
		OnPalmFacingAxisChanged.Broadcast(Space, Direction, AxisValue);
	}
}

UVRExpHandTrackingSubsystem* UVRExpHandTrackingComponent::GetHandTrackingSubsystem() const
{
	return UVRExpHandTrackingFunctionLibrary::GetHandTrackingSubsystem(this);
}

void UVRExpHandTrackingComponent::BindHandTrackingSubsystemDelegates()
{
	if (UVRExpHandTrackingSubsystem* Subsystem = GetHandTrackingSubsystem())
	{
		Subsystem->OnGestureStarted.AddUniqueDynamic(this, &UVRExpHandTrackingComponent::HandleSubsystemGestureStarted);
		Subsystem->OnGestureEnded.AddUniqueDynamic(this, &UVRExpHandTrackingComponent::HandleSubsystemGestureEnded);
		Subsystem->OnGestureAxisChanged.AddUniqueDynamic(this, &UVRExpHandTrackingComponent::HandleSubsystemGestureAxisChanged);
		Subsystem->OnPalmFacingStarted.AddUniqueDynamic(this, &UVRExpHandTrackingComponent::HandleSubsystemPalmFacingStarted);
		Subsystem->OnPalmFacingEnded.AddUniqueDynamic(this, &UVRExpHandTrackingComponent::HandleSubsystemPalmFacingEnded);
		Subsystem->OnPalmFacingAxisChanged.AddUniqueDynamic(this, &UVRExpHandTrackingComponent::HandleSubsystemPalmFacingAxisChanged);
	}
}

void UVRExpHandTrackingComponent::UnbindHandTrackingSubsystemDelegates()
{
	if (UVRExpHandTrackingSubsystem* Subsystem = GetHandTrackingSubsystem())
	{
		Subsystem->OnGestureStarted.RemoveAll(this);
		Subsystem->OnGestureEnded.RemoveAll(this);
		Subsystem->OnGestureAxisChanged.RemoveAll(this);
		Subsystem->OnPalmFacingStarted.RemoveAll(this);
		Subsystem->OnPalmFacingEnded.RemoveAll(this);
		Subsystem->OnPalmFacingAxisChanged.RemoveAll(this);
	}
}

bool UVRExpHandTrackingComponent::ApplyTrackedHandPose(const TArray<FVector>& WorldPositions, const TArray<FQuat>& WorldRotations)
{
	if (!GetSkinnedAsset())
	{
		return false;
	}

	const int32 WristKeypointIndex = GetHandKeypointIndex(EHandKeypoint::Wrist);
	if (!WorldPositions.IsValidIndex(WristKeypointIndex) || !WorldRotations.IsValidIndex(WristKeypointIndex))
	{
		return false;
	}

	const FQuat WristWorldRotation = WorldRotations[WristKeypointIndex].GetNormalized();
	SetWorldLocation(WorldPositions[WristKeypointIndex]);
	SetWorldRotation(ApplyComponentRotationAdjustment(WristWorldRotation, ComponentRotationAdjustment));

	TArray<FResolvedBoneMapping> ResolvedMappings;
	BuildResolvedBoneMappings(FMath::Min(WorldPositions.Num(), WorldRotations.Num()), ResolvedMappings);
	if (ResolvedMappings.Num() == 0)
	{
		return true;
	}

	const FReferenceSkeleton& RefSkeleton = GetSkinnedAsset()->GetRefSkeleton();
	const int32 NumBones = RefSkeleton.GetNum();
	const bool bMirrorBonePose = ShouldMirrorForCurrentHand() && MirrorSettings.bMirrorBonePose;
	const FTransform PoseComponentWorldTransform = BuildPoseComponentTransform();
	const EAxis::Type PoseMirrorAxis = ToEAxis(MirrorSettings.MirrorAxis);
	const EAxis::Type PoseMirrorFlipAxis = ToEAxis(MirrorSettings.PoseMirrorFlipAxis);
	const TArray<FTransform>& CurrentComponentTransforms = GetComponentSpaceTransforms();

	TArray<FTransform> RawComponentTransforms;
	RawComponentTransforms.SetNum(NumBones);
	TArray<uint8> bHasRawComponentTransform;
	bHasRawComponentTransform.Init(false, NumBones);

	for (const FResolvedBoneMapping& ResolvedMapping : ResolvedMappings)
	{
		const int32 KeypointIndex = GetHandKeypointIndex(ResolvedMapping.HandKeypoint);
		const FTransform WorldTransform(WorldRotations[KeypointIndex].GetNormalized(), WorldPositions[KeypointIndex], FVector::OneVector);
		FTransform ComponentTransform = WorldTransform.GetRelativeTransform(PoseComponentWorldTransform);
		ComponentTransform.NormalizeRotation();

		if (bMirrorBonePose)
		{
			ComponentTransform.Mirror(PoseMirrorAxis, PoseMirrorFlipAxis);
			ComponentTransform.NormalizeRotation();
		}

		RawComponentTransforms[ResolvedMapping.BoneIndex] = ComponentTransform;
		bHasRawComponentTransform[ResolvedMapping.BoneIndex] = true;
	}

	TArray<FTransform> HierarchyComponentTransforms;
	HierarchyComponentTransforms.SetNum(NumBones);
	TArray<uint8> bHasHierarchyComponentTransform;
	bHasHierarchyComponentTransform.Init(false, NumBones);

	auto GetParentComponentTransform = [&CurrentComponentTransforms](int32 ParentIndex, const TArray<FTransform>& CandidateTransforms, const TArray<uint8>& bHasCandidateTransform)
	{
		if (ParentIndex != INDEX_NONE)
		{
			if (CandidateTransforms.IsValidIndex(ParentIndex) && bHasCandidateTransform.IsValidIndex(ParentIndex) && bHasCandidateTransform[ParentIndex])
			{
				return CandidateTransforms[ParentIndex];
			}

			if (CurrentComponentTransforms.IsValidIndex(ParentIndex))
			{
				return CurrentComponentTransforms[ParentIndex];
			}
		}

		return FTransform::Identity;
	};

	for (const FResolvedBoneMapping& ResolvedMapping : ResolvedMappings)
	{
		const FTransform& RawComponentTransform = RawComponentTransforms[ResolvedMapping.BoneIndex];
		const FTransform RawParentComponentTransform = GetParentComponentTransform(ResolvedMapping.ParentIndex, RawComponentTransforms, bHasRawComponentTransform);
		FTransform ParentBoneSpaceTransform = RawComponentTransform.GetRelativeTransform(RawParentComponentTransform);
		ParentBoneSpaceTransform.NormalizeRotation();
		ParentBoneSpaceTransform.SetRotation(ApplyParentBoneRotationOffset(ParentBoneSpaceTransform.GetRotation(), ResolvedMapping.RotationAdjustment));

		const FTransform AdjustedParentComponentTransform = GetParentComponentTransform(ResolvedMapping.ParentIndex, HierarchyComponentTransforms, bHasHierarchyComponentTransform);
		FTransform HierarchyComponentTransform = ParentBoneSpaceTransform * AdjustedParentComponentTransform;
		HierarchyComponentTransform.NormalizeRotation();

		HierarchyComponentTransforms[ResolvedMapping.BoneIndex] = HierarchyComponentTransform;
		bHasHierarchyComponentTransform[ResolvedMapping.BoneIndex] = true;

		FTransform OutputComponentTransform = HierarchyComponentTransform;
		OutputComponentTransform.SetRotation(ApplyAxisAdjustment(OutputComponentTransform.GetRotation(), ResolvedMapping.RotationAdjustment));

		if (bMirrorBonePose && ResolvedMapping.HandKeypoint == EHandKeypoint::Wrist && !MirrorSettings.bApplyWristBoneTransformWhenMirrored)
		{
			continue;
		}

		SetBoneRotationByName(ResolvedMapping.BoneName, OutputComponentTransform.GetRotation().Rotator(), EBoneSpaces::ComponentSpace);
		if (ResolvedMapping.HandKeypoint == EHandKeypoint::Wrist || ApplyLocationToEveryBone)
		{
			SetBoneLocationByName(ResolvedMapping.BoneName, OutputComponentTransform.GetLocation(), EBoneSpaces::ComponentSpace);
		}
	}

	return true;
}

void UVRExpHandTrackingComponent::BuildResolvedBoneMappings(int32 NumKeypoints, TArray<FResolvedBoneMapping>& OutMappings) const
{
	OutMappings.Reset();

	if (!GetSkinnedAsset())
	{
		return;
	}

	const FReferenceSkeleton& RefSkeleton = GetSkinnedAsset()->GetRefSkeleton();
	for (const TPair<EHandKeypoint, FVRExpHandBoneMapping>& MappingPair : BoneMappings)
	{
		const FVRExpHandBoneMapping& BoneMapping = MappingPair.Value;
		if (BoneMapping.BoneName.IsNone())
		{
			continue;
		}

		const int32 KeypointIndex = GetHandKeypointIndex(MappingPair.Key);
		if (KeypointIndex < 0 || KeypointIndex >= NumKeypoints)
		{
			continue;
		}

		const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneMapping.BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			continue;
		}

		FResolvedBoneMapping ResolvedMapping;
		ResolvedMapping.HandKeypoint = MappingPair.Key;
		ResolvedMapping.BoneName = BoneMapping.BoneName;
		ResolvedMapping.BoneIndex = BoneIndex;
		ResolvedMapping.ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
		ResolvedMapping.RotationAdjustment = BoneMapping.RotationAdjustment;
		OutMappings.Add(ResolvedMapping);
	}

	OutMappings.Sort([](const FResolvedBoneMapping& A, const FResolvedBoneMapping& B)
	{
		return A.BoneIndex < B.BoneIndex;
	});
}

bool UVRExpHandTrackingComponent::ShouldMirrorForCurrentHand() const
{
	if (!MirrorSettings.bEnableMirror)
	{
		return false;
	}

	return MirrorSettings.bAutoMirrorByHandType
		? MirrorSettings.SourceMeshHandType != SkeletonMeshType
		: true;
}

FTransform UVRExpHandTrackingComponent::BuildPoseComponentTransform() const
{
	FTransform PoseComponentTransform = GetComponentTransform();
	const FVector ComponentScale = PoseComponentTransform.GetScale3D();
	PoseComponentTransform.SetScale3D(FVector(FMath::Abs(ComponentScale.X), FMath::Abs(ComponentScale.Y), FMath::Abs(ComponentScale.Z)));
	PoseComponentTransform.NormalizeRotation();
	return PoseComponentTransform;
}

void UVRExpHandTrackingComponent::ApplyEffectiveMeshScale(float TrackingScale)
{
	FVector EffectiveScale = MirrorSettings.UnmirroredMeshScale * TrackingScale;

	if (ShouldMirrorForCurrentHand())
	{
		const FVector MirrorSign = GetMirrorAxisSign(MirrorSettings.MirrorAxis);
		EffectiveScale.X *= MirrorSign.X;
		EffectiveScale.Y *= MirrorSign.Y;
		EffectiveScale.Z *= MirrorSign.Z;
	}

	SetRelativeScale3D(EffectiveScale);
}

FQuat UVRExpHandTrackingComponent::ApplyComponentRotationAdjustment(const FQuat& RawRotation, const FVRExpHandRotationAdjustment& RotationAdjustment) const
{
	FQuat ResultRotation = RawRotation;

	if (RotationAdjustment.bEnableRotationOffset)
	{
		ResultRotation = (ResultRotation * RotationAdjustment.RotationOffset.Quaternion()).GetNormalized();
	}

	if (RotationAdjustment.bEnableAxisAdjustment)
	{
		FQuat AxisCorrection = FQuat::Identity;
		if (TryBuildAxisCorrection(RotationAdjustment.AxisSettings, AxisCorrection))
		{
			ResultRotation = (ResultRotation * AxisCorrection).GetNormalized();
		}
	}

	return ResultRotation;
}

FQuat UVRExpHandTrackingComponent::ApplyParentBoneRotationOffset(const FQuat& ParentBoneSpaceRotation, const FVRExpHandRotationAdjustment& RotationAdjustment) const
{
	if (!RotationAdjustment.bEnableRotationOffset)
	{
		return ParentBoneSpaceRotation;
	}

	return (RotationAdjustment.RotationOffset.Quaternion() * ParentBoneSpaceRotation).GetNormalized();
}

FQuat UVRExpHandTrackingComponent::ApplyAxisAdjustment(const FQuat& ComponentSpaceRotation, const FVRExpHandRotationAdjustment& RotationAdjustment) const
{
	if (!RotationAdjustment.bEnableAxisAdjustment)
	{
		return ComponentSpaceRotation;
	}

	FQuat AxisCorrection = FQuat::Identity;
	if (!TryBuildAxisCorrection(RotationAdjustment.AxisSettings, AxisCorrection))
	{
		return ComponentSpaceRotation;
	}

	return (ComponentSpaceRotation * AxisCorrection).GetNormalized();
}

EAxis::Type UVRExpHandTrackingComponent::ToEAxis(EVRExpHandMirrorAxis Axis)
{
	switch (Axis)
	{
	case EVRExpHandMirrorAxis::X:
		return EAxis::X;
	case EVRExpHandMirrorAxis::Y:
		return EAxis::Y;
	case EVRExpHandMirrorAxis::Z:
		return EAxis::Z;
	default:
		return EAxis::Y;
	}
}

FVector UVRExpHandTrackingComponent::GetAxisVector(EVRExpHandBoneAxis Axis)
{
	switch (Axis)
	{
	case EVRExpHandBoneAxis::X:
		return FVector::ForwardVector;
	case EVRExpHandBoneAxis::Y:
		return FVector::RightVector;
	case EVRExpHandBoneAxis::Z:
		return FVector::UpVector;
	case EVRExpHandBoneAxis::NegativeX:
		return -FVector::ForwardVector;
	case EVRExpHandBoneAxis::NegativeY:
		return -FVector::RightVector;
	case EVRExpHandBoneAxis::NegativeZ:
		return -FVector::UpVector;
	default:
		return FVector::ForwardVector;
	}
}

FVector UVRExpHandTrackingComponent::GetMirrorAxisSign(EVRExpHandMirrorAxis MirrorAxis)
{
	switch (MirrorAxis)
	{
	case EVRExpHandMirrorAxis::X:
		return FVector(-1.0f, 1.0f, 1.0f);
	case EVRExpHandMirrorAxis::Y:
		return FVector(1.0f, -1.0f, 1.0f);
	case EVRExpHandMirrorAxis::Z:
		return FVector(1.0f, 1.0f, -1.0f);
	default:
		return FVector(1.0f, -1.0f, 1.0f);
	}
}

int32 UVRExpHandTrackingComponent::GetHandKeypointIndex(EHandKeypoint HandKeypoint)
{
	return static_cast<int32>(static_cast<uint8>(HandKeypoint));
}

bool UVRExpHandTrackingComponent::TryBuildAxisCorrection(const FVRExpHandBoneAxisSettings& AxisSettings, FQuat& OutAxisCorrection)
{
	const FVector ForwardVector = GetAxisVector(AxisSettings.ForwardAxis);
	const FVector UpVector = GetAxisVector(AxisSettings.UpAxis);
	if (FMath::Abs(FVector::DotProduct(ForwardVector, UpVector)) > 1.0f - KINDA_SMALL_NUMBER)
	{
		OutAxisCorrection = FQuat::Identity;
		return false;
	}

	const FQuat BoneBasisRotation = FRotationMatrix::MakeFromXZ(ForwardVector, UpVector).ToQuat();
	OutAxisCorrection = BoneBasisRotation.Inverse().GetNormalized();
	return true;
}

