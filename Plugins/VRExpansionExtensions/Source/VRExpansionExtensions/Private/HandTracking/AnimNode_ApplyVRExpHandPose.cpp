// Fill out your copyright notice in the Description page of Project Settings.

#include "HandTracking/AnimNode_ApplyVRExpHandPose.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "HandTracking/VRExpHandPoseRuntimeUtils.h"
#include "HandTracking/VRExpHandTrackingSubsystem.h"

namespace
{
bool CopyTrackedHandSnapshot(
	const UVRExpHandTrackingSubsystem& Subsystem,
	EVRExpHandType HandType,
	TArray<FTransform>& OutTransforms)
{
	OutTransforms.Reset();

	FVRExpTrackedHandState HandState;
	if (!Subsystem.GetTrackedHandState(HandType, HandState))
	{
		return false;
	}

	const int32 WristIndex = FVRExpHandPoseRuntimeUtils::GetHandKeypointIndex(EHandKeypoint::Wrist);
	if (!HandState.HandKeyTransforms.IsValidIndex(WristIndex)
		|| HandState.HandKeyTransforms[WristIndex].ContainsNaN())
	{
		return false;
	}

	OutTransforms = MoveTemp(HandState.HandKeyTransforms);
	return true;
}

FVRExpHandMirrorSettings MakeRuntimeMirrorSettings(const FVRExpHandPoseMirrorSettings& PoseMirrorSettings)
{
	FVRExpHandMirrorSettings Result;
	Result.bEnableMirror = PoseMirrorSettings.bEnableMirror;
	Result.bAutoMirrorByHandType = PoseMirrorSettings.bAutoMirrorByHandType;
	Result.SourceMeshHandType = PoseMirrorSettings.SourceMeshHandType;
	Result.MirrorAxis = PoseMirrorSettings.MirrorAxis;
	Result.bMirrorBonePose = true;
	Result.PoseMirrorFlipAxis = PoseMirrorSettings.PoseMirrorFlipAxis;
	Result.bApplyWristBoneTransformWhenMirrored = PoseMirrorSettings.bApplyWristBoneTransformWhenMirrored;
	return Result;
}
}

FAnimNode_ApplyVRExpHandPose::FAnimNode_ApplyVRExpHandPose()
	: HandType(EVRExpHandType::HandLeft)
	, CachedMeshWorldTransform(FTransform::Identity)
	, bHasValidLeftPoseSnapshot(false)
	, bHasValidRightPoseSnapshot(false)
{
}

void FAnimNode_ApplyVRExpHandPose::PreUpdate(const UAnimInstance* InAnimInstance)
{
	bHasValidLeftPoseSnapshot = false;
	bHasValidRightPoseSnapshot = false;
	CachedLeftWorldTransforms.Reset();
	CachedRightWorldTransforms.Reset();
	CachedMeshWorldTransform = FTransform::Identity;

	if (!IsValid(InAnimInstance))
	{
		return;
	}

	const USkeletalMeshComponent* SkeletalMeshComponent = InAnimInstance->GetSkelMeshComponent();
	const UWorld* World = IsValid(SkeletalMeshComponent) ? SkeletalMeshComponent->GetWorld() : nullptr;
	const UVRExpHandTrackingSubsystem* Subsystem = IsValid(World)
		? World->GetSubsystem<UVRExpHandTrackingSubsystem>()
		: nullptr;
	if (!IsValid(Subsystem))
	{
		return;
	}

	CachedMeshWorldTransform = SkeletalMeshComponent->GetComponentTransform();
	const FVector MeshScale = CachedMeshWorldTransform.GetScale3D();
	CachedMeshWorldTransform.SetScale3D(FVector(
		FMath::Abs(MeshScale.X),
		FMath::Abs(MeshScale.Y),
		FMath::Abs(MeshScale.Z)));
	CachedMeshWorldTransform.NormalizeRotation();
	if (CachedMeshWorldTransform.ContainsNaN())
	{
		CachedMeshWorldTransform = FTransform::Identity;
		return;
	}

	bHasValidLeftPoseSnapshot = CopyTrackedHandSnapshot(
		*Subsystem,
		EVRExpHandType::HandLeft,
		CachedLeftWorldTransforms);
	bHasValidRightPoseSnapshot = CopyTrackedHandSnapshot(
		*Subsystem,
		EVRExpHandType::HandRight,
		CachedRightWorldTransforms);
}

void FAnimNode_ApplyVRExpHandPose::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	RuntimeState.InitializeBoneReferences(RequiredBones, HandConfig.BoneMappings);
}

bool FAnimNode_ApplyVRExpHandPose::IsValidToEvaluate(
	const USkeleton* Skeleton,
	const FBoneContainer& RequiredBones)
{
	(void)Skeleton;
	return RuntimeState.IsValidToEvaluate(RequiredBones, HandConfig.BoneMappings);
}

void FAnimNode_ApplyVRExpHandPose::EvaluateSkeletalControl_AnyThread(
	FComponentSpacePoseContext& Output,
	TArray<FBoneTransform>& OutBoneTransforms)
{
	const bool bUseLeftHand = HandType == EVRExpHandType::HandLeft;
	if ((bUseLeftHand && !bHasValidLeftPoseSnapshot)
		|| (!bUseLeftHand && !bHasValidRightPoseSnapshot))
	{
		return;
	}

	const TArray<FTransform>& WorldTransforms = bUseLeftHand
		? CachedLeftWorldTransforms
		: CachedRightWorldTransforms;

	RuntimeState.EvaluateSkeletalControl(
		Output,
		OutBoneTransforms,
		WorldTransforms,
		CachedMeshWorldTransform,
		HandType,
		HandConfig.TransformApplicationMode,
		HandConfig.bApplyWristBone,
		HandConfig.ComponentRotationAdjustment,
		MakeRuntimeMirrorSettings(HandConfig.PoseMirrorSettings));
}
