// Fill out your copyright notice in the Description page of Project Settings.

#include "HandTracking/VRExpHandPoseAnimNodeRuntime.h"

#include "HandTracking/VRExpHandPoseRuntimeUtils.h"

void FVRExpHandPoseAnimNodeRuntime::InitializeBoneReferences(
	const FBoneContainer& RequiredBones,
	const TMap<EHandKeypoint, FVRExpHandBoneMapping>& BoneMappings)
{
	bBoneMappingsCacheInitialized = false;
	RebuildCachedBoneMappings(RequiredBones, BoneMappings);
}

bool FVRExpHandPoseAnimNodeRuntime::IsValidToEvaluate(
	const FBoneContainer& RequiredBones,
	const TMap<EHandKeypoint, FVRExpHandBoneMapping>& BoneMappings)
{
	const uint32 ActiveSignature = CalculateBoneMappingsSignature(BoneMappings);
	if (!bBoneMappingsCacheInitialized || ActiveSignature != CachedBoneMappingsSignature)
	{
		RebuildCachedBoneMappings(RequiredBones, BoneMappings);
	}

	for (const FCachedBoneMapping& Mapping : CachedBoneMappings)
	{
		if (Mapping.BoneReference.IsValidToEvaluate(RequiredBones))
		{
			return true;
		}
	}

	return false;
}

