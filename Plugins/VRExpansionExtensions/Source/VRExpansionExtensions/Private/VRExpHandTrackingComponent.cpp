// Fill out your copyright notice in the Description page of Project Settings.

#include "VRExpHandTrackingComponent.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAsset.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "UObject/UnrealType.h"

UVRExpHandTrackingComponent::UVRExpHandTrackingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SkeletonMeshType(EVRExpHandType::HandLeft)
	, ApplyLocationToEveryBone(false)
	, AutoHide(false)
	, AutoScaleComponent(false)
	, bEnableGestureRecognition(true)
	, PinchClosedDistanceScale(0.25f)
	, PinchOpenDistanceScale(0.85f)
	, FingerClosedAngleDegrees(95.0f)
	, FingerOpenAngleDegrees(25.0f)
	, GestureActiveThreshold(0.75f)
	, GestureInactiveThreshold(0.35f)
	, GestureAxisBroadcastDelta(0.02f)
	, bPinchGestureActive(false)
	, bFistGestureActive(false)
	, bOpenPalmGestureActive(false)
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

void UVRExpHandTrackingComponent::OnRegister()
{
	Super::OnRegister();

	ApplyEffectiveMeshScale(1.0f);
}

#if WITH_EDITOR
void UVRExpHandTrackingComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplyEffectiveMeshScale(1.0f);
}
#endif

float UVRExpHandTrackingComponent::GetGestureAxis(EVRExpHandGesture Gesture) const
{
	return GetGestureAxisFromValues(CurrentGestureAxisValues, Gesture);
}

FVRExpHandGestureAxisValues UVRExpHandTrackingComponent::GetGestureAxisValues() const
{
	return CurrentGestureAxisValues;
}

void UVRExpHandTrackingComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AutoHide)
	{
		SetHiddenInGame(true, true);
	}
}

void UVRExpHandTrackingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UVRExpHandTrackingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool bHidden = true;
	const EControllerHand ControllerHand = ToControllerHand(SkeletonMeshType);

	FXRMotionControllerData Data;
	UHeadMountedDisplayFunctionLibrary::GetMotionControllerData(nullptr, ControllerHand, Data);
	const bool bHasValidHandData = Data.bValid && Data.DeviceVisualType == EXRVisualType::Hand;

	if (bEnableGestureRecognition && bHasValidHandData)
	{
		EvaluateRawHandGestures(Data.HandKeyPositions, Data.HandKeyRotations);
	}
	else
	{
		ResetGestureState();
	}

	if (GetSkinnedAsset())
	{
		ApplyEffectiveMeshScale(1.0f);

		if (bHasValidHandData)
		{
			bHidden = !ApplyTrackedHandPose(Data.HandKeyPositions, Data.HandKeyRotations);
		}
	}

	if (AutoHide && bHidden != bHiddenInGame)
	{
		SetHiddenInGame(bHidden, true);
	}
}

void UVRExpHandTrackingComponent::EvaluateRawHandGestures(const TArray<FVector>& WorldPositions, const TArray<FQuat>& WorldRotations)
{
	(void)WorldRotations;

	if (!bEnableGestureRecognition)
	{
		ResetGestureState();
		return;
	}

	TArray<FVector> GestureLocalPositions;
	float PalmWidth = 0.0f;
	if (!BuildGestureLocalPositions(WorldPositions, GestureLocalPositions, PalmWidth))
	{
		ResetGestureState();
		return;
	}

	ApplyGestureAxisValues(CalculateGestureAxisValues(GestureLocalPositions, PalmWidth));
}

void UVRExpHandTrackingComponent::ResetGestureState()
{
	ApplyGestureAxisValues(FVRExpHandGestureAxisValues());
}

