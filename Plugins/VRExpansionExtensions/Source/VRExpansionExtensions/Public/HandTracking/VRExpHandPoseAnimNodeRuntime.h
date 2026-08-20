// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/BoneReference.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "HandTracking/VRExpHandPoseTypes.h"

/** Non-reflected bone-cache and evaluation state used by a hand-pose animation node. */
class VREXPANSIONEXTENSIONS_API FVRExpHandPoseAnimNodeRuntime
{
public:
	void InitializeBoneReferences(
		const FBoneContainer& RequiredBones,
		const TMap<EHandKeypoint, FVRExpHandBoneMapping>& BoneMappings);

	bool IsValidToEvaluate(
		const FBoneContainer& RequiredBones,
		const TMap<EHandKeypoint, FVRExpHandBoneMapping>& BoneMappings);

	void EvaluateSkeletalControl(
		FComponentSpacePoseContext& Output,
		TArray<FBoneTransform>& OutBoneTransforms,
		TConstArrayView<FTransform> WorldTransforms,
		const FTransform& MeshWorldTransform,
		EVRExpHandType HandType,
		EVRExpHandPoseTransformApplicationMode TransformApplicationMode,
		bool bApplyWristBone,
		const FVRExpHandRotationAdjustment& ComponentRotationAdjustment,
		const FVRExpHandMirrorSettings& MirrorSettings) const;

	static bool ShouldApplyTrackedLocation(
		EVRExpHandPoseTransformApplicationMode TransformApplicationMode,
		bool bIsWrist);

	static uint32 CalculateBoneMappingsSignature(
		const TMap<EHandKeypoint, FVRExpHandBoneMapping>& BoneMappings);

private:
	struct FCachedBoneMapping
	{
		EHandKeypoint HandKeypoint = EHandKeypoint::Wrist;
		FBoneReference BoneReference;
		FCompactPoseBoneIndex ParentIndex = FCompactPoseBoneIndex(INDEX_NONE);
		FVRExpHandRotationAdjustment RotationAdjustment;
	};

	void RebuildCachedBoneMappings(
		const FBoneContainer& RequiredBones,
		const TMap<EHandKeypoint, FVRExpHandBoneMapping>& BoneMappings);

	TArray<FCachedBoneMapping> CachedBoneMappings;
	uint32 CachedBoneMappingsSignature = 0;
	bool bBoneMappingsCacheInitialized = false;
};