void FVRExpHandPoseAnimNodeRuntime::EvaluateSkeletalControl(
	FComponentSpacePoseContext& Output,
	TArray<FBoneTransform>& OutBoneTransforms,
	TConstArrayView<FTransform> WorldTransforms,
	const FTransform& MeshWorldTransform,
	EVRExpHandType HandType,
	EVRExpHandPoseTransformApplicationMode TransformApplicationMode,
	bool bApplyWristBone,
	const FVRExpHandRotationAdjustment& ComponentRotationAdjustment,
	const FVRExpHandMirrorSettings& MirrorSettings) const
{
	if (WorldTransforms.IsEmpty() || CachedBoneMappings.IsEmpty())
	{
		return;
	}

	const FBoneContainer& RequiredBones = Output.Pose.GetPose().GetBoneContainer();
	const int32 NumBones = Output.Pose.GetPose().GetNumBones();
	if (NumBones <= 0)
	{
		return;
	}

	TArray<FTransform> CurrentComponentTransforms;
	CurrentComponentTransforms.SetNum(NumBones);
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		CurrentComponentTransforms[BoneIndex] = Output.Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(BoneIndex));
	}

	TArray<FVRExpResolvedHandBoneMapping> ResolvedMappings;
	ResolvedMappings.Reserve(CachedBoneMappings.Num());
	for (const FCachedBoneMapping& CachedMapping : CachedBoneMappings)
	{
		if (!CachedMapping.BoneReference.IsValidToEvaluate(RequiredBones))
		{
			continue;
		}

		FVRExpResolvedHandBoneMapping& ResolvedMapping = ResolvedMappings.AddDefaulted_GetRef();
		ResolvedMapping.HandKeypoint = CachedMapping.HandKeypoint;
		ResolvedMapping.BoneName = CachedMapping.BoneReference.BoneName;
		ResolvedMapping.BoneIndex = CachedMapping.BoneReference.GetCompactPoseIndex(RequiredBones).GetInt();
		ResolvedMapping.ParentIndex = CachedMapping.ParentIndex.GetInt();
		ResolvedMapping.RotationAdjustment = CachedMapping.RotationAdjustment;
	}

	const bool bMirrorPose = FVRExpHandPoseRuntimeUtils::ShouldMirror(
		HandType,
		MirrorSettings.bEnableMirror,
		MirrorSettings.bAutoMirrorByHandType,
		MirrorSettings.SourceMeshHandType)
		&& MirrorSettings.bMirrorBonePose;
	const bool bApplyTrackedWristTransform = bApplyWristBone
		&& (!bMirrorPose || MirrorSettings.bApplyWristBoneTransformWhenMirrored);
	FTransform AdjustedMeshWorldTransform = MeshWorldTransform;
	AdjustedMeshWorldTransform.SetRotation(FVRExpHandPoseRuntimeUtils::ApplyComponentRotationAdjustment(
		MeshWorldTransform.GetRotation(),
		ComponentRotationAdjustment));
	AdjustedMeshWorldTransform.NormalizeRotation();

	TArray<FVRExpSolvedHandBoneTransform> SolvedTransforms;
	if (!FVRExpHandPoseRuntimeUtils::SolveComponentSpacePose(
		WorldTransforms,
		AdjustedMeshWorldTransform,
		CurrentComponentTransforms,
		ResolvedMappings,
		!bApplyTrackedWristTransform,
		bMirrorPose,
		MirrorSettings.MirrorAxis,
		MirrorSettings.PoseMirrorFlipAxis,
		SolvedTransforms))
	{
		return;
	}

	TArray<int32> SolvedTransformByBone;
	SolvedTransformByBone.Init(INDEX_NONE, NumBones);
	for (int32 SolvedIndex = 0; SolvedIndex < SolvedTransforms.Num(); ++SolvedIndex)
	{
		if (SolvedTransformByBone.IsValidIndex(SolvedTransforms[SolvedIndex].BoneIndex))
		{
			SolvedTransformByBone[SolvedTransforms[SolvedIndex].BoneIndex] = SolvedIndex;
		}
	}

	TArray<FTransform> TargetComponentTransforms;
	TargetComponentTransforms.SetNum(NumBones);
	OutBoneTransforms.Reserve(SolvedTransforms.Num());

	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FCompactPoseBoneIndex CompactBoneIndex(BoneIndex);
		const FCompactPoseBoneIndex ParentIndex = RequiredBones.GetParentBoneIndex(CompactBoneIndex);
		if (ParentIndex != INDEX_NONE)
		{
			const FTransform CurrentLocalTransform = CurrentComponentTransforms[BoneIndex].GetRelativeTransform(CurrentComponentTransforms[ParentIndex.GetInt()]);
			TargetComponentTransforms[BoneIndex] = CurrentLocalTransform * TargetComponentTransforms[ParentIndex.GetInt()];
		}
		else
		{
			TargetComponentTransforms[BoneIndex] = CurrentComponentTransforms[BoneIndex];
		}

		const int32 SolvedIndex = SolvedTransformByBone[BoneIndex];
		if (SolvedIndex == INDEX_NONE)
		{
			continue;
		}

		const FVRExpSolvedHandBoneTransform& SolvedTransform = SolvedTransforms[SolvedIndex];
		const bool bIsWrist = SolvedTransform.HandKeypoint == EHandKeypoint::Wrist;
		if (bIsWrist && !bApplyTrackedWristTransform)
		{
			continue;
		}

		FTransform& TargetTransform = TargetComponentTransforms[BoneIndex];
		TargetTransform.SetRotation(SolvedTransform.ComponentTransform.GetRotation());
		if (ShouldApplyTrackedLocation(TransformApplicationMode, bIsWrist))
		{
			TargetTransform.SetLocation(SolvedTransform.ComponentTransform.GetLocation());
		}
		TargetTransform.NormalizeRotation();
		OutBoneTransforms.Emplace(CompactBoneIndex, TargetTransform);
	}
}

bool FVRExpHandPoseAnimNodeRuntime::ShouldApplyTrackedLocation(
	EVRExpHandPoseTransformApplicationMode TransformApplicationMode,
	bool bIsWrist)
{
	switch (TransformApplicationMode)
	{
	case EVRExpHandPoseTransformApplicationMode::RotationAndWristLocation:
		return bIsWrist;
	case EVRExpHandPoseTransformApplicationMode::RotationAndAllLocations:
		return true;
	case EVRExpHandPoseTransformApplicationMode::RotationOnly:
	default:
		return false;
	}
}

