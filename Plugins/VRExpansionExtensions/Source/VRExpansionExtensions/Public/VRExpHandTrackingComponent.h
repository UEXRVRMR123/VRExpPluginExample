// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "VRExpHandTrackingTypes.h"
#include "VRExpHandTrackingComponent.generated.h"

#if WITH_EDITOR
struct FPropertyChangedEvent;
#endif

class UVRExpHandTrackingSubsystem;

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

USTRUCT(BlueprintType)
struct FVRExpHandBoneAxisSettings
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
struct FVRExpHandMirrorSettings
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

USTRUCT(BlueprintType)
struct FVRExpHandRotationAdjustment
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
struct FVRExpHandBoneMapping
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

UCLASS(Blueprintable, ClassGroup = (VRExp), meta = (BlueprintSpawnableComponent, DisplayName = "VRExp Hand Tracking Component"))
class VREXPANSIONEXTENSIONS_API UVRExpHandTrackingComponent : public UPoseableMeshComponent
{
	GENERATED_BODY()

public:
	UVRExpHandTrackingComponent(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|Gesture")
	float GetGestureAxis(EVRExpHandGesture Gesture) const;

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|Gesture")
	FVRExpHandGestureAxisValues GetGestureAxisValues() const;

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|PalmFacing")
	float GetPalmFacingAxis(EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction) const;

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|PalmFacing")
	FVRExpPalmFacingAxisValues GetPalmFacingAxisValues(EVRExpHandTrackingSpace Space) const;

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|PalmFacing")
	bool IsPalmFacingActive(EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "VRExp|HandTracking")
	EVRExpHandType SkeletonMeshType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "VRExp|HandTracking")
	bool ApplyLocationToEveryBone;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "VRExp|HandTracking")
	bool AutoHide;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "VRExp|HandTracking")
	TMap<EHandKeypoint, FVRExpHandBoneMapping> BoneMappings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "VRExp|HandTracking")
	bool AutoScaleComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Mirror")
	FVRExpHandMirrorSettings MirrorSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Rotation")
	FVRExpHandRotationAdjustment ComponentRotationAdjustment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Gesture")
	bool bEnableGestureRecognition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|PalmFacing")
	bool bEnablePalmFacingRecognition;

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

	UPROPERTY(BlueprintAssignable, Category = "VRExp|HandTracking|Gesture")
	FVRExpHandGestureEvent OnGestureStarted;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|HandTracking|Gesture")
	FVRExpHandGestureEvent OnGestureEnded;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|HandTracking|Gesture")
	FVRExpHandGestureEvent OnGestureAxisChanged;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|HandTracking|PalmFacing")
	FVRExpPalmFacingEvent OnPalmFacingStarted;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|HandTracking|PalmFacing")
	FVRExpPalmFacingEvent OnPalmFacingEnded;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|HandTracking|PalmFacing")
	FVRExpPalmFacingEvent OnPalmFacingAxisChanged;

private:
	UFUNCTION()
	void HandleSubsystemGestureStarted(EVRExpHandType HandType, EVRExpHandGesture Gesture, float AxisValue);

	UFUNCTION()
	void HandleSubsystemGestureEnded(EVRExpHandType HandType, EVRExpHandGesture Gesture, float AxisValue);

	UFUNCTION()
	void HandleSubsystemGestureAxisChanged(EVRExpHandType HandType, EVRExpHandGesture Gesture, float AxisValue);

	UFUNCTION()
	void HandleSubsystemPalmFacingStarted(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction, float AxisValue);

	UFUNCTION()
	void HandleSubsystemPalmFacingEnded(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction, float AxisValue);

	UFUNCTION()
	void HandleSubsystemPalmFacingAxisChanged(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction, float AxisValue);

	struct FResolvedBoneMapping
	{
		EHandKeypoint HandKeypoint;
		FName BoneName;
		int32 BoneIndex;
		int32 ParentIndex;
		FVRExpHandRotationAdjustment RotationAdjustment;
	};

	UVRExpHandTrackingSubsystem* GetHandTrackingSubsystem() const;
	void BindHandTrackingSubsystemDelegates();
	void UnbindHandTrackingSubsystemDelegates();
	bool ApplyTrackedHandPose(const TArray<FVector>& WorldPositions, const TArray<FQuat>& WorldRotations);
	void BuildResolvedBoneMappings(int32 NumKeypoints, TArray<FResolvedBoneMapping>& OutMappings) const;
	void ApplyEffectiveMeshScale(float TrackingScale);
	bool ShouldMirrorForCurrentHand() const;
	FTransform BuildPoseComponentTransform() const;

	FQuat ApplyComponentRotationAdjustment(const FQuat& RawRotation, const FVRExpHandRotationAdjustment& RotationAdjustment) const;
	FQuat ApplyParentBoneRotationOffset(const FQuat& ParentBoneSpaceRotation, const FVRExpHandRotationAdjustment& RotationAdjustment) const;
	FQuat ApplyAxisAdjustment(const FQuat& ComponentSpaceRotation, const FVRExpHandRotationAdjustment& RotationAdjustment) const;

	static EAxis::Type ToEAxis(EVRExpHandMirrorAxis Axis);
	static FVector GetAxisVector(EVRExpHandBoneAxis Axis);
	static FVector GetMirrorAxisSign(EVRExpHandMirrorAxis MirrorAxis);
	static int32 GetHandKeypointIndex(EHandKeypoint HandKeypoint);
	static bool TryBuildAxisCorrection(const FVRExpHandBoneAxisSettings& AxisSettings, FQuat& OutAxisCorrection);
};

