// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "HeadMountedDisplayTypes.h"
#include "VRExpPICOHandTrackingComponent.generated.h"

UENUM(BlueprintType)
enum class EVRExpPICOHandType : uint8
{
	HandLeft,
	HandRight,
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
struct FVRExpPICOHandBoneMapping
{
	GENERATED_BODY()

	FVRExpPICOHandBoneMapping()
		: BoneName(NAME_None)
		, bEnableAxisAdjustment(false)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking")
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking")
	bool bEnableAxisAdjustment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking", meta = (EditCondition = "bEnableAxisAdjustment"))
	FVRExpPICOHandBoneAxisSettings AxisSettings;
};

UCLASS(Blueprintable, ClassGroup = (PICO), meta = (BlueprintSpawnableComponent, DisplayName = "VRExp PICO Hand Tracking Component"))
class VREXPPLUGINEXAMPLE_API UVRExpPICOHandTrackingComponent : public UPoseableMeshComponent
{
	GENERATED_BODY()

public:
	UVRExpPICOHandTrackingComponent(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Axis")
	bool bEnablePerBoneAxisAdjustment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO|HandTracking|Axis")
	FVRExpPICOHandBoneAxisSettings ComponentAxisSettings;

private:
	FQuat ApplyAxisCorrectionToRotation(const FVRExpPICOHandBoneMapping& BoneMapping, const FQuat& WorldRotation) const;
	FQuat ApplyComponentAxisCorrectionToRotation(const FQuat& WorldRotation) const;

	static EControllerHand ToControllerHand(EVRExpPICOHandType HandType);
	static FVector GetAxisVector(EVRExpPICOHandBoneAxis Axis);
	static bool TryBuildAxisCorrection(const FVRExpPICOHandBoneAxisSettings& AxisSettings, FQuat& OutAxisCorrection);

	bool bHandTrackingAvailable;
	bool bIsRunning;

	static int32 HandTrackingInstanceCount;
};
