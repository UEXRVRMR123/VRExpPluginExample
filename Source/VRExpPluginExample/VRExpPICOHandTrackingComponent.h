// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "HeadMountedDisplayTypes.h"
#include "VRExpPICOHandTrackingComponent.generated.h"

#if WITH_EDITOR
struct FPropertyChangedEvent;
#endif

UENUM(BlueprintType)
enum class EVRExpPICOHandType : uint8
{
	HandLeft,
	HandRight,
};

UENUM(BlueprintType)
enum class EVRExpPICOHandGesture : uint8
{
	Pinch,
	Fist,
	OpenPalm,
};

UENUM(BlueprintType)
enum class EVRExpPICOHandMirrorAxis : uint8
{
	X,
	Y,
	Z,
};

UENUM(BlueprintType)
enum class EVRExpPICOHandBoneAxis : uint8
{
	X,
	Y,
	Z,
	NegativeX UMETA(DisplayName = "-X"),
	NegativeY UMETA(DisplayName = "-Y"),
	NegativeZ UMETA(DisplayName = "-Z"),
};

USTRUCT(BlueprintType)
struct FVRExpPICOGestureAxisValues
{
	GENERATED_BODY()

	FVRExpPICOGestureAxisValues()
		: PinchAxis(0.0f)
		, FistAxis(0.0f)
		, OpenPalmAxis(0.0f)
		, bGestureDataValid(false)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "PICO|HandTracking|Gesture")
	float PinchAxis;

	UPROPERTY(BlueprintReadOnly, Category = "PICO|HandTracking|Gesture")
	float FistAxis;

	UPROPERTY(BlueprintReadOnly, Category = "PICO|HandTracking|Gesture")
	float OpenPalmAxis;

	UPROPERTY(BlueprintReadOnly, Category = "PICO|HandTracking|Gesture")
	bool bGestureDataValid;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVRExpPICOHandGestureEvent, EVRExpPICOHandGesture, Gesture, float, AxisValue);

USTRUCT(BlueprintType)
struct FVRExpPICOHandBoneAxisSettings
{
	GENERATED_BODY()

	FVRExpPICOHandBoneAxisSettings()
		: ForwardAxis(EVRExpPICOHandBoneAxis::X)
		, UpAxis(EVRExpPICOHandBoneAxis::Z)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Axis")
	EVRExpPICOHandBoneAxis ForwardAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Axis")
	EVRExpPICOHandBoneAxis UpAxis;
};

USTRUCT(BlueprintType)
struct FVRExpPICOHandMirrorSettings
{
	GENERATED_BODY()

	FVRExpPICOHandMirrorSettings()
		: bEnableMirror(false)
		, bAutoMirrorByHandType(true)
		, SourceMeshHandType(EVRExpPICOHandType::HandLeft)
		, MirrorAxis(EVRExpPICOHandMirrorAxis::Y)
		, bMirrorBonePose(true)
		, PoseMirrorFlipAxis(EVRExpPICOHandMirrorAxis::Y)
		, bApplyWristBoneTransformWhenMirrored(false)
		, UnmirroredMeshScale(FVector::OneVector)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Mirror")
	bool bEnableMirror;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	bool bAutoMirrorByHandType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	EVRExpPICOHandType SourceMeshHandType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	EVRExpPICOHandMirrorAxis MirrorAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	bool bMirrorBonePose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	EVRExpPICOHandMirrorAxis PoseMirrorFlipAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Mirror", meta = (EditCondition = "bEnableMirror"))
	bool bApplyWristBoneTransformWhenMirrored;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Mirror")
	FVector UnmirroredMeshScale;
};

USTRUCT(BlueprintType)
struct FVRExpPICORotationAdjustment
{
	GENERATED_BODY()

	FVRExpPICORotationAdjustment()
		: bEnableRotationOffset(false)
		, RotationOffset(FRotator::ZeroRotator)
		, bEnableAxisAdjustment(false)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Rotation")
	bool bEnableRotationOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Rotation", meta = (EditCondition = "bEnableRotationOffset"))
	FRotator RotationOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Rotation")
	bool bEnableAxisAdjustment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Rotation", meta = (EditCondition = "bEnableAxisAdjustment"))
	FVRExpPICOHandBoneAxisSettings AxisSettings;
};

USTRUCT(BlueprintType)
struct FVRExpPICOHandBoneMapping
{
	GENERATED_BODY()

	FVRExpPICOHandBoneMapping()
		: BoneName(NAME_None)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking")
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking")
	FVRExpPICORotationAdjustment RotationAdjustment;
};

UCLASS(Blueprintable, ClassGroup = (PICO), meta = (BlueprintSpawnableComponent, DisplayName = "VRExp PICO Hand Tracking Component"))
class VREXPPLUGINEXAMPLE_API UVRExpPICOHandTrackingComponent : public UPoseableMeshComponent
{
	GENERATED_BODY()

public:
	UVRExpPICOHandTrackingComponent(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintPure, Category = "PICO|HandTracking|Gesture")
	float GetGestureAxis(EVRExpPICOHandGesture Gesture) const;

