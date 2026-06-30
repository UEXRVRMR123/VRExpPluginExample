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

float UVRExpPICOHandTrackingComponent::GetGestureAxis(EVRExpPICOHandGesture Gesture) const
{
	return GetGestureAxisFromValues(CurrentGestureAxisValues, Gesture);
}

FVRExpPICOGestureAxisValues UVRExpPICOHandTrackingComponent::GetGestureAxisValues() const
{
	return CurrentGestureAxisValues;
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
	if (bHandTrackingAvailable)
	{
		const EControllerHand ControllerHand = ToControllerHand(SkeletonMeshType);

		FXRMotionControllerData Data;
		UHeadMountedDisplayFunctionLibrary::GetMotionControllerData(nullptr, ControllerHand, Data);

		if (bEnableGestureRecognition && Data.bValid)
		{
			EvaluateRawOpenXRGestures(Data.HandKeyPositions, Data.HandKeyRotations);
		}
		else
		{
			ResetGestureState();
		}

		if (GetSkinnedAsset())
		{
			if (!AutoScaleComponent)
			{
				ApplyEffectiveMeshScale(1.0f);
			}

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
	}
	else
	{
		ResetGestureState();
	}

	if (AutoHide && bHidden != bHiddenInGame)
	{
		SetHiddenInGame(bHidden, true);
	}
}

void UVRExpPICOHandTrackingComponent::EvaluateRawOpenXRGestures(const TArray<FVector>& WorldPositions, const TArray<FQuat>& WorldRotations)
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

void UVRExpPICOHandTrackingComponent::ResetGestureState()
{
	ApplyGestureAxisValues(FVRExpPICOGestureAxisValues());
}

bool UVRExpPICOHandTrackingComponent::BuildGestureLocalPositions(const TArray<FVector>& WorldPositions, TArray<FVector>& OutLocalPositions, float& OutPalmWidth) const
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

FVRExpPICOGestureAxisValues UVRExpPICOHandTrackingComponent::CalculateGestureAxisValues(const TArray<FVector>& GestureLocalPositions, float PalmWidth) const
{
	FVRExpPICOGestureAxisValues AxisValues;
	AxisValues.PinchAxis = CalculatePinchAxis(GestureLocalPositions, PalmWidth);
	AxisValues.FistAxis = CalculateFistAxis(GestureLocalPositions);
	AxisValues.OpenPalmAxis = 1.0f - AxisValues.FistAxis;
	AxisValues.bGestureDataValid = true;
	return AxisValues;
}

float UVRExpPICOHandTrackingComponent::CalculatePinchAxis(const TArray<FVector>& GestureLocalPositions, float PalmWidth) const
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

float UVRExpPICOHandTrackingComponent::CalculateFistAxis(const TArray<FVector>& GestureLocalPositions) const
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

float UVRExpPICOHandTrackingComponent::CalculateFingerCurlAxis(const TArray<FVector>& GestureLocalPositions, const FGestureFingerKeypoints& FingerKeypoints) const
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

void UVRExpPICOHandTrackingComponent::ApplyGestureAxisValues(const FVRExpPICOGestureAxisValues& NewAxisValues)
{
	const FVRExpPICOGestureAxisValues OldAxisValues = CurrentGestureAxisValues;

	UpdateGestureState(EVRExpPICOHandGesture::Pinch, NewAxisValues.PinchAxis);
	UpdateGestureState(EVRExpPICOHandGesture::Fist, NewAxisValues.FistAxis);
	UpdateGestureState(EVRExpPICOHandGesture::OpenPalm, NewAxisValues.OpenPalmAxis);

	const float AxisBroadcastDelta = FMath::Max(0.0f, GestureAxisBroadcastDelta);
	if (FMath::Abs(OldAxisValues.PinchAxis - NewAxisValues.PinchAxis) >= AxisBroadcastDelta || OldAxisValues.bGestureDataValid != NewAxisValues.bGestureDataValid)
	{
		OnGestureAxisChanged.Broadcast(EVRExpPICOHandGesture::Pinch, NewAxisValues.PinchAxis);
	}

	if (FMath::Abs(OldAxisValues.FistAxis - NewAxisValues.FistAxis) >= AxisBroadcastDelta || OldAxisValues.bGestureDataValid != NewAxisValues.bGestureDataValid)
	{
		OnGestureAxisChanged.Broadcast(EVRExpPICOHandGesture::Fist, NewAxisValues.FistAxis);
	}

	if (FMath::Abs(OldAxisValues.OpenPalmAxis - NewAxisValues.OpenPalmAxis) >= AxisBroadcastDelta || OldAxisValues.bGestureDataValid != NewAxisValues.bGestureDataValid)
	{
		OnGestureAxisChanged.Broadcast(EVRExpPICOHandGesture::OpenPalm, NewAxisValues.OpenPalmAxis);
	}

	CurrentGestureAxisValues = NewAxisValues;
}

void UVRExpPICOHandTrackingComponent::UpdateGestureState(EVRExpPICOHandGesture Gesture, float NewAxisValue)
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

bool UVRExpPICOHandTrackingComponent::IsGestureActive(EVRExpPICOHandGesture Gesture) const
{
	switch (Gesture)
	{
	case EVRExpPICOHandGesture::Pinch:
		return bPinchGestureActive;
	case EVRExpPICOHandGesture::Fist:
		return bFistGestureActive;
	case EVRExpPICOHandGesture::OpenPalm:
		return bOpenPalmGestureActive;
	default:
		return false;
	}
}

void UVRExpPICOHandTrackingComponent::SetGestureActive(EVRExpPICOHandGesture Gesture, bool bActive)
{
	switch (Gesture)
	{
	case EVRExpPICOHandGesture::Pinch:
		bPinchGestureActive = bActive;
		break;
	case EVRExpPICOHandGesture::Fist:
		bFistGestureActive = bActive;
		break;
	case EVRExpPICOHandGesture::OpenPalm:
		bOpenPalmGestureActive = bActive;
		break;
	default:
		break;
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

bool UVRExpPICOHandTrackingComponent::TryGetKeypointPosition(const TArray<FVector>& Positions, EHandKeypoint HandKeypoint, FVector& OutPosition)
{
	const int32 KeypointIndex = GetHandKeypointIndex(HandKeypoint);
	if (!Positions.IsValidIndex(KeypointIndex))
	{
		return false;
	}

	OutPosition = Positions[KeypointIndex];
	return true;
}

float UVRExpPICOHandTrackingComponent::GetGestureAxisFromValues(const FVRExpPICOGestureAxisValues& AxisValues, EVRExpPICOHandGesture Gesture)
{
	switch (Gesture)
	{
	case EVRExpPICOHandGesture::Pinch:
		return AxisValues.PinchAxis;
	case EVRExpPICOHandGesture::Fist:
		return AxisValues.FistAxis;
	case EVRExpPICOHandGesture::OpenPalm:
		return AxisValues.OpenPalmAxis;
	default:
		return 0.0f;
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