bool UVRExpHandTrackingComponent::BuildGestureLocalPositions(const TArray<FVector>& WorldPositions, TArray<FVector>& OutLocalPositions, float& OutPalmWidth) const
{
	OutLocalPositions.Reset();
	OutPalmWidth = 0.0f;

	FVector WristPosition;
	FVector IndexProximalPosition;
	FVector MiddleProximalPosition;
	FVector RingProximalPosition;
	FVector LittleProximalPosition;
	if (!TryGetKeypointPosition(WorldPositions, EHandKeypoint::Wrist, WristPosition)
		|| !TryGetKeypointPosition(WorldPositions, EHandKeypoint::IndexProximal, IndexProximalPosition)
		|| !TryGetKeypointPosition(WorldPositions, EHandKeypoint::MiddleProximal, MiddleProximalPosition)
		|| !TryGetKeypointPosition(WorldPositions, EHandKeypoint::RingProximal, RingProximalPosition)
		|| !TryGetKeypointPosition(WorldPositions, EHandKeypoint::LittleProximal, LittleProximalPosition))
	{
		return false;
	}

	const FVector FingerBaseCenter = (IndexProximalPosition + MiddleProximalPosition + RingProximalPosition + LittleProximalPosition) * 0.25f;
	const FVector ForwardAxis = (FingerBaseCenter - WristPosition).GetSafeNormal();
	const FVector AcrossPalmAxis = (LittleProximalPosition - IndexProximalPosition).GetSafeNormal();
	if (ForwardAxis.IsNearlyZero() || AcrossPalmAxis.IsNearlyZero())
	{
		return false;
	}

	const FVector UpAxis = FVector::CrossProduct(ForwardAxis, AcrossPalmAxis).GetSafeNormal();
	if (UpAxis.IsNearlyZero())
	{
		return false;
	}

	const FVector RightAxis = FVector::CrossProduct(UpAxis, ForwardAxis).GetSafeNormal();
	if (RightAxis.IsNearlyZero())
	{
		return false;
	}

	FVector IndexMetacarpalPosition;
	FVector LittleMetacarpalPosition;
	if (TryGetKeypointPosition(WorldPositions, EHandKeypoint::IndexMetacarpal, IndexMetacarpalPosition)
		&& TryGetKeypointPosition(WorldPositions, EHandKeypoint::LittleMetacarpal, LittleMetacarpalPosition))
	{
		OutPalmWidth = FVector::Distance(IndexMetacarpalPosition, LittleMetacarpalPosition);
	}

	if (OutPalmWidth <= KINDA_SMALL_NUMBER)
	{
		OutPalmWidth = FVector::Distance(IndexProximalPosition, LittleProximalPosition);
	}

	if (OutPalmWidth <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutLocalPositions.SetNum(WorldPositions.Num());
	for (int32 PositionIndex = 0; PositionIndex < WorldPositions.Num(); ++PositionIndex)
	{
		const FVector WristRelativePosition = WorldPositions[PositionIndex] - WristPosition;
		OutLocalPositions[PositionIndex] = FVector(
			FVector::DotProduct(WristRelativePosition, ForwardAxis),
			FVector::DotProduct(WristRelativePosition, RightAxis),
			FVector::DotProduct(WristRelativePosition, UpAxis));
	}

	return true;
}

FVRExpHandGestureAxisValues UVRExpHandTrackingComponent::CalculateGestureAxisValues(const TArray<FVector>& GestureLocalPositions, float PalmWidth) const
{
	FVRExpHandGestureAxisValues AxisValues;
	AxisValues.PinchAxis = CalculatePinchAxis(GestureLocalPositions, PalmWidth);
	AxisValues.FistAxis = CalculateFistAxis(GestureLocalPositions);
	AxisValues.OpenPalmAxis = 1.0f - AxisValues.FistAxis;
	AxisValues.bGestureDataValid = true;
	return AxisValues;
}

float UVRExpHandTrackingComponent::CalculatePinchAxis(const TArray<FVector>& GestureLocalPositions, float PalmWidth) const
{
	FVector ThumbTipPosition;
	FVector IndexTipPosition;
	if (PalmWidth <= KINDA_SMALL_NUMBER
		|| !TryGetKeypointPosition(GestureLocalPositions, EHandKeypoint::ThumbTip, ThumbTipPosition)
		|| !TryGetKeypointPosition(GestureLocalPositions, EHandKeypoint::IndexTip, IndexTipPosition))
	{
		return 0.0f;
	}

	const float ClosedDistance = FMath::Max(0.0f, PinchClosedDistanceScale) * PalmWidth;
	const float OpenDistance = FMath::Max(ClosedDistance + KINDA_SMALL_NUMBER, FMath::Max(0.0f, PinchOpenDistanceScale) * PalmWidth);
	const float TipDistance = FVector::Distance(ThumbTipPosition, IndexTipPosition);
	const float DistanceAlpha = (TipDistance - ClosedDistance) / (OpenDistance - ClosedDistance);
	return FMath::Clamp(1.0f - DistanceAlpha, 0.0f, 1.0f);
}

float UVRExpHandTrackingComponent::CalculateFistAxis(const TArray<FVector>& GestureLocalPositions) const
{
	const FGestureFingerKeypoints Fingers[] =
	{
		{ EHandKeypoint::IndexMetacarpal, EHandKeypoint::IndexProximal, EHandKeypoint::IndexTip },
		{ EHandKeypoint::MiddleMetacarpal, EHandKeypoint::MiddleProximal, EHandKeypoint::MiddleTip },
		{ EHandKeypoint::RingMetacarpal, EHandKeypoint::RingProximal, EHandKeypoint::RingTip },
		{ EHandKeypoint::LittleMetacarpal, EHandKeypoint::LittleProximal, EHandKeypoint::LittleTip },
	};

	float TotalCurlAxis = 0.0f;
	for (const FGestureFingerKeypoints& Finger : Fingers)
	{
		TotalCurlAxis += CalculateFingerCurlAxis(GestureLocalPositions, Finger);
	}

	return FMath::Clamp(TotalCurlAxis / UE_ARRAY_COUNT(Fingers), 0.0f, 1.0f);
}

float UVRExpHandTrackingComponent::CalculateFingerCurlAxis(const TArray<FVector>& GestureLocalPositions, const FGestureFingerKeypoints& FingerKeypoints) const
{
	FVector ProximalPosition;
	FVector TipPosition;
	if (!TryGetKeypointPosition(GestureLocalPositions, FingerKeypoints.Proximal, ProximalPosition)
		|| !TryGetKeypointPosition(GestureLocalPositions, FingerKeypoints.Tip, TipPosition))
	{
		return 0.0f;
	}

	const FVector FingerDirection = (TipPosition - ProximalPosition).GetSafeNormal();
	if (FingerDirection.IsNearlyZero())
	{
		return 0.0f;
	}

	const float DotForward = FMath::Clamp(FVector::DotProduct(FingerDirection, FVector::ForwardVector), -1.0f, 1.0f);
	const float FingerAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(DotForward));
	const float OpenAngle = FMath::Clamp(FingerOpenAngleDegrees, 0.0f, 180.0f);
	const float ClosedAngle = FMath::Clamp(FingerClosedAngleDegrees, 0.0f, 180.0f);
	if (ClosedAngle <= OpenAngle + KINDA_SMALL_NUMBER)
	{
		return FingerAngleDegrees >= ClosedAngle ? 1.0f : 0.0f;
	}

	const float AngleAlpha = (FingerAngleDegrees - OpenAngle) / (ClosedAngle - OpenAngle);
	return FMath::Clamp(AngleAlpha, 0.0f, 1.0f);
}

