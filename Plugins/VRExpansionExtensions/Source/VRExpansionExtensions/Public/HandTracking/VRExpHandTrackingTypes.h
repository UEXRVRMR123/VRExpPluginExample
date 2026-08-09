// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HeadMountedDisplayTypes.h"
#include "VRExpHandTrackingTypes.generated.h"

UENUM(BlueprintType)
enum class EVRExpHandType : uint8
{
	HandLeft,
	HandRight,
};

UENUM(BlueprintType)
enum class EVRExpHandGesture : uint8
{
	Pinch,
	Fist,
	OpenPalm,
};

UENUM(BlueprintType)
enum class EVRExpHandTrackingSpace : uint8
{
	WorldSpace,
	XRSpace,
};

UENUM(BlueprintType)
enum class EVRExpPalmFacingDirection : uint8
{
	Up,
	Down,
};

UENUM(BlueprintType)
enum class EVRExpPalmNormalCalculationMode : uint8
{
	PalmRotationAxis UMETA(DisplayName = "Palm Rotation Axis"),
	ReconstructFromKeypoints UMETA(DisplayName = "Reconstruct From Keypoints"),
};

UENUM(BlueprintType)
enum class EVRExpPalmRotationNormalAxis : uint8
{
	X,
	Y,
	Z,
};

USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpHandGestureAxisValues
{
	GENERATED_BODY()

	FVRExpHandGestureAxisValues()
		: PinchAxis(0.0f)
		, FistAxis(0.0f)
		, OpenPalmAxis(0.0f)
		, bGestureDataValid(false)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|HandTracking|Gesture")
	float PinchAxis;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|HandTracking|Gesture")
	float FistAxis;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|HandTracking|Gesture")
	float OpenPalmAxis;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|HandTracking|Gesture")
	bool bGestureDataValid;
};

USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpPalmFacingAxisValues
{
	GENERATED_BODY()

	FVRExpPalmFacingAxisValues()
		: PalmUpAxis(0.0f)
		, PalmDownAxis(0.0f)
		, bPalmFacingDataValid(false)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|HandTracking|PalmFacing")
	float PalmUpAxis;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|HandTracking|PalmFacing")
	float PalmDownAxis;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|HandTracking|PalmFacing")
	bool bPalmFacingDataValid;
};

USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpHandGestureRecognitionSettings
{
	GENERATED_BODY()

	FVRExpHandGestureRecognitionSettings()
		: bEnableGestureRecognition(true)
		, PinchClosedDistanceScale(0.25f)
		, PinchOpenDistanceScale(0.85f)
		, FingerClosedAngleDegrees(95.0f)
		, FingerOpenAngleDegrees(25.0f)
		, GestureActiveThreshold(0.75f)
		, GestureInactiveThreshold(0.35f)
		, GestureAxisBroadcastDelta(0.02f)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Gesture")
	bool bEnableGestureRecognition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Gesture", meta = (ClampMin = "0.0"))
	float PinchClosedDistanceScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Gesture", meta = (ClampMin = "0.0"))
	float PinchOpenDistanceScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Gesture", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float FingerClosedAngleDegrees;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Gesture", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float FingerOpenAngleDegrees;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Gesture", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GestureActiveThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Gesture", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GestureInactiveThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Gesture", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GestureAxisBroadcastDelta;
};

USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpPalmFacingRecognitionSettings
{
	GENERATED_BODY()

	FVRExpPalmFacingRecognitionSettings()
		: PalmFacingActiveThreshold(0.75f)
		, PalmFacingInactiveThreshold(0.35f)
		, PalmFacingAxisBroadcastDelta(0.02f)
		, PalmNormalCalculationMode(EVRExpPalmNormalCalculationMode::PalmRotationAxis)
		, PalmRotationNormalAxis(EVRExpPalmRotationNormalAxis::Z)
		, bInvertPalmFacingNormal(false)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|PalmFacing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PalmFacingActiveThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|PalmFacing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PalmFacingInactiveThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|PalmFacing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PalmFacingAxisBroadcastDelta;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|PalmFacing", meta = (DisplayName = "Palm Normal Calculation Mode"))
	EVRExpPalmNormalCalculationMode PalmNormalCalculationMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|PalmFacing", meta = (EditCondition = "PalmNormalCalculationMode == EVRExpPalmNormalCalculationMode::PalmRotationAxis", EditConditionHides))
	EVRExpPalmRotationNormalAxis PalmRotationNormalAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|PalmFacing")
	bool bInvertPalmFacingNormal;
};

USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpTrackedHandState
{
	GENERATED_BODY()

	FVRExpTrackedHandState()
		: HandType(EVRExpHandType::HandLeft)
		, bIsTracked(false)
		, bHasHandTrackingData(false)
		, TrackingStatus(ETrackingStatus::NotTracked)
		, WristTransform(FTransform::Identity)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|HandTracking")
	EVRExpHandType HandType;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|HandTracking")
	bool bIsTracked;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|HandTracking")
	bool bHasHandTrackingData;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|HandTracking")
	ETrackingStatus TrackingStatus;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|HandTracking")
	FTransform WristTransform;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|HandTracking|Gesture")
	FVRExpHandGestureAxisValues GestureAxisValues;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|HandTracking")
	TArray<FTransform> HandKeyTransforms;

	TArray<FVector> HandKeyPositions;
	TArray<FQuat> HandKeyRotations;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVRExpHandGestureEvent, EVRExpHandGesture, Gesture, float, AxisValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVRExpHandGestureHandEvent, EVRExpHandType, HandType, EVRExpHandGesture, Gesture, float, AxisValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVRExpPalmFacingEvent, EVRExpHandTrackingSpace, Space, EVRExpPalmFacingDirection, Direction, float, AxisValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FVRExpPalmFacingHandEvent, EVRExpHandType, HandType, EVRExpHandTrackingSpace, Space, EVRExpPalmFacingDirection, Direction, float, AxisValue);
