// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "HandTracking/VRExpHandPoseAnimNodeRuntime.h"
#include "HandTracking/VRExpHandPoseTypes.h"
#include "AnimNode_ApplyVRExpHandPose.generated.h"

/** Applies the selected hand's tracking snapshot using the supplied pose configuration. */
USTRUCT(BlueprintInternalUseOnly)
struct VREXPANSIONEXTENSIONS_API FAnimNode_ApplyVRExpHandPose : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	FAnimNode_ApplyVRExpHandPose();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Pose", meta = (PinShownByDefault))
	EVRExpHandType HandType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Pose", meta = (PinShownByDefault, DisplayName = "Hand Config"))
	FVRExpHandPoseConfig HandConfig;

	// FAnimNode_Base
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

	// FAnimNode_SkeletalControlBase
	virtual void EvaluateSkeletalControl_AnyThread(
		FComponentSpacePoseContext& Output,
		TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

private:
	FVRExpHandPoseAnimNodeRuntime RuntimeState;
	TArray<FTransform> CachedLeftWorldTransforms;
	TArray<FTransform> CachedRightWorldTransforms;
	FTransform CachedMeshWorldTransform;
	bool bHasValidLeftPoseSnapshot;
	bool bHasValidRightPoseSnapshot;
};