void UVRExpHandTrackingComponent::ApplyGestureAxisValues(const FVRExpHandGestureAxisValues& NewAxisValues)
{
	const FVRExpHandGestureAxisValues OldAxisValues = CurrentGestureAxisValues;

	UpdateGestureState(EVRExpHandGesture::Pinch, NewAxisValues.PinchAxis);
	UpdateGestureState(EVRExpHandGesture::Fist, NewAxisValues.FistAxis);
	UpdateGestureState(EVRExpHandGesture::OpenPalm, NewAxisValues.OpenPalmAxis);

	const float AxisBroadcastDelta = FMath::Max(0.0f, GestureAxisBroadcastDelta);
	if (FMath::Abs(OldAxisValues.PinchAxis - NewAxisValues.PinchAxis) >= AxisBroadcastDelta || OldAxisValues.bGestureDataValid != NewAxisValues.bGestureDataValid)
	{
		OnGestureAxisChanged.Broadcast(EVRExpHandGesture::Pinch, NewAxisValues.PinchAxis);
	}

	if (FMath::Abs(OldAxisValues.FistAxis - NewAxisValues.FistAxis) >= AxisBroadcastDelta || OldAxisValues.bGestureDataValid != NewAxisValues.bGestureDataValid)
	{
		OnGestureAxisChanged.Broadcast(EVRExpHandGesture::Fist, NewAxisValues.FistAxis);
	}

	if (FMath::Abs(OldAxisValues.OpenPalmAxis - NewAxisValues.OpenPalmAxis) >= AxisBroadcastDelta || OldAxisValues.bGestureDataValid != NewAxisValues.bGestureDataValid)
	{
		OnGestureAxisChanged.Broadcast(EVRExpHandGesture::OpenPalm, NewAxisValues.OpenPalmAxis);
	}

	CurrentGestureAxisValues = NewAxisValues;
}

