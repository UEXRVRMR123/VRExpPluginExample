// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HandTracking/VRExpHandTrackingTypes.h"
#include "VRExpHandPoseTypes.generated.h"

UENUM(BlueprintType)
enum class EVRExpHandMirrorAxis : uint8
{
	X,
	Y,
	Z,
};

UENUM(BlueprintType)
enum class EVRExpHandBoneAxis : uint8
{
	X,
	Y,
	Z,
	NegativeX UMETA(DisplayName = "-X"),
	NegativeY UMETA(DisplayName = "-Y"),
	NegativeZ UMETA(DisplayName = "-Z"),
};

UENUM(BlueprintType)
enum class EVRExpHandPoseTransformApplicationMode : uint8
{
	RotationOnly UMETA(DisplayName = "Rotation Only"),
	RotationAndWristLocation UMETA(DisplayName = "Rotation + Wrist Location"),
	RotationAndAllLocations UMETA(DisplayName = "Rotation + All Locations"),
};

USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpHandBoneAxisSettings
{
	GENERATED_BODY()

	FVRExpHandBoneAxisSettings()
		: ForwardAxis(EVRExpHandBoneAxis::X)
		, UpAxis(EVRExpHandBoneAxis::Z)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Axis")
	EVRExpHandBoneAxis ForwardAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Axis")
	EVRExpHandBoneAxis UpAxis;
};

USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpHandMirrorSettings
{
	GENERATED_BODY()

	FVRExpHandMirrorSettings()
		: bEnableMirror(false)
		, bAutoMirrorByHandType(true)
		, SourceMeshHandType(EVRExpHandType::HandLeft)
		, MirrorAxis(EVRExpHandMirrorAxis::Y)
		, bMirrorBonePose(true)
		, PoseMirrorFlipAxis(EVRExpHandMirrorAxis::Y)
		, bApplyWristBoneTransformWhenMirrored(false)
		, UnmirroredMeshScale(FVector::OneVector)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Mirror")
	bool bEnableMirror;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	bool bAutoMirrorByHandType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	EVRExpHandType SourceMeshHandType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	EVRExpHandMirrorAxis MirrorAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	bool bMirrorBonePose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	EVRExpHandMirrorAxis PoseMirrorFlipAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	bool bApplyWristBoneTransformWhenMirrored;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Mirror")
	FVector UnmirroredMeshScale;
};

/** Pose-only mirror options used by the animation node. Component scale is intentionally excluded. */
USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpHandPoseMirrorSettings
{
	GENERATED_BODY()

	FVRExpHandPoseMirrorSettings()
		: bEnableMirror(false)
		, bAutoMirrorByHandType(true)
		, SourceMeshHandType(EVRExpHandType::HandLeft)
		, MirrorAxis(EVRExpHandMirrorAxis::Y)
		, PoseMirrorFlipAxis(EVRExpHandMirrorAxis::Y)
		, bApplyWristBoneTransformWhenMirrored(false)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Mirror")
	bool bEnableMirror;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	bool bAutoMirrorByHandType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	EVRExpHandType SourceMeshHandType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	EVRExpHandMirrorAxis MirrorAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	EVRExpHandMirrorAxis PoseMirrorFlipAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	bool bApplyWristBoneTransformWhenMirrored;
};

USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpHandRotationAdjustment
{
	GENERATED_BODY()

	FVRExpHandRotationAdjustment()
		: bEnableRotationOffset(false)
		, RotationOffset(FRotator::ZeroRotator)
		, bEnableAxisAdjustment(false)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Rotation")
	bool bEnableRotationOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Rotation", meta = (EditCondition = "bEnableRotationOffset"))
	FRotator RotationOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Rotation")
	bool bEnableAxisAdjustment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Rotation", meta = (EditCondition = "bEnableAxisAdjustment"))
	FVRExpHandBoneAxisSettings AxisSettings;
};

USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpHandBoneMapping
{
	GENERATED_BODY()

	FVRExpHandBoneMapping()
		: BoneName(NAME_None)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking")
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking")
	FVRExpHandRotationAdjustment RotationAdjustment;
};

/** Complete pose configuration supplied to FAnimNode_ApplyVRExpHandPose. */
USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpHandPoseConfig
{
	GENERATED_BODY()

	FVRExpHandPoseConfig()
		: TransformApplicationMode(EVRExpHandPoseTransformApplicationMode::RotationOnly)
		, bApplyWristBone(false)
	{
		for (int32 KeypointIndex = 0; KeypointIndex < EHandKeypointCount; ++KeypointIndex)
		{
			BoneMappings.FindOrAdd(static_cast<EHandKeypoint>(KeypointIndex));
		}
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking")
	TMap<EHandKeypoint, FVRExpHandBoneMapping> BoneMappings;

	/** Selects whether tracked positions are ignored, applied to the wrist, or applied to every mapped bone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Application")
	EVRExpHandPoseTransformApplicationMode TransformApplicationMode;

	/** When disabled, the wrist keeps its complete input pose while other mapped bones are driven. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Application")
	bool bApplyWristBone;

	/** Pose-only mirroring. The animation node never changes component scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Mirror")
	FVRExpHandPoseMirrorSettings PoseMirrorSettings;

	/** Adjusts the tracking-to-mesh rotation basis without rotating the skeletal mesh component itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Rotation")
	FVRExpHandRotationAdjustment ComponentRotationAdjustment;
};

/** Runtime-only resolved mapping shared by pose consumers. Bone indexes are consumer-defined. */
struct VREXPANSIONEXTENSIONS_API FVRExpResolvedHandBoneMapping
{
	EHandKeypoint HandKeypoint = EHandKeypoint::Wrist;
	FName BoneName = NAME_None;
	int32 BoneIndex = INDEX_NONE;
	int32 ParentIndex = INDEX_NONE;
	FVRExpHandRotationAdjustment RotationAdjustment;
};

/** Component-space result produced by the common hand-pose solver. */
struct VREXPANSIONEXTENSIONS_API FVRExpSolvedHandBoneTransform
{
	EHandKeypoint HandKeypoint = EHandKeypoint::Wrist;
	int32 BoneIndex = INDEX_NONE;
	int32 ParentIndex = INDEX_NONE;
	FTransform ComponentTransform = FTransform::Identity;
};
