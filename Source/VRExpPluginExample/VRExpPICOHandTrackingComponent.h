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

private:
	struct FResolvedBoneMapping
	{
		EHandKeypoint HandKeypoint;
		FName BoneName;
		int32 BoneIndex;
		int32 ParentIndex;
		FVRExpPICORotationAdjustment RotationAdjustment;
	};

	bool ApplyTrackedHandPose(const TArray<FVector>& WorldPositions, const TArray<FQuat>& WorldRotations);
	void BuildResolvedBoneMappings(int32 NumKeypoints, TArray<FResolvedBoneMapping>& OutMappings) const;
	void ApplyEffectiveMeshScale(float PICOScale);

	FQuat ApplyComponentRotationAdjustment(const FQuat& RawRotation, const FVRExpPICORotationAdjustment& RotationAdjustment) const;
	FQuat ApplyParentBoneRotationOffset(const FQuat& ParentBoneSpaceRotation, const FVRExpPICORotationAdjustment& RotationAdjustment) const;
	FQuat ApplyAxisAdjustment(const FQuat& ComponentSpaceRotation, const FVRExpPICORotationAdjustment& RotationAdjustment) const;

	static EControllerHand ToControllerHand(EVRExpPICOHandType HandType);
	static FVector GetAxisVector(EVRExpPICOHandBoneAxis Axis);
	static FVector GetMirrorAxisSign(EVRExpPICOHandMirrorAxis MirrorAxis);
	static int32 GetHandKeypointIndex(EHandKeypoint HandKeypoint);
	static bool TryBuildAxisCorrection(const FVRExpPICOHandBoneAxisSettings& AxisSettings, FQuat& OutAxisCorrection);

	bool bHandTrackingAvailable;
	bool bIsRunning;

	static int32 HandTrackingInstanceCount;
};