void UVRExpHandTrackingComponent::UpdateGestureState(EVRExpHandGesture Gesture, float NewAxisValue)
{
	const bool bWasActive = IsGestureActive(Gesture);
	const float ActiveThreshold = FMath::Clamp(GestureActiveThreshold, 0.0f, 1.0f);
	const float InactiveThreshold = FMath::Min(FMath::Clamp(GestureInactiveThreshold, 0.0f, 1.0f), ActiveThreshold);
	const bool bShouldBeActive = bWasActive
		? NewAxisValue > InactiveThreshold
		: NewAxisValue >= ActiveThreshold;

	if (bWasActive == bShouldBeActive)
	{
		return;
	}

	SetGestureActive(Gesture, bShouldBeActive);
	if (bShouldBeActive)
	{
		OnGestureStarted.Broadcast(Gesture, NewAxisValue);
	}
	else
	{
		OnGestureEnded.Broadcast(Gesture, NewAxisValue);
	}
}

bool UVRExpHandTrackingComponent::IsGestureActive(EVRExpHandGesture Gesture) const
{
	switch (Gesture)
	{
	case EVRExpHandGesture::Pinch:
		return bPinchGestureActive;
	case EVRExpHandGesture::Fist:
		return bFistGestureActive;
	case EVRExpHandGesture::OpenPalm:
		return bOpenPalmGestureActive;
	default:
		return false;
	}
}

void UVRExpHandTrackingComponent::SetGestureActive(EVRExpHandGesture Gesture, bool bActive)
{
	switch (Gesture)
	{
	case EVRExpHandGesture::Pinch:
		bPinchGestureActive = bActive;
		break;
	case EVRExpHandGesture::Fist:
		bFistGestureActive = bActive;
		break;
	case EVRExpHandGesture::OpenPalm:
		bOpenPalmGestureActive = bActive;
		break;
	default:
		break;
	}
}

bool UVRExpHandTrackingComponent::ApplyTrackedHandPose(const TArray<FVector>& WorldPositions, const TArray<FQuat>& WorldRotations)
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