	UFUNCTION(BlueprintPure, Category = "PICO|HandTracking|Gesture")
	FVRExpPICOGestureAxisValues GetGestureAxisValues() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PICO|HandTracking")
	EVRExpPICOHandType SkeletonMeshType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PICO|HandTracking")
	bool ApplyLocationToEveryBone;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PICO|HandTracking")
	bool AutoHide;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PICO|HandTracking")
	TMap<EHandKeypoint, FVRExpPICOHandBoneMapping> BoneMappings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PICO|HandTracking")
	bool AutoScaleComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Mirror")
	FVRExpPICOHandMirrorSettings MirrorSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Rotation")
	FVRExpPICORotationAdjustment ComponentRotationAdjustment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Gesture")
	bool bEnableGestureRecognition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Gesture", meta = (ClampMin = "0.0"))
	float PinchClosedDistanceScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Gesture", meta = (ClampMin = "0.0"))
	float PinchOpenDistanceScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Gesture", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float FingerClosedAngleDegrees;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Gesture", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float FingerOpenAngleDegrees;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Gesture", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GestureActiveThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Gesture", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GestureInactiveThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Gesture", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GestureAxisBroadcastDelta;

	UPROPERTY(BlueprintAssignable, Category = "PICO|HandTracking|Gesture")
	FVRExpPICOHandGestureEvent OnGestureStarted;

	UPROPERTY(BlueprintAssignable, Category = "PICO|HandTracking|Gesture")
	FVRExpPICOHandGestureEvent OnGestureEnded;

	UPROPERTY(BlueprintAssignable, Category = "PICO|HandTracking|Gesture")
	FVRExpPICOHandGestureEvent OnGestureAxisChanged;

private:
	struct FResolvedBoneMapping
	{
		EHandKeypoint HandKeypoint;
		FName BoneName;
		int32 BoneIndex;
		int32 ParentIndex;
		FVRExpPICORotationAdjustment RotationAdjustment;
	};

	struct FGestureFingerKeypoints
	{
		EHandKeypoint Metacarpal;
		EHandKeypoint Proximal;
		EHandKeypoint Tip;
	};

	void EvaluateRawOpenXRGestures(const TArray<FVector>& WorldPositions, const TArray<FQuat>& WorldRotations);
	void ResetGestureState();
	bool BuildGestureLocalPositions(const TArray<FVector>& WorldPositions, TArray<FVector>& OutLocalPositions, float& OutPalmWidth) const;
	FVRExpPICOGestureAxisValues CalculateGestureAxisValues(const TArray<FVector>& GestureLocalPositions, float PalmWidth) const;
	float CalculatePinchAxis(const TArray<FVector>& GestureLocalPositions, float PalmWidth) const;
	float CalculateFistAxis(const TArray<FVector>& GestureLocalPositions) const;
	float CalculateFingerCurlAxis(const TArray<FVector>& GestureLocalPositions, const FGestureFingerKeypoints& FingerKeypoints) const;
	void ApplyGestureAxisValues(const FVRExpPICOGestureAxisValues& NewAxisValues);
	void UpdateGestureState(EVRExpPICOHandGesture Gesture, float NewAxisValue);
	bool IsGestureActive(EVRExpPICOHandGesture Gesture) const;
	void SetGestureActive(EVRExpPICOHandGesture Gesture, bool bActive);

	bool ApplyTrackedHandPose(const TArray<FVector>& WorldPositions, const TArray<FQuat>& WorldRotations);
	void BuildResolvedBoneMappings(int32 NumKeypoints, TArray<FResolvedBoneMapping>& OutMappings) const;
	void ApplyEffectiveMeshScale(float PICOScale);
	bool ShouldMirrorForCurrentHand() const;
	FTransform BuildPoseComponentTransform() const;

	FQuat ApplyComponentRotationAdjustment(const FQuat& RawRotation, const FVRExpPICORotationAdjustment& RotationAdjustment) const;
	FQuat ApplyParentBoneRotationOffset(const FQuat& ParentBoneSpaceRotation, const FVRExpPICORotationAdjustment& RotationAdjustment) const;
	FQuat ApplyAxisAdjustment(const FQuat& ComponentSpaceRotation, const FVRExpPICORotationAdjustment& RotationAdjustment) const;

	static EControllerHand ToControllerHand(EVRExpPICOHandType HandType);
	static EAxis::Type ToEAxis(EVRExpPICOHandMirrorAxis Axis);
	static FVector GetAxisVector(EVRExpPICOHandBoneAxis Axis);
	static FVector GetMirrorAxisSign(EVRExpPICOHandMirrorAxis MirrorAxis);
	static int32 GetHandKeypointIndex(EHandKeypoint HandKeypoint);
	static bool TryGetKeypointPosition(const TArray<FVector>& Positions, EHandKeypoint HandKeypoint, FVector& OutPosition);
	static float GetGestureAxisFromValues(const FVRExpPICOGestureAxisValues& AxisValues, EVRExpPICOHandGesture Gesture);
	static bool TryBuildAxisCorrection(const FVRExpPICOHandBoneAxisSettings& AxisSettings, FQuat& OutAxisCorrection);

	FVRExpPICOGestureAxisValues CurrentGestureAxisValues;
	bool bPinchGestureActive;
	bool bFistGestureActive;
	bool bOpenPalmGestureActive;

	bool bHandTrackingAvailable;
	bool bIsRunning;

	static int32 HandTrackingInstanceCount;
};
