// Fill out your copyright notice in the Description page of Project Settings.

#include "HandTracking/VRExpHandPoseRuntimeUtils.h"

bool FVRExpHandPoseRuntimeUtils::SolveComponentSpacePose(
	TConstArrayView<FTransform> WorldTransforms,
	const FTransform& PoseComponentWorldTransform,
	TConstArrayView<FTransform> CurrentComponentTransforms,
	TConstArrayView<FVRExpResolvedHandBoneMapping> ResolvedMappings,
	bool bPreserveInputWristTransform,
	bool bMirrorBonePose,
	EVRExpHandMirrorAxis MirrorAxis,
	EVRExpHandMirrorAxis MirrorFlipAxis,
	TArray<FVRExpSolvedHandBoneTransform>& OutSolvedTransforms)
{
	OutSolvedTransforms.Reset();

	const int32 WristIndex = GetHandKeypointIndex(EHandKeypoint::Wrist);
	if (!WorldTransforms.IsValidIndex(WristIndex) || ResolvedMappings.IsEmpty() || CurrentComponentTransforms.IsEmpty())
	{
		return false;
	}

	FTransform NormalizedPoseComponentTransform = PoseComponentWorldTransform;
	if (!TryNormalizeTransform(NormalizedPoseComponentTransform))
	{
		return false;
	}

	FTransform WristTransform = WorldTransforms[WristIndex];
	if (!TryNormalizeTransform(WristTransform))
	{
		return false;
	}

	const int32 NumBones = CurrentComponentTransforms.Num();
	TArray<FTransform> RawComponentTransforms;
	RawComponentTransforms.SetNum(NumBones);
	TBitArray<> bHasRawComponentTransform(false, NumBones);

	const EAxis::Type PoseMirrorAxis = ToEAxis(MirrorAxis);
	const EAxis::Type PoseMirrorFlipAxis = ToEAxis(MirrorFlipAxis);

	for (const FVRExpResolvedHandBoneMapping& Mapping : ResolvedMappings)
	{
		const int32 KeypointIndex = GetHandKeypointIndex(Mapping.HandKeypoint);
		if (!CurrentComponentTransforms.IsValidIndex(Mapping.BoneIndex) || !WorldTransforms.IsValidIndex(KeypointIndex))
		{
			continue;
		}

		FTransform WorldTransform = WorldTransforms[KeypointIndex];
		if (!TryNormalizeTransform(WorldTransform))
		{
			continue;
		}

		FTransform ComponentTransform = WorldTransform.GetRelativeTransform(NormalizedPoseComponentTransform);
		if (!TryNormalizeTransform(ComponentTransform))
		{
			continue;
		}

		if (bMirrorBonePose)
		{
			ComponentTransform.Mirror(PoseMirrorAxis, PoseMirrorFlipAxis);
			if (!TryNormalizeTransform(ComponentTransform))
			{
				continue;
			}
		}

		RawComponentTransforms[Mapping.BoneIndex] = ComponentTransform;
		bHasRawComponentTransform[Mapping.BoneIndex] = true;
	}

	TArray<FTransform> HierarchyComponentTransforms;
	HierarchyComponentTransforms.SetNum(NumBones);
	TBitArray<> bHasHierarchyComponentTransform(false, NumBones);
	int32 PreservedWristBoneIndex = INDEX_NONE;
	const FVRExpResolvedHandBoneMapping* PreservedWristMapping = nullptr;
	if (bPreserveInputWristTransform)
	{
		for (const FVRExpResolvedHandBoneMapping& Mapping : ResolvedMappings)
		{
			if (Mapping.HandKeypoint != EHandKeypoint::Wrist)
			{
				continue;
			}

			if (!CurrentComponentTransforms.IsValidIndex(Mapping.BoneIndex))
			{
				return false;
			}

			FTransform InputWristTransform = CurrentComponentTransforms[Mapping.BoneIndex];
			if (!TryNormalizeTransform(InputWristTransform))
			{
				return false;
			}

			PreservedWristBoneIndex = Mapping.BoneIndex;
			PreservedWristMapping = &Mapping;
			HierarchyComponentTransforms[PreservedWristBoneIndex] = InputWristTransform;
			bHasHierarchyComponentTransform[PreservedWristBoneIndex] = true;
			break;
		}

		if (PreservedWristBoneIndex == INDEX_NONE || PreservedWristMapping == nullptr)
		{
			return false;
		}

		if (!RawComponentTransforms.IsValidIndex(PreservedWristBoneIndex)
			|| !bHasRawComponentTransform.IsValidIndex(PreservedWristBoneIndex)
			|| !bHasRawComponentTransform[PreservedWristBoneIndex])
		{
			return false;
		}

		FTransform TrackedWristParentComponentTransform = FTransform::Identity;
		if (PreservedWristMapping->ParentIndex != INDEX_NONE)
		{
			if (RawComponentTransforms.IsValidIndex(PreservedWristMapping->ParentIndex)
				&& bHasRawComponentTransform.IsValidIndex(PreservedWristMapping->ParentIndex)
				&& bHasRawComponentTransform[PreservedWristMapping->ParentIndex])
			{
				TrackedWristParentComponentTransform = RawComponentTransforms[PreservedWristMapping->ParentIndex];
			}
			else if (CurrentComponentTransforms.IsValidIndex(PreservedWristMapping->ParentIndex))
			{
				TrackedWristParentComponentTransform = CurrentComponentTransforms[PreservedWristMapping->ParentIndex];
			}
		}

		FTransform AdjustedTrackedWristParentSpaceTransform =
			RawComponentTransforms[PreservedWristBoneIndex].GetRelativeTransform(TrackedWristParentComponentTransform);
		if (!TryNormalizeTransform(AdjustedTrackedWristParentSpaceTransform))
		{
			return false;
		}
		AdjustedTrackedWristParentSpaceTransform.SetRotation(ApplyParentBoneRotationOffset(
			AdjustedTrackedWristParentSpaceTransform.GetRotation(),
			PreservedWristMapping->RotationAdjustment));

		FTransform AdjustedTrackedWristComponentTransform =
			AdjustedTrackedWristParentSpaceTransform * TrackedWristParentComponentTransform;
		if (!TryNormalizeTransform(AdjustedTrackedWristComponentTransform))
		{
			return false;
		}
		AdjustedTrackedWristComponentTransform.SetRotation(ApplyAxisAdjustment(
			AdjustedTrackedWristComponentTransform.GetRotation(),
			PreservedWristMapping->RotationAdjustment));

		for (const FVRExpResolvedHandBoneMapping& Mapping : ResolvedMappings)
		{
			if (!RawComponentTransforms.IsValidIndex(Mapping.BoneIndex)
				|| !bHasRawComponentTransform.IsValidIndex(Mapping.BoneIndex)
				|| !bHasRawComponentTransform[Mapping.BoneIndex])
			{
				continue;
			}

			// The Wrist mapping still defines the tracking basis when its target bone is
			// preserved. Rebase through the fully adjusted tracked Wrist before the target
			// hierarchy is evaluated, including unmapped intermediary skeleton bones.
			FTransform WristRelativeTransform = RawComponentTransforms[Mapping.BoneIndex].GetRelativeTransform(
				AdjustedTrackedWristComponentTransform);
			FTransform RebasedComponentTransform = WristRelativeTransform * HierarchyComponentTransforms[PreservedWristBoneIndex];
			if (!TryNormalizeTransform(RebasedComponentTransform))
			{
				bHasRawComponentTransform[Mapping.BoneIndex] = false;
				continue;
			}

			RawComponentTransforms[Mapping.BoneIndex] = RebasedComponentTransform;
		}

		// The Wrist itself remains exactly on the input pose. It participates as the
		// corrected hierarchy basis above but is intentionally never emitted.
		RawComponentTransforms[PreservedWristBoneIndex] = HierarchyComponentTransforms[PreservedWristBoneIndex];
	}

	auto GetParentComponentTransform = [&CurrentComponentTransforms](
		int32 ParentIndex,
		const TArray<FTransform>& CandidateTransforms,
		const TBitArray<>& bHasCandidateTransform)
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

	OutSolvedTransforms.Reserve(ResolvedMappings.Num());
	for (const FVRExpResolvedHandBoneMapping& Mapping : ResolvedMappings)
	{
		if (!RawComponentTransforms.IsValidIndex(Mapping.BoneIndex) || !bHasRawComponentTransform[Mapping.BoneIndex])
		{
			continue;
		}

		if (bPreserveInputWristTransform
			&& Mapping.HandKeypoint == EHandKeypoint::Wrist
			&& Mapping.BoneIndex == PreservedWristBoneIndex)
		{
			continue;
		}

		const FTransform& RawComponentTransform = RawComponentTransforms[Mapping.BoneIndex];
		const FTransform RawParentComponentTransform = GetParentComponentTransform(
			Mapping.ParentIndex,
			RawComponentTransforms,
			bHasRawComponentTransform);

		FTransform ParentBoneSpaceTransform = RawComponentTransform.GetRelativeTransform(RawParentComponentTransform);
		if (!TryNormalizeTransform(ParentBoneSpaceTransform))
		{
			continue;
		}
		ParentBoneSpaceTransform.SetRotation(ApplyParentBoneRotationOffset(
			ParentBoneSpaceTransform.GetRotation(),
			Mapping.RotationAdjustment));

		const FTransform AdjustedParentComponentTransform = GetParentComponentTransform(
			Mapping.ParentIndex,
			HierarchyComponentTransforms,
			bHasHierarchyComponentTransform);
		FTransform HierarchyComponentTransform = ParentBoneSpaceTransform * AdjustedParentComponentTransform;
		if (!TryNormalizeTransform(HierarchyComponentTransform))
		{
			continue;
		}

		HierarchyComponentTransforms[Mapping.BoneIndex] = HierarchyComponentTransform;
		bHasHierarchyComponentTransform[Mapping.BoneIndex] = true;

		FVRExpSolvedHandBoneTransform& SolvedTransform = OutSolvedTransforms.AddDefaulted_GetRef();
		SolvedTransform.HandKeypoint = Mapping.HandKeypoint;
		SolvedTransform.BoneIndex = Mapping.BoneIndex;
		SolvedTransform.ParentIndex = Mapping.ParentIndex;
		SolvedTransform.ComponentTransform = HierarchyComponentTransform;
		SolvedTransform.ComponentTransform.SetRotation(ApplyAxisAdjustment(
			SolvedTransform.ComponentTransform.GetRotation(),
			Mapping.RotationAdjustment));
	}

	return !OutSolvedTransforms.IsEmpty();
}