void UVRExpHandTrackingComponent::BuildResolvedBoneMappings(int32 NumKeypoints, TArray<FResolvedBoneMapping>& OutMappings) const
{
	OutMappings.Reset();

	if (!GetSkinnedAsset())
	{
		return;
	}

	const FReferenceSkeleton& RefSkeleton = GetSkinnedAsset()->GetRefSkeleton();
	for (const TPair<EHandKeypoint, FVRExpHandBoneMapping>& MappingPair : BoneMappings)
	{
		const FVRExpHandBoneMapping& BoneMapping = MappingPair.Value;
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

bool UVRExpHandTrackingComponent::ShouldMirrorForCurrentHand() const
{
	if (!MirrorSettings.bEnableMirror)
	{
		return false;
	}

	return MirrorSettings.bAutoMirrorByHandType
		? MirrorSettings.SourceMeshHandType != SkeletonMeshType
		: true;
}

FTransform UVRExpHandTrackingComponent::BuildPoseComponentTransform() const
{
	FTransform PoseComponentTransform = GetComponentTransform();
	const FVector ComponentScale = PoseComponentTransform.GetScale3D();
	PoseComponentTransform.SetScale3D(FVector(FMath::Abs(ComponentScale.X), FMath::Abs(ComponentScale.Y), FMath::Abs(ComponentScale.Z)));
	PoseComponentTransform.NormalizeRotation();
	return PoseComponentTransform;
}

void UVRExpHandTrackingComponent::ApplyEffectiveMeshScale(float TrackingScale)
{
	FVector EffectiveScale = MirrorSettings.UnmirroredMeshScale * TrackingScale;

	if (ShouldMirrorForCurrentHand())
	{
		const FVector MirrorSign = GetMirrorAxisSign(MirrorSettings.MirrorAxis);
		EffectiveScale.X *= MirrorSign.X;
		EffectiveScale.Y *= MirrorSign.Y;
		EffectiveScale.Z *= MirrorSign.Z;
	}

	SetRelativeScale3D(EffectiveScale);
}

FQuat UVRExpHandTrackingComponent::ApplyComponentRotationAdjustment(const FQuat& RawRotation, const FVRExpHandRotationAdjustment& RotationAdjustment) const
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

FQuat UVRExpHandTrackingComponent::ApplyParentBoneRotationOffset(const FQuat& ParentBoneSpaceRotation, const FVRExpHandRotationAdjustment& RotationAdjustment) const
{
	if (!RotationAdjustment.bEnableRotationOffset)
	{
		return ParentBoneSpaceRotation;
	}

	return (RotationAdjustment.RotationOffset.Quaternion() * ParentBoneSpaceRotation).GetNormalized();
}

FQuat UVRExpHandTrackingComponent::ApplyAxisAdjustment(const FQuat& ComponentSpaceRotation, const FVRExpHandRotationAdjustment& RotationAdjustment) const
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

EControllerHand UVRExpHandTrackingComponent::ToControllerHand(EVRExpHandType HandType)
{
	return HandType == EVRExpHandType::HandRight ? EControllerHand::Right : EControllerHand::Left;
}

EAxis::Type UVRExpHandTrackingComponent::ToEAxis(EVRExpHandMirrorAxis Axis)
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

FVector UVRExpHandTrackingComponent::GetAxisVector(EVRExpHandBoneAxis Axis)
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

FVector UVRExpHandTrackingComponent::GetMirrorAxisSign(EVRExpHandMirrorAxis MirrorAxis)
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

int32 UVRExpHandTrackingComponent::GetHandKeypointIndex(EHandKeypoint HandKeypoint)
{
	return static_cast<int32>(static_cast<uint8>(HandKeypoint));
}

bool UVRExpHandTrackingComponent::TryGetKeypointPosition(const TArray<FVector>& Positions, EHandKeypoint HandKeypoint, FVector& OutPosition)
{
	const int32 KeypointIndex = GetHandKeypointIndex(HandKeypoint);
	if (!Positions.IsValidIndex(KeypointIndex))
	{
		return false;
	}

	OutPosition = Positions[KeypointIndex];
	return true;
}

float UVRExpHandTrackingComponent::GetGestureAxisFromValues(const FVRExpHandGestureAxisValues& AxisValues, EVRExpHandGesture Gesture)
{
	switch (Gesture)
	{
	case EVRExpHandGesture::Pinch:
		return AxisValues.PinchAxis;
	case EVRExpHandGesture::Fist:
		return AxisValues.FistAxis;
	case EVRExpHandGesture::OpenPalm:
		return AxisValues.OpenPalmAxis;
	default:
		return 0.0f;
	}
}

bool UVRExpHandTrackingComponent::TryBuildAxisCorrection(const FVRExpHandBoneAxisSettings& AxisSettings, FQuat& OutAxisCorrection)
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

