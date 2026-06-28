// Fill out your copyright notice in the Description page of Project Settings.

#include "VRExpPICOHandTrackingComponent.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAsset.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "PICO_HandTrackingFunctionLibrary.h"
#include "UObject/UnrealType.h"

int32 UVRExpPICOHandTrackingComponent::HandTrackingInstanceCount = 0;

UVRExpPICOHandTrackingComponent::UVRExpPICOHandTrackingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SkeletonMeshType(EVRExpPICOHandType::HandLeft)
	, ApplyLocationToEveryBone(false)
	, AutoHide(false)
	, AutoScaleComponent(false)
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

void UVRExpPICOHandTrackingComponent::OnRegister()
{
	Super::OnRegister();

	ApplyEffectiveMeshScale(1.0f);
}

#if WITH_EDITOR
void UVRExpPICOHandTrackingComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplyEffectiveMeshScale(1.0f);
}
#endif

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
		if (!AutoScaleComponent)
		{
			ApplyEffectiveMeshScale(1.0f);
		}

		const EControllerHand ControllerHand = ToControllerHand(SkeletonMeshType);

		FXRMotionControllerData Data;
		UHeadMountedDisplayFunctionLibrary::GetMotionControllerData(nullptr, ControllerHand, Data);
		if (Data.bValid)
		{
			if (AutoScaleComponent)
			{
				float Scale = 1.0f;
				UHandTrackingFunctionLibraryPICO::GetHandTrackingMeshScalePICO(ControllerHand, Scale);
				ApplyEffectiveMeshScale(Scale);
			}

			bHidden = !ApplyTrackedHandPose(Data.HandKeyPositions, Data.HandKeyRotations);
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
				if (AutoScaleComponent)
				{
					ApplyEffectiveMeshScale(Scale);
				}

				bHidden = !ApplyTrackedHandPose(OutPositions, OutRotations);
			}
		}
	}

	if (AutoHide && bHidden != bHiddenInGame)
	{
		SetHiddenInGame(bHidden, true);
	}
}

