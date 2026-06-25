// Fill out your copyright notice in the Description page of Project Settings.

#include "VRExpPICOHandTrackingComponent.h"

#include "Engine/SkeletalMesh.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "PICO_HandTrackingFunctionLibrary.h"

int32 UVRExpPICOHandTrackingComponent::HandTrackingInstanceCount = 0;

UVRExpPICOHandTrackingComponent::UVRExpPICOHandTrackingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SkeletonMeshType(EVRExpPICOHandType::HandLeft)
	, ApplyLocationToEveryBone(false)
	, AutoHide(false)
	, AutoScaleComponent(false)
	, bEnablePerBoneAxisAdjustment(false)
	, bHandTrackingAvailable(false)
	, bIsRunning(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	for (int32 KeypointIndex = 0; KeypointIndex < EHandKeypointCount; ++KeypointIndex)
	{
		const EHandKeypoint HandKeypoint = static_cast<EHandKeypoint>(KeypointIndex);
		BoneMappings.FindOrAdd(HandKeypoint);
	}
}

void UVRExpPICOHandTrackingComponent::BeginPlay()
{
	Super::BeginPlay();

	FXRMotionControllerData Data;
	UHeadMountedDisplayFunctionLibrary::GetMotionControllerData(nullptr, ToControllerHand(SkeletonMeshType), Data);
	if (Data.DeviceVisualType == EXRVisualType::Hand || UHandTrackingFunctionLibraryPICO::IsHandTrackingSupportPICO())
	{
		bHandTrackingAvailable = true;
	}

	if (AutoHide)
	{
		SetHiddenInGame(true, true);
	}

	++HandTrackingInstanceCount;
}

void UVRExpPICOHandTrackingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (--HandTrackingInstanceCount == 0 && bIsRunning)
	{
		UHandTrackingFunctionLibraryPICO::StopHandTrackingPICO();
	}
}

void UVRExpPICOHandTrackingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool bHidden = true;
	if (bHandTrackingAvailable && GetSkinnedAsset())
	{
		const EControllerHand ControllerHand = ToControllerHand(SkeletonMeshType);

		FXRMotionControllerData Data;
		UHeadMountedDisplayFunctionLibrary::GetMotionControllerData(nullptr, ControllerHand, Data);
		if (Data.bValid)
		{
			bHidden = false;
			for (const TPair<EHandKeypoint, FVRExpPICOHandBoneMapping>& MappingPair : BoneMappings)
			{
				const EHandKeypoint HandKeypoint = MappingPair.Key;
				const FVRExpPICOHandBoneMapping& BoneMapping = MappingPair.Value;
				const FName BoneName = BoneMapping.BoneName;
				const int32 BoneIndex = GetSkinnedAsset()->GetRefSkeleton().FindBoneIndex(BoneName);
				if (BoneIndex >= 0 && Data.HandKeyPositions.IsValidIndex(static_cast<uint8>(HandKeypoint)))
				{
					const FQuat& WorldRotation = Data.HandKeyRotations[static_cast<uint8>(HandKeypoint)];
					const FVector& WorldLocation = Data.HandKeyPositions[static_cast<uint8>(HandKeypoint)];
					const FQuat CorrectedRotation = ApplyAxisCorrectionToRotation(BoneMapping, WorldRotation);

					SetBoneRotationByName(BoneName, CorrectedRotation.Rotator(), EBoneSpaces::WorldSpace);

					if (HandKeypoint == EHandKeypoint::Wrist || ApplyLocationToEveryBone)
					{
						SetBoneLocationByName(BoneName, WorldLocation, EBoneSpaces::WorldSpace);
					}

					if (HandKeypoint == EHandKeypoint::Wrist)
					{
						SetWorldLocation(WorldLocation);
						SetWorldRotation(ApplyComponentAxisCorrectionToRotation(WorldRotation));
					}

					if (AutoScaleComponent)
					{
						float Scale = 1.0f;
						UHandTrackingFunctionLibraryPICO::GetHandTrackingMeshScalePICO(ControllerHand, Scale);
						SetRelativeScale3D(FVector(Scale));
					}
				}
			}
		}
		else
		{
			if (!bIsRunning)
			{
				UHandTrackingFunctionLibraryPICO::StartHandTrackingPICO();
			}

			bIsRunning = UHandTrackingFunctionLibraryPICO::IsHandTrackingRunningPICO();

			TArray<FVector> OutPositions;
			TArray<FQuat> OutRotations;
			TArray<float> OutRadii;
			TArray<FVector> LinearVelocity;
			TArray<FVector> AngularVelocity;
			float Scale = 1.0f;
			if (bIsRunning && UHandTrackingFunctionLibraryPICO::UpdateHandTrackingDataPICO() && UHandTrackingFunctionLibraryPICO::GetHandTrackingDataPICO(ControllerHand, OutPositions, OutRotations, OutRadii, LinearVelocity, AngularVelocity, Scale))
			{
				bHidden = false;
				for (const TPair<EHandKeypoint, FVRExpPICOHandBoneMapping>& MappingPair : BoneMappings)
				{
					const EHandKeypoint HandKeypoint = MappingPair.Key;
					const FVRExpPICOHandBoneMapping& BoneMapping = MappingPair.Value;
					const FName BoneName = BoneMapping.BoneName;
					const int32 BoneIndex = GetSkinnedAsset()->GetRefSkeleton().FindBoneIndex(BoneName);
					if (BoneIndex >= 0)
					{
						const uint8 KeypointIndex = static_cast<uint8>(HandKeypoint);
						if (OutPositions.IsValidIndex(KeypointIndex) && OutRotations.IsValidIndex(KeypointIndex))
						{
							const FQuat& WorldRotation = OutRotations[KeypointIndex];
							const FVector& WorldLocation = OutPositions[KeypointIndex];
							const FQuat CorrectedRotation = ApplyAxisCorrectionToRotation(BoneMapping, WorldRotation);

							SetBoneRotationByName(BoneName, CorrectedRotation.Rotator(), EBoneSpaces::WorldSpace);

							if (HandKeypoint == EHandKeypoint::Wrist || ApplyLocationToEveryBone)
							{
								SetBoneLocationByName(BoneName, WorldLocation, EBoneSpaces::WorldSpace);
							}

							if (HandKeypoint == EHandKeypoint::Wrist)
							{
								SetWorldLocation(WorldLocation);
								SetWorldRotation(ApplyComponentAxisCorrectionToRotation(WorldRotation));
							}

							if (AutoScaleComponent)
							{
								SetRelativeScale3D(FVector(Scale));
							}
						}
					}
				}
			}
		}
	}

	if (AutoHide && bHidden != bHiddenInGame)
	{
		SetHiddenInGame(bHidden, true);
	}
}