bool FVRExpHandPoseRuntimeUtils::ShouldMirror(
	EVRExpHandType HandType,
	bool bEnableMirror,
	bool bAutoMirrorByHandType,
	EVRExpHandType SourceMeshHandType)
{
	if (!bEnableMirror)
	{
		return false;
	}

	return bAutoMirrorByHandType ? SourceMeshHandType != HandType : true;
}

FQuat FVRExpHandPoseRuntimeUtils::ApplyComponentRotationAdjustment(
	const FQuat& RawRotation,
	const FVRExpHandRotationAdjustment& RotationAdjustment)
{
	FQuat ResultRotation = RawRotation.GetNormalized();

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

FQuat FVRExpHandPoseRuntimeUtils::ApplyParentBoneRotationOffset(
	const FQuat& ParentBoneSpaceRotation,
	const FVRExpHandRotationAdjustment& RotationAdjustment)
{
	if (!RotationAdjustment.bEnableRotationOffset)
	{
		return ParentBoneSpaceRotation.GetNormalized();
	}

	return (RotationAdjustment.RotationOffset.Quaternion() * ParentBoneSpaceRotation).GetNormalized();
}

FQuat FVRExpHandPoseRuntimeUtils::ApplyAxisAdjustment(
	const FQuat& ComponentSpaceRotation,
	const FVRExpHandRotationAdjustment& RotationAdjustment)
{
	if (!RotationAdjustment.bEnableAxisAdjustment)
	{
		return ComponentSpaceRotation.GetNormalized();
	}

	FQuat AxisCorrection = FQuat::Identity;
	if (!TryBuildAxisCorrection(RotationAdjustment.AxisSettings, AxisCorrection))
	{
		return ComponentSpaceRotation.GetNormalized();
	}

	return (ComponentSpaceRotation * AxisCorrection).GetNormalized();
}

EAxis::Type FVRExpHandPoseRuntimeUtils::ToEAxis(EVRExpHandMirrorAxis Axis)
{
	switch (Axis)
	{
	case EVRExpHandMirrorAxis::X:
		return EAxis::X;
	case EVRExpHandMirrorAxis::Y:
		return EAxis::Y;
	case EVRExpHandMirrorAxis::Z:
		return EAxis::Z;
	default:
		return EAxis::Y;
	}
}

FVector FVRExpHandPoseRuntimeUtils::GetAxisVector(EVRExpHandBoneAxis Axis)
{
	switch (Axis)
	{
	case EVRExpHandBoneAxis::X:
		return FVector::ForwardVector;
	case EVRExpHandBoneAxis::Y:
		return FVector::RightVector;
	case EVRExpHandBoneAxis::Z:
		return FVector::UpVector;
	case EVRExpHandBoneAxis::NegativeX:
		return -FVector::ForwardVector;
	case EVRExpHandBoneAxis::NegativeY:
		return -FVector::RightVector;
	case EVRExpHandBoneAxis::NegativeZ:
		return -FVector::UpVector;
	default:
		return FVector::ForwardVector;
	}
}

FVector FVRExpHandPoseRuntimeUtils::GetMirrorAxisSign(EVRExpHandMirrorAxis MirrorAxis)
{
	switch (MirrorAxis)
	{
	case EVRExpHandMirrorAxis::X:
		return FVector(-1.0f, 1.0f, 1.0f);
	case EVRExpHandMirrorAxis::Y:
		return FVector(1.0f, -1.0f, 1.0f);
	case EVRExpHandMirrorAxis::Z:
		return FVector(1.0f, 1.0f, -1.0f);
	default:
		return FVector(1.0f, -1.0f, 1.0f);
	}
}

int32 FVRExpHandPoseRuntimeUtils::GetHandKeypointIndex(EHandKeypoint HandKeypoint)
{
	return static_cast<int32>(static_cast<uint8>(HandKeypoint));
}

bool FVRExpHandPoseRuntimeUtils::TryBuildAxisCorrection(
	const FVRExpHandBoneAxisSettings& AxisSettings,
	FQuat& OutAxisCorrection)
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

bool FVRExpHandPoseRuntimeUtils::TryNormalizeTransform(FTransform& Transform)
{
	if (Transform.ContainsNaN() || Transform.GetRotation().SizeSquared() <= UE_SMALL_NUMBER)
	{
		return false;
	}

	Transform.NormalizeRotation();
	return !Transform.ContainsNaN();
}