bool UVRExpPICOHandTrackingComponent::ApplyTrackedHandPose(const TArray<FVector>& WorldPositions, const TArray<FQuat>& WorldRotations)
{
	if (!GetSkinnedAsset())
	{
		return false;
	}

	const int32 WristKeypointIndex = GetHandKeypointIndex(EHandKeypoint::Wrist);
	if (!WorldPositions.IsValidIndex(WristKeypointIndex) || !WorldRotations.IsValidIndex(WristKeypointIndex))
	{
		return false;
	}

	const FQuat WristWorldRotation = WorldRotations[WristKeypointIndex].GetNormalized();
	SetWorldLocation(WorldPositions[WristKeypointIndex]);
	SetWorldRotation(ApplyComponentRotationAdjustment(WristWorldRotation, ComponentRotationAdjustment));

	TArray<FResolvedBoneMapping> ResolvedMappings;
	BuildResolvedBoneMappings(FMath::Min(WorldPositions.Num(), WorldRotations.Num()), ResolvedMappings);
	if (ResolvedMappings.Num() == 0)
	{
		return true;
	}

	const FReferenceSkeleton& RefSkeleton = GetSkinnedAsset()->GetRefSkeleton();
	const int32 NumBones = RefSkeleton.GetNum();
	const bool bMirrorBonePose = ShouldMirrorForCurrentHand() && MirrorSettings.bMirrorBonePose;
	const FTransform PoseComponentWorldTransform = BuildPoseComponentTransform();
	const EAxis::Type PoseMirrorAxis = ToEAxis(MirrorSettings.MirrorAxis);
	const EAxis::Type PoseMirrorFlipAxis = ToEAxis(MirrorSettings.PoseMirrorFlipAxis);
	const TArray<FTransform>& CurrentComponentTransforms = GetComponentSpaceTransforms();

	TArray<FTransform> RawComponentTransforms;
	RawComponentTransforms.SetNum(NumBones);
	TArray<uint8> bHasRawComponentTransform;
	bHasRawComponentTransform.Init(false, NumBones);

	for (const FResolvedBoneMapping& ResolvedMapping : ResolvedMappings)
	{
		const int32 KeypointIndex = GetHandKeypointIndex(ResolvedMapping.HandKeypoint);
		const FTransform WorldTransform(WorldRotations[KeypointIndex].GetNormalized(), WorldPositions[KeypointIndex], FVector::OneVector);
		FTransform ComponentTransform = WorldTransform.GetRelativeTransform(PoseComponentWorldTransform);
		ComponentTransform.NormalizeRotation();

		if (bMirrorBonePose)
		{
			ComponentTransform.Mirror(PoseMirrorAxis, PoseMirrorFlipAxis);
			ComponentTransform.NormalizeRotation();
		}

		RawComponentTransforms[ResolvedMapping.BoneIndex] = ComponentTransform;
		bHasRawComponentTransform[ResolvedMapping.BoneIndex] = true;
	}

	TArray<FTransform> HierarchyComponentTransforms;
	HierarchyComponentTransforms.SetNum(NumBones);
	TArray<uint8> bHasHierarchyComponentTransform;
	bHasHierarchyComponentTransform.Init(false, NumBones);

	auto GetParentComponentTransform = [&CurrentComponentTransforms](int32 ParentIndex, const TArray<FTransform>& CandidateTransforms, const TArray<uint8>& bHasCandidateTransform)
	{
		if (ParentIndex != INDEX_NONE)
		{
			if (CandidateTransforms.IsValidIndex(ParentIndex) && bHasCandidateTransform.IsValidIndex(ParentIndex) && bHasCandidateTransform[ParentIndex])
			{
				return CandidateTransforms[ParentIndex];
			}

			if (CurrentComponentTransforms.IsValidIndex(ParentIndex))
			{
				return CurrentComponentTransforms[ParentIndex];
			}
		}

		return FTransform::Identity;
	};

	for (const FResolvedBoneMapping& ResolvedMapping : ResolvedMappings)
	{
		const FTransform& RawComponentTransform = RawComponentTransforms[ResolvedMapping.BoneIndex];
		const FTransform RawParentComponentTransform = GetParentComponentTransform(ResolvedMapping.ParentIndex, RawComponentTransforms, bHasRawComponentTransform);
		FTransform ParentBoneSpaceTransform = RawComponentTransform.GetRelativeTransform(RawParentComponentTransform);
		ParentBoneSpaceTransform.NormalizeRotation();
		ParentBoneSpaceTransform.SetRotation(ApplyParentBoneRotationOffset(ParentBoneSpaceTransform.GetRotation(), ResolvedMapping.RotationAdjustment));

		const FTransform AdjustedParentComponentTransform = GetParentComponentTransform(ResolvedMapping.ParentIndex, HierarchyComponentTransforms, bHasHierarchyComponentTransform);
		FTransform HierarchyComponentTransform = ParentBoneSpaceTransform * AdjustedParentComponentTransform;
		HierarchyComponentTransform.NormalizeRotation();

		HierarchyComponentTransforms[ResolvedMapping.BoneIndex] = HierarchyComponentTransform;
		bHasHierarchyComponentTransform[ResolvedMapping.BoneIndex] = true;

		FTransform OutputComponentTransform = HierarchyComponentTransform;
		OutputComponentTransform.SetRotation(ApplyAxisAdjustment(OutputComponentTransform.GetRotation(), ResolvedMapping.RotationAdjustment));

		if (bMirrorBonePose && ResolvedMapping.HandKeypoint == EHandKeypoint::Wrist && !MirrorSettings.bApplyWristBoneTransformWhenMirrored)
		{
			continue;
		}

		SetBoneRotationByName(ResolvedMapping.BoneName, OutputComponentTransform.GetRotation().Rotator(), EBoneSpaces::ComponentSpace);
		if (ResolvedMapping.HandKeypoint == EHandKeypoint::Wrist || ApplyLocationToEveryBone)
		{
			SetBoneLocationByName(ResolvedMapping.BoneName, OutputComponentTransform.GetLocation(), EBoneSpaces::ComponentSpace);
		}
	}

	return true;
}

void UVRExpPICOHandTrackingComponent::BuildResolvedBoneMappings(int32 NumKeypoints, TArray<FResolvedBoneMapping>& OutMappings) const
{
	OutMappings.Reset();

	if (!GetSkinnedAsset())
	{
		return;
	}

	const FReferenceSkeleton& RefSkeleton = GetSkinnedAsset()->GetRefSkeleton();
	for (const TPair<EHandKeypoint, FVRExpPICOHandBoneMapping>& MappingPair : BoneMappings)
	{
		const FVRExpPICOHandBoneMapping& BoneMapping = MappingPair.Value;
		if (BoneMapping.BoneName.IsNone())
		{
			continue;
		}

		const int32 KeypointIndex = GetHandKeypointIndex(MappingPair.Key);
		if (KeypointIndex < 0 || KeypointIndex >= NumKeypoints)
		{
			continue;
		}

		const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneMapping.BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			continue;
		}

		FResolvedBoneMapping ResolvedMapping;
		ResolvedMapping.HandKeypoint = MappingPair.Key;
		ResolvedMapping.BoneName = BoneMapping.BoneName;
		ResolvedMapping.BoneIndex = BoneIndex;
		ResolvedMapping.ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
		ResolvedMapping.RotationAdjustment = BoneMapping.RotationAdjustment;
		OutMappings.Add(ResolvedMapping);
	}

	OutMappings.Sort([](const FResolvedBoneMapping& A, const FResolvedBoneMapping& B)
	{
		return A.BoneIndex < B.BoneIndex;
	});
}

