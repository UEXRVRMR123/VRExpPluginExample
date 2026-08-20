// Fill out your copyright notice in the Description page of Project Settings.

#include "HandTracking/VRExpHandTrackingComponent.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAsset.h"
#include "UObject/UnrealType.h"
#include "HandTracking/VRExpHandPoseRuntimeUtils.h"
#include "HandTracking/VRExpHandTrackingFunctionLibrary.h"
#include "HandTracking/VRExpHandTrackingSubsystem.h"

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

	const int32 WristKeypointIndex = FVRExpHandPoseRuntimeUtils::GetHandKeypointIndex(EHandKeypoint::Wrist);
	if (!WorldPositions.IsValidIndex(WristKeypointIndex) || !WorldRotations.IsValidIndex(WristKeypointIndex))
	{
		return false;
	}

	const FQuat WristWorldRotation = WorldRotations[WristKeypointIndex].GetNormalized();
	SetWorldLocation(WorldPositions[WristKeypointIndex]);
	SetWorldRotation(FVRExpHandPoseRuntimeUtils::ApplyComponentRotationAdjustment(WristWorldRotation, ComponentRotationAdjustment));

	const int32 NumKeypoints = FMath::Min(WorldPositions.Num(), WorldRotations.Num());
	TArray<FVRExpResolvedHandBoneMapping> ResolvedMappings;
	BuildResolvedBoneMappings(NumKeypoints, ResolvedMappings);
	if (ResolvedMappings.Num() == 0)
	{
		return true;
	}

	const bool bMirrorBonePose = ShouldMirrorForCurrentHand() && MirrorSettings.bMirrorBonePose;
	const FTransform PoseComponentWorldTransform = BuildPoseComponentTransform();
	const TArray<FTransform>& CurrentComponentTransforms = GetComponentSpaceTransforms();

	TArray<FTransform> WorldTransforms;
	WorldTransforms.Reserve(NumKeypoints);
	for (int32 KeypointIndex = 0; KeypointIndex < NumKeypoints; ++KeypointIndex)
	{
		WorldTransforms.Emplace(WorldRotations[KeypointIndex].GetNormalized(), WorldPositions[KeypointIndex], FVector::OneVector);
	}

	TArray<FVRExpSolvedHandBoneTransform> SolvedTransforms;
	FVRExpHandPoseRuntimeUtils::SolveComponentSpacePose(
		WorldTransforms,
		PoseComponentWorldTransform,
		CurrentComponentTransforms,
		ResolvedMappings,
		false,
		bMirrorBonePose,
		MirrorSettings.MirrorAxis,
		MirrorSettings.PoseMirrorFlipAxis,
		SolvedTransforms);

	const FReferenceSkeleton& RefSkeleton = GetSkinnedAsset()->GetRefSkeleton();
	for (const FVRExpSolvedHandBoneTransform& SolvedTransform : SolvedTransforms)
	{
		if (bMirrorBonePose && SolvedTransform.HandKeypoint == EHandKeypoint::Wrist && !MirrorSettings.bApplyWristBoneTransformWhenMirrored)
		{
			continue;
		}

		const FName BoneName = RefSkeleton.GetBoneName(SolvedTransform.BoneIndex);
		SetBoneRotationByName(BoneName, SolvedTransform.ComponentTransform.GetRotation().Rotator(), EBoneSpaces::ComponentSpace);
		if (SolvedTransform.HandKeypoint == EHandKeypoint::Wrist || ApplyLocationToEveryBone)
		{
			SetBoneLocationByName(BoneName, SolvedTransform.ComponentTransform.GetLocation(), EBoneSpaces::ComponentSpace);
		}
	}

	return true;
}

void UVRExpHandTrackingComponent::BuildResolvedBoneMappings(int32 NumKeypoints, TArray<FVRExpResolvedHandBoneMapping>& OutMappings) const
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

		const int32 KeypointIndex = FVRExpHandPoseRuntimeUtils::GetHandKeypointIndex(MappingPair.Key);
		if (KeypointIndex < 0 || KeypointIndex >= NumKeypoints)
		{
			continue;
		}

		const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneMapping.BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			continue;
		}

		FVRExpResolvedHandBoneMapping ResolvedMapping;
		ResolvedMapping.HandKeypoint = MappingPair.Key;
		ResolvedMapping.BoneName = BoneMapping.BoneName;
		ResolvedMapping.BoneIndex = BoneIndex;
		ResolvedMapping.ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
		ResolvedMapping.RotationAdjustment = BoneMapping.RotationAdjustment;
		OutMappings.Add(ResolvedMapping);
	}

	OutMappings.Sort([](const FVRExpResolvedHandBoneMapping& A, const FVRExpResolvedHandBoneMapping& B)
	{
		return A.BoneIndex < B.BoneIndex;
	});
}

bool UVRExpHandTrackingComponent::ShouldMirrorForCurrentHand() const
{
	return FVRExpHandPoseRuntimeUtils::ShouldMirror(
		SkeletonMeshType,
		MirrorSettings.bEnableMirror,
		MirrorSettings.bAutoMirrorByHandType,
		MirrorSettings.SourceMeshHandType);
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
		const FVector MirrorSign = FVRExpHandPoseRuntimeUtils::GetMirrorAxisSign(MirrorSettings.MirrorAxis);
		EffectiveScale.X *= MirrorSign.X;
		EffectiveScale.Y *= MirrorSign.Y;
		EffectiveScale.Z *= MirrorSign.Z;
	}

	SetRelativeScale3D(EffectiveScale);
}