uint32 FVRExpHandPoseAnimNodeRuntime::CalculateBoneMappingsSignature(
	const TMap<EHandKeypoint, FVRExpHandBoneMapping>& BoneMappings)
{
	uint32 Signature = 0;
	for (int32 KeypointIndex = 0; KeypointIndex < EHandKeypointCount; ++KeypointIndex)
	{
		const EHandKeypoint Keypoint = static_cast<EHandKeypoint>(KeypointIndex);
		Signature = HashCombineFast(Signature, GetTypeHash(KeypointIndex));
		const FVRExpHandBoneMapping* Mapping = BoneMappings.Find(Keypoint);
		if (Mapping == nullptr)
		{
			continue;
		}

		Signature = HashCombineFast(Signature, GetTypeHash(Mapping->BoneName));
		const FVRExpHandRotationAdjustment& Adjustment = Mapping->RotationAdjustment;
		Signature = HashCombineFast(Signature, GetTypeHash(Adjustment.bEnableRotationOffset));
		Signature = HashCombineFast(Signature, GetTypeHash(Adjustment.RotationOffset.Pitch));
		Signature = HashCombineFast(Signature, GetTypeHash(Adjustment.RotationOffset.Yaw));
		Signature = HashCombineFast(Signature, GetTypeHash(Adjustment.RotationOffset.Roll));
		Signature = HashCombineFast(Signature, GetTypeHash(Adjustment.bEnableAxisAdjustment));
		Signature = HashCombineFast(Signature, GetTypeHash(static_cast<uint8>(Adjustment.AxisSettings.ForwardAxis)));
		Signature = HashCombineFast(Signature, GetTypeHash(static_cast<uint8>(Adjustment.AxisSettings.UpAxis)));
	}
	return Signature;
}

void FVRExpHandPoseAnimNodeRuntime::RebuildCachedBoneMappings(
	const FBoneContainer& RequiredBones,
	const TMap<EHandKeypoint, FVRExpHandBoneMapping>& BoneMappings)
{
	CachedBoneMappings.Reset();

	for (const TPair<EHandKeypoint, FVRExpHandBoneMapping>& MappingPair : BoneMappings)
	{
		if (MappingPair.Value.BoneName.IsNone())
		{
			continue;
		}

		FCachedBoneMapping CachedMapping;
		CachedMapping.HandKeypoint = MappingPair.Key;
		CachedMapping.BoneReference.BoneName = MappingPair.Value.BoneName;
		CachedMapping.RotationAdjustment = MappingPair.Value.RotationAdjustment;
		if (!CachedMapping.BoneReference.Initialize(RequiredBones))
		{
			continue;
		}

		const FCompactPoseBoneIndex CompactPoseIndex = CachedMapping.BoneReference.GetCompactPoseIndex(RequiredBones);
		CachedMapping.ParentIndex = RequiredBones.GetParentBoneIndex(CompactPoseIndex);
		CachedBoneMappings.Add(MoveTemp(CachedMapping));
	}

	CachedBoneMappings.Sort([&RequiredBones](const FCachedBoneMapping& A, const FCachedBoneMapping& B)
	{
		const int32 AIndex = A.BoneReference.GetCompactPoseIndex(RequiredBones).GetInt();
		const int32 BIndex = B.BoneReference.GetCompactPoseIndex(RequiredBones).GetInt();
		return AIndex == BIndex
			? static_cast<uint8>(A.HandKeypoint) < static_cast<uint8>(B.HandKeypoint)
			: AIndex < BIndex;
	});

	for (int32 MappingIndex = CachedBoneMappings.Num() - 1; MappingIndex > 0; --MappingIndex)
	{
		const int32 BoneIndex = CachedBoneMappings[MappingIndex].BoneReference.GetCompactPoseIndex(RequiredBones).GetInt();
		const int32 PreviousBoneIndex = CachedBoneMappings[MappingIndex - 1].BoneReference.GetCompactPoseIndex(RequiredBones).GetInt();
		if (BoneIndex == PreviousBoneIndex)
		{
			CachedBoneMappings.RemoveAt(MappingIndex);
		}
	}

	CachedBoneMappingsSignature = CalculateBoneMappingsSignature(BoneMappings);
	bBoneMappingsCacheInitialized = true;
}