bool UVRExpPICOHandTrackingComponent::ShouldMirrorForCurrentHand() const
{
	if (!MirrorSettings.bEnableMirror)
	{
		return false;
	}

	return MirrorSettings.bAutoMirrorByHandType
		? MirrorSettings.SourceMeshHandType != SkeletonMeshType
		: true;
}

FTransform UVRExpPICOHandTrackingComponent::BuildPoseComponentTransform() const
{
	FTransform PoseComponentTransform = GetComponentTransform();
	const FVector ComponentScale = PoseComponentTransform.GetScale3D();
	PoseComponentTransform.SetScale3D(FVector(FMath::Abs(ComponentScale.X), FMath::Abs(ComponentScale.Y), FMath::Abs(ComponentScale.Z)));
	PoseComponentTransform.NormalizeRotation();
	return PoseComponentTransform;
}

void UVRExpPICOHandTrackingComponent::ApplyEffectiveMeshScale(float PICOScale)
{
	FVector EffectiveScale = MirrorSettings.UnmirroredMeshScale * PICOScale;

	if (ShouldMirrorForCurrentHand())
	{
		const FVector MirrorSign = GetMirrorAxisSign(MirrorSettings.MirrorAxis);
		EffectiveScale.X *= MirrorSign.X;
		EffectiveScale.Y *= MirrorSign.Y;
		EffectiveScale.Z *= MirrorSign.Z;
	}

	SetRelativeScale3D(EffectiveScale);
}

FQuat UVRExpPICOHandTrackingComponent::ApplyComponentRotationAdjustment(const FQuat& RawRotation, const FVRExpPICORotationAdjustment& RotationAdjustment) const
{
	FQuat ResultRotation = RawRotation;

	if (RotationAdjustment.bEnableRotationOffset)
	{
		ResultRotation = (ResultRotation * RotationAdjustment.RotationOffset.Quaternion()).GetNormalized();
	}

	if (RotationAdjustment.bEnableAxisAdjustment)
	{
		FQuat AxisCorrection = FQuat::Identity;
		if (TryBuildAxisCorrection(RotationAdjustment.AxisSettings, AxisCorrection))
		{
			ResultRotation = (ResultRotation * AxisCorrection).GetNormalized();
		}
	}

	return ResultRotation;
}

FQuat UVRExpPICOHandTrackingComponent::ApplyParentBoneRotationOffset(const FQuat& ParentBoneSpaceRotation, const FVRExpPICORotationAdjustment& RotationAdjustment) const
{
	if (!RotationAdjustment.bEnableRotationOffset)
	{
		return ParentBoneSpaceRotation;
	}

	return (RotationAdjustment.RotationOffset.Quaternion() * ParentBoneSpaceRotation).GetNormalized();
}

FQuat UVRExpPICOHandTrackingComponent::ApplyAxisAdjustment(const FQuat& ComponentSpaceRotation, const FVRExpPICORotationAdjustment& RotationAdjustment) const
{
	if (!RotationAdjustment.bEnableAxisAdjustment)
	{
		return ComponentSpaceRotation;
	}

	FQuat AxisCorrection = FQuat::Identity;
	if (!TryBuildAxisCorrection(RotationAdjustment.AxisSettings, AxisCorrection))
	{
		return ComponentSpaceRotation;
	}

	return (ComponentSpaceRotation * AxisCorrection).GetNormalized();
}

EControllerHand UVRExpPICOHandTrackingComponent::ToControllerHand(EVRExpPICOHandType HandType)
{
	return HandType == EVRExpPICOHandType::HandRight ? EControllerHand::Right : EControllerHand::Left;
}

EAxis::Type UVRExpPICOHandTrackingComponent::ToEAxis(EVRExpPICOHandMirrorAxis Axis)
{
	switch (Axis)
	{
	case EVRExpPICOHandMirrorAxis::X:
		return EAxis::X;
	case EVRExpPICOHandMirrorAxis::Y:
		return EAxis::Y;
	case EVRExpPICOHandMirrorAxis::Z:
		return EAxis::Z;
	default:
		return EAxis::Y;
	}
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

FVector UVRExpPICOHandTrackingComponent::GetMirrorAxisSign(EVRExpPICOHandMirrorAxis MirrorAxis)
{
	switch (MirrorAxis)
	{
	case EVRExpPICOHandMirrorAxis::X:
		return FVector(-1.0f, 1.0f, 1.0f);
	case EVRExpPICOHandMirrorAxis::Y:
		return FVector(1.0f, -1.0f, 1.0f);
	case EVRExpPICOHandMirrorAxis::Z:
		return FVector(1.0f, 1.0f, -1.0f);
	default:
		return FVector(1.0f, -1.0f, 1.0f);
	}
}

int32 UVRExpPICOHandTrackingComponent::GetHandKeypointIndex(EHandKeypoint HandKeypoint)
{
	return static_cast<int32>(static_cast<uint8>(HandKeypoint));
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