FQuat UVRExpPICOHandTrackingComponent::ApplyAxisCorrectionToRotation(const FVRExpPICOHandBoneMapping& BoneMapping, const FQuat& WorldRotation) const
{
	if (!bEnablePerBoneAxisAdjustment || !BoneMapping.bEnableAxisAdjustment)
	{
		return WorldRotation;
	}

	FQuat AxisCorrection = FQuat::Identity;
	if (!TryBuildAxisCorrection(BoneMapping.AxisSettings, AxisCorrection))
	{
		return WorldRotation;
	}

	return (WorldRotation * AxisCorrection).GetNormalized();
}

FQuat UVRExpPICOHandTrackingComponent::ApplyComponentAxisCorrectionToRotation(const FQuat& WorldRotation) const
{
	FQuat AxisCorrection = FQuat::Identity;
	if (!TryBuildAxisCorrection(ComponentAxisSettings, AxisCorrection))
	{
		return WorldRotation;
	}

	return (WorldRotation * AxisCorrection).GetNormalized();
}

EControllerHand UVRExpPICOHandTrackingComponent::ToControllerHand(EVRExpPICOHandType HandType)
{
	return HandType == EVRExpPICOHandType::HandRight ? EControllerHand::Right : EControllerHand::Left;
}

FVector UVRExpPICOHandTrackingComponent::GetAxisVector(EVRExpPICOHandBoneAxis Axis)
{
	switch (Axis)
	{
	case EVRExpPICOHandBoneAxis::X:
		return FVector::ForwardVector;
	case EVRExpPICOHandBoneAxis::Y:
		return FVector::RightVector;
	case EVRExpPICOHandBoneAxis::Z:
		return FVector::UpVector;
	case EVRExpPICOHandBoneAxis::NegativeX:
		return -FVector::ForwardVector;
	case EVRExpPICOHandBoneAxis::NegativeY:
		return -FVector::RightVector;
	case EVRExpPICOHandBoneAxis::NegativeZ:
		return -FVector::UpVector;
	default:
		return FVector::ForwardVector;
	}
}

bool UVRExpPICOHandTrackingComponent::TryBuildAxisCorrection(const FVRExpPICOHandBoneAxisSettings& AxisSettings, FQuat& OutAxisCorrection)
{
	const FVector ForwardVector = GetAxisVector(AxisSettings.ForwardAxis);
	const FVector UpVector = GetAxisVector(AxisSettings.UpAxis);
	if (FMath::Abs(FVector::DotProduct(ForwardVector, UpVector)) > 1.0f - KINDA_SMALL_NUMBER)
	{
		OutAxisCorrection = FQuat::Identity;
		return false;
	}

	const FQuat BoneBasisRotation = FRotationMatrix::MakeFromXZ(ForwardVector, UpVector).ToQuat();
	OutAxisCorrection = BoneBasisRotation.Inverse().GetNormalized();
	return true;
}
