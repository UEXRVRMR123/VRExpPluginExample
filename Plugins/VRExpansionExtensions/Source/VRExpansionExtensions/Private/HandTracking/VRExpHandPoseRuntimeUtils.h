// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "HandTracking/VRExpHandPoseTypes.h"

class FVRExpHandPoseRuntimeUtils final
{
public:
	static bool SolveComponentSpacePose(
		TConstArrayView<FTransform> WorldTransforms,
		const FTransform& PoseComponentWorldTransform,
		TConstArrayView<FTransform> CurrentComponentTransforms,
		TConstArrayView<FVRExpResolvedHandBoneMapping> ResolvedMappings,
		bool bPreserveInputWristTransform,
		bool bMirrorBonePose,
		EVRExpHandMirrorAxis MirrorAxis,
		EVRExpHandMirrorAxis MirrorFlipAxis,
		TArray<FVRExpSolvedHandBoneTransform>& OutSolvedTransforms);

	static bool ShouldMirror(
		EVRExpHandType HandType,
		bool bEnableMirror,
		bool bAutoMirrorByHandType,
		EVRExpHandType SourceMeshHandType);

	static FQuat ApplyComponentRotationAdjustment(
		const FQuat& RawRotation,
		const FVRExpHandRotationAdjustment& RotationAdjustment);

	static FQuat ApplyParentBoneRotationOffset(
		const FQuat& ParentBoneSpaceRotation,
		const FVRExpHandRotationAdjustment& RotationAdjustment);

	static FQuat ApplyAxisAdjustment(
		const FQuat& ComponentSpaceRotation,
		const FVRExpHandRotationAdjustment& RotationAdjustment);

	static EAxis::Type ToEAxis(EVRExpHandMirrorAxis Axis);
	static FVector GetAxisVector(EVRExpHandBoneAxis Axis);
	static FVector GetMirrorAxisSign(EVRExpHandMirrorAxis MirrorAxis);
	static int32 GetHandKeypointIndex(EHandKeypoint HandKeypoint);
	static bool TryBuildAxisCorrection(
		const FVRExpHandBoneAxisSettings& AxisSettings,
		FQuat& OutAxisCorrection);

private:
	static bool TryNormalizeTransform(FTransform& Transform);
};
