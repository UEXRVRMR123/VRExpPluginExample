// Fill out your copyright notice in the Description page of Project Settings.

#include "VRExpHandTrackingSubsystem.h"

#include "HeadMountedDisplayFunctionLibrary.h"
#include "VRExpHandTrackingFunctionLibrary.h"
#include "VRExpansionExtensionsSettings.h"

namespace
{
	static void BuildTrackedHandTransforms(FVRExpTrackedHandState& HandState)
	{
		const int32 NumKeypoints = FMath::Min(HandState.HandKeyPositions.Num(), HandState.HandKeyRotations.Num());
		HandState.HandKeyTransforms.Reset(NumKeypoints);

		for (int32 KeypointIndex = 0; KeypointIndex < NumKeypoints; ++KeypointIndex)
		{
			HandState.HandKeyTransforms.Add(FTransform(HandState.HandKeyRotations[KeypointIndex].GetNormalized(), HandState.HandKeyPositions[KeypointIndex], FVector::OneVector));
		}

		const int32 WristIndex = UVRExpHandTrackingFunctionLibrary::GetHandKeypointIndex(EHandKeypoint::Wrist);
		if (HandState.HandKeyTransforms.IsValidIndex(WristIndex))
		{
			HandState.WristTransform = HandState.HandKeyTransforms[WristIndex];
		}
	}
}

UVRExpHandTrackingSubsystem::UVRExpHandTrackingSubsystem()
	: Super()
{
	LeftHandState.HandType = EVRExpHandType::HandLeft;
	RightHandState.HandType = EVRExpHandType::HandRight;
}

void UVRExpHandTrackingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (const UVRExpansionExtensionsSettings* Settings = GetDefault<UVRExpansionExtensionsSettings>())
	{
		PalmFacingRecognitionSettings = Settings->PalmFacingRecognitionSettings;
	}
}

bool UVRExpHandTrackingSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UVRExpHandTrackingSubsystem::Tick(float DeltaTime)
{
	(void)DeltaTime;

	UpdateTrackedHand(EVRExpHandType::HandLeft);
	UpdateTrackedHand(EVRExpHandType::HandRight);
}

bool UVRExpHandTrackingSubsystem::IsTickable() const
{
	return !IsTemplate(RF_ClassDefaultObject);
}

UWorld* UVRExpHandTrackingSubsystem::GetTickableGameObjectWorld() const
{
	return GetWorld();
}

bool UVRExpHandTrackingSubsystem::IsTickableInEditor() const
{
	return false;
}

bool UVRExpHandTrackingSubsystem::IsTickableWhenPaused() const
{
	return false;
}

ETickableTickType UVRExpHandTrackingSubsystem::GetTickableTickType() const
{
	if (IsTemplate(RF_ClassDefaultObject))
	{
		return ETickableTickType::Never;
	}

	return ETickableTickType::Conditional;
}

TStatId UVRExpHandTrackingSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVRExpHandTrackingSubsystem, STATGROUP_Tickables);
}

bool UVRExpHandTrackingSubsystem::GetTrackedHandState(EVRExpHandType HandType, FVRExpTrackedHandState& OutState) const
{
	OutState = GetTrackedHandStateRef(HandType);
	return OutState.bHasHandTrackingData;
}

FVRExpHandGestureAxisValues UVRExpHandTrackingSubsystem::GetGestureAxisValues(EVRExpHandType HandType) const
{
	return GetTrackedHandStateRef(HandType).GestureAxisValues;
}

float UVRExpHandTrackingSubsystem::GetGestureAxis(EVRExpHandType HandType, EVRExpHandGesture Gesture) const
{
	return UVRExpHandTrackingFunctionLibrary::GetGestureAxis(GetGestureAxisValues(HandType), Gesture);
}

bool UVRExpHandTrackingSubsystem::IsGestureActive(EVRExpHandType HandType, EVRExpHandGesture Gesture) const
{
	return IsGestureActiveInternal(HandType, Gesture);
}

FVRExpPalmFacingAxisValues UVRExpHandTrackingSubsystem::GetPalmFacingAxisValues(EVRExpHandType HandType, EVRExpHandTrackingSpace Space) const
{
	return GetPalmFacingAxisValuesRef(HandType, Space);
}

float UVRExpHandTrackingSubsystem::GetPalmFacingAxis(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction) const
{
	const FVRExpPalmFacingAxisValues& AxisValues = GetPalmFacingAxisValuesRef(HandType, Space);
	switch (Direction)
	{
	case EVRExpPalmFacingDirection::Up:
		return AxisValues.PalmUpAxis;
	case EVRExpPalmFacingDirection::Down:
		return AxisValues.PalmDownAxis;
	default:
		return 0.0f;
	}
}

bool UVRExpHandTrackingSubsystem::IsPalmFacingActive(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction) const
{
	return IsPalmFacingActiveInternal(HandType, Space, Direction);
}

bool UVRExpHandTrackingSubsystem::GetKeypointTransform(EVRExpHandType HandType, EHandKeypoint HandKeypoint, FTransform& OutTransform) const
{
	return UVRExpHandTrackingFunctionLibrary::GetKeypointTransform(GetTrackedHandStateRef(HandType), HandKeypoint, OutTransform);
}

FVRExpHandGestureRecognitionSettings UVRExpHandTrackingSubsystem::GetGestureRecognitionSettings() const
{
	return GestureRecognitionSettings;
}

void UVRExpHandTrackingSubsystem::SetGestureRecognitionSettings(const FVRExpHandGestureRecognitionSettings& NewSettings)
{
	GestureRecognitionSettings = NewSettings;
}

FVRExpPalmFacingRecognitionSettings UVRExpHandTrackingSubsystem::GetPalmFacingRecognitionSettings() const
{
	return PalmFacingRecognitionSettings;
}

void UVRExpHandTrackingSubsystem::SetPalmFacingRecognitionSettings(const FVRExpPalmFacingRecognitionSettings& NewSettings)
{
	PalmFacingRecognitionSettings = NewSettings;
}

void UVRExpHandTrackingSubsystem::UpdateTrackedHand(EVRExpHandType HandType)
{
	FVRExpTrackedHandState NewState;
	NewState.HandType = HandType;

	UObject* WorldContext = GetWorld();

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
	FXRHandTrackingState XRHandState;
	UHeadMountedDisplayFunctionLibrary::GetHandTrackingState(WorldContext, EXRSpaceType::UnrealWorldSpace, ToControllerHand(HandType), XRHandState);

	NewState.TrackingStatus = XRHandState.TrackingStatus;
	NewState.bIsTracked = XRHandState.bValid && XRHandState.TrackingStatus == ETrackingStatus::Tracked;
	NewState.bHasHandTrackingData = NewState.bIsTracked && XRHandState.HandKeyLocations.Num() > 0 && XRHandState.HandKeyRotations.Num() > 0;

	if (NewState.bHasHandTrackingData)
	{
		NewState.HandKeyPositions = XRHandState.HandKeyLocations;
		NewState.HandKeyRotations = XRHandState.HandKeyRotations;
	}
#else
	PRAGMA_DISABLE_DEPRECATION_WARNINGS

	FXRMotionControllerData MotionControllerData;
	UHeadMountedDisplayFunctionLibrary::GetMotionControllerData(WorldContext, ToControllerHand(HandType), MotionControllerData);

	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	NewState.TrackingStatus = MotionControllerData.TrackingStatus;
	NewState.bIsTracked = MotionControllerData.bValid
		&& MotionControllerData.TrackingStatus == ETrackingStatus::Tracked
		&& MotionControllerData.DeviceVisualType == EXRVisualType::Hand;
	NewState.bHasHandTrackingData = NewState.bIsTracked && MotionControllerData.HandKeyPositions.Num() > 0 && MotionControllerData.HandKeyRotations.Num() > 0;

	if (NewState.bHasHandTrackingData)
	{
		NewState.HandKeyPositions = MotionControllerData.HandKeyPositions;
		NewState.HandKeyRotations = MotionControllerData.HandKeyRotations;
	}
#endif

	if (NewState.bHasHandTrackingData)
	{
		BuildTrackedHandTransforms(NewState);
		UVRExpHandTrackingFunctionLibrary::CalculateGestureAxisValues(NewState.HandKeyPositions, GestureRecognitionSettings, NewState.GestureAxisValues);
	}

	FVRExpTrackedHandState& CurrentState = GetMutableTrackedHandState(HandType);
	const FVRExpHandGestureAxisValues OldAxisValues = CurrentState.GestureAxisValues;
	const FVRExpPalmFacingAxisValues OldWorldPalmFacingAxisValues = GetPalmFacingAxisValuesRef(HandType, EVRExpHandTrackingSpace::WorldSpace);
	const FVRExpPalmFacingAxisValues OldXRPalmFacingAxisValues = GetPalmFacingAxisValuesRef(HandType, EVRExpHandTrackingSpace::XRSpace);

	FVRExpPalmFacingAxisValues NewWorldPalmFacingAxisValues;
	FVRExpPalmFacingAxisValues NewXRPalmFacingAxisValues;
	if (NewState.bHasHandTrackingData)
	{
		UVRExpHandTrackingFunctionLibrary::CalculatePalmFacingAxisValues(WorldContext, HandType, EVRExpHandTrackingSpace::WorldSpace, NewState.HandKeyPositions, NewState.HandKeyRotations, PalmFacingRecognitionSettings, NewWorldPalmFacingAxisValues);
		UVRExpHandTrackingFunctionLibrary::CalculatePalmFacingAxisValues(WorldContext, HandType, EVRExpHandTrackingSpace::XRSpace, NewState.HandKeyPositions, NewState.HandKeyRotations, PalmFacingRecognitionSettings, NewXRPalmFacingAxisValues);
	}

	CurrentState = MoveTemp(NewState);
	ApplyGestureAxisValues(HandType, OldAxisValues, CurrentState.GestureAxisValues);

	GetMutablePalmFacingAxisValues(HandType, EVRExpHandTrackingSpace::WorldSpace) = NewWorldPalmFacingAxisValues;
	GetMutablePalmFacingAxisValues(HandType, EVRExpHandTrackingSpace::XRSpace) = NewXRPalmFacingAxisValues;
	ApplyPalmFacingAxisValues(HandType, EVRExpHandTrackingSpace::WorldSpace, OldWorldPalmFacingAxisValues, NewWorldPalmFacingAxisValues);
	ApplyPalmFacingAxisValues(HandType, EVRExpHandTrackingSpace::XRSpace, OldXRPalmFacingAxisValues, NewXRPalmFacingAxisValues);
}

void UVRExpHandTrackingSubsystem::ApplyGestureAxisValues(EVRExpHandType HandType, const FVRExpHandGestureAxisValues& OldAxisValues, const FVRExpHandGestureAxisValues& NewAxisValues)
{
	UpdateGestureState(HandType, EVRExpHandGesture::Pinch, NewAxisValues.PinchAxis);
	UpdateGestureState(HandType, EVRExpHandGesture::Fist, NewAxisValues.FistAxis);
	UpdateGestureState(HandType, EVRExpHandGesture::OpenPalm, NewAxisValues.OpenPalmAxis);

	const float AxisBroadcastDelta = FMath::Max(0.0f, GestureRecognitionSettings.GestureAxisBroadcastDelta);
	if (FMath::Abs(OldAxisValues.PinchAxis - NewAxisValues.PinchAxis) >= AxisBroadcastDelta || OldAxisValues.bGestureDataValid != NewAxisValues.bGestureDataValid)
	{
		OnGestureAxisChanged.Broadcast(HandType, EVRExpHandGesture::Pinch, NewAxisValues.PinchAxis);
	}

	if (FMath::Abs(OldAxisValues.FistAxis - NewAxisValues.FistAxis) >= AxisBroadcastDelta || OldAxisValues.bGestureDataValid != NewAxisValues.bGestureDataValid)
	{
		OnGestureAxisChanged.Broadcast(HandType, EVRExpHandGesture::Fist, NewAxisValues.FistAxis);
	}

	if (FMath::Abs(OldAxisValues.OpenPalmAxis - NewAxisValues.OpenPalmAxis) >= AxisBroadcastDelta || OldAxisValues.bGestureDataValid != NewAxisValues.bGestureDataValid)
	{
		OnGestureAxisChanged.Broadcast(HandType, EVRExpHandGesture::OpenPalm, NewAxisValues.OpenPalmAxis);
	}
}

void UVRExpHandTrackingSubsystem::ApplyPalmFacingAxisValues(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, const FVRExpPalmFacingAxisValues& OldAxisValues, const FVRExpPalmFacingAxisValues& NewAxisValues)
{
	UpdatePalmFacingState(HandType, Space, EVRExpPalmFacingDirection::Up, NewAxisValues.PalmUpAxis);
	UpdatePalmFacingState(HandType, Space, EVRExpPalmFacingDirection::Down, NewAxisValues.PalmDownAxis);

	const float AxisBroadcastDelta = FMath::Max(0.0f, PalmFacingRecognitionSettings.PalmFacingAxisBroadcastDelta);
	if (FMath::Abs(OldAxisValues.PalmUpAxis - NewAxisValues.PalmUpAxis) >= AxisBroadcastDelta || OldAxisValues.bPalmFacingDataValid != NewAxisValues.bPalmFacingDataValid)
	{
		OnPalmFacingAxisChanged.Broadcast(HandType, Space, EVRExpPalmFacingDirection::Up, NewAxisValues.PalmUpAxis);
	}

	if (FMath::Abs(OldAxisValues.PalmDownAxis - NewAxisValues.PalmDownAxis) >= AxisBroadcastDelta || OldAxisValues.bPalmFacingDataValid != NewAxisValues.bPalmFacingDataValid)
	{
		OnPalmFacingAxisChanged.Broadcast(HandType, Space, EVRExpPalmFacingDirection::Down, NewAxisValues.PalmDownAxis);
	}
}

void UVRExpHandTrackingSubsystem::UpdateGestureState(EVRExpHandType HandType, EVRExpHandGesture Gesture, float NewAxisValue)
{
	const bool bWasActive = IsGestureActiveInternal(HandType, Gesture);
	const float ActiveThreshold = FMath::Clamp(GestureRecognitionSettings.GestureActiveThreshold, 0.0f, 1.0f);
	const float InactiveThreshold = FMath::Min(FMath::Clamp(GestureRecognitionSettings.GestureInactiveThreshold, 0.0f, 1.0f), ActiveThreshold);
	const bool bShouldBeActive = bWasActive
		? NewAxisValue > InactiveThreshold
		: NewAxisValue >= ActiveThreshold;

	if (bWasActive == bShouldBeActive)
	{
		return;
	}

	SetGestureActiveInternal(HandType, Gesture, bShouldBeActive);
	if (bShouldBeActive)
	{
		OnGestureStarted.Broadcast(HandType, Gesture, NewAxisValue);
	}
	else
	{
		OnGestureEnded.Broadcast(HandType, Gesture, NewAxisValue);
	}
}

void UVRExpHandTrackingSubsystem::UpdatePalmFacingState(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction, float NewAxisValue)
{
	const bool bWasActive = IsPalmFacingActiveInternal(HandType, Space, Direction);
	const float ActiveThreshold = FMath::Clamp(PalmFacingRecognitionSettings.PalmFacingActiveThreshold, 0.0f, 1.0f);
	const float InactiveThreshold = FMath::Min(FMath::Clamp(PalmFacingRecognitionSettings.PalmFacingInactiveThreshold, 0.0f, 1.0f), ActiveThreshold);
	const bool bShouldBeActive = bWasActive
		? NewAxisValue > InactiveThreshold
		: NewAxisValue >= ActiveThreshold;

	if (bWasActive == bShouldBeActive)
	{
		return;
	}

	SetPalmFacingActiveInternal(HandType, Space, Direction, bShouldBeActive);
	if (bShouldBeActive)
	{
		OnPalmFacingStarted.Broadcast(HandType, Space, Direction, NewAxisValue);
	}
	else
	{
		OnPalmFacingEnded.Broadcast(HandType, Space, Direction, NewAxisValue);
	}
}

bool UVRExpHandTrackingSubsystem::IsGestureActiveInternal(EVRExpHandType HandType, EVRExpHandGesture Gesture) const
{
	const FGestureActiveState& ActiveState = GetGestureActiveStateRef(HandType);

	switch (Gesture)
	{
	case EVRExpHandGesture::Pinch:
		return ActiveState.bPinchGestureActive;
	case EVRExpHandGesture::Fist:
		return ActiveState.bFistGestureActive;
	case EVRExpHandGesture::OpenPalm:
		return ActiveState.bOpenPalmGestureActive;
	default:
		return false;
	}
}

bool UVRExpHandTrackingSubsystem::IsPalmFacingActiveInternal(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction) const
{
	const FPalmFacingActiveState& ActiveState = GetPalmFacingActiveStateRef(HandType, Space);

	switch (Direction)
	{
	case EVRExpPalmFacingDirection::Up:
		return ActiveState.bPalmUpActive;
	case EVRExpPalmFacingDirection::Down:
		return ActiveState.bPalmDownActive;
	default:
		return false;
	}
}

void UVRExpHandTrackingSubsystem::SetGestureActiveInternal(EVRExpHandType HandType, EVRExpHandGesture Gesture, bool bActive)
{
	FGestureActiveState& ActiveState = GetMutableGestureActiveState(HandType);

	switch (Gesture)
	{
	case EVRExpHandGesture::Pinch:
		ActiveState.bPinchGestureActive = bActive;
		break;
	case EVRExpHandGesture::Fist:
		ActiveState.bFistGestureActive = bActive;
		break;
	case EVRExpHandGesture::OpenPalm:
		ActiveState.bOpenPalmGestureActive = bActive;
		break;
	default:
		break;
	}
}

void UVRExpHandTrackingSubsystem::SetPalmFacingActiveInternal(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction, bool bActive)
{
	FPalmFacingActiveState& ActiveState = GetMutablePalmFacingActiveState(HandType, Space);

	switch (Direction)
	{
	case EVRExpPalmFacingDirection::Up:
		ActiveState.bPalmUpActive = bActive;
		break;
	case EVRExpPalmFacingDirection::Down:
		ActiveState.bPalmDownActive = bActive;
		break;
	default:
		break;
	}
}

FVRExpTrackedHandState& UVRExpHandTrackingSubsystem::GetMutableTrackedHandState(EVRExpHandType HandType)
{
	return HandType == EVRExpHandType::HandRight ? RightHandState : LeftHandState;
}

const FVRExpTrackedHandState& UVRExpHandTrackingSubsystem::GetTrackedHandStateRef(EVRExpHandType HandType) const
{
	return HandType == EVRExpHandType::HandRight ? RightHandState : LeftHandState;
}

UVRExpHandTrackingSubsystem::FGestureActiveState& UVRExpHandTrackingSubsystem::GetMutableGestureActiveState(EVRExpHandType HandType)
{
	return HandType == EVRExpHandType::HandRight ? RightGestureActiveState : LeftGestureActiveState;
}

const UVRExpHandTrackingSubsystem::FGestureActiveState& UVRExpHandTrackingSubsystem::GetGestureActiveStateRef(EVRExpHandType HandType) const
{
	return HandType == EVRExpHandType::HandRight ? RightGestureActiveState : LeftGestureActiveState;
}

FVRExpPalmFacingAxisValues& UVRExpHandTrackingSubsystem::GetMutablePalmFacingAxisValues(EVRExpHandType HandType, EVRExpHandTrackingSpace Space)
{
	if (HandType == EVRExpHandType::HandRight)
	{
		return Space == EVRExpHandTrackingSpace::XRSpace ? RightXRPalmFacingAxisValues : RightWorldPalmFacingAxisValues;
	}

	return Space == EVRExpHandTrackingSpace::XRSpace ? LeftXRPalmFacingAxisValues : LeftWorldPalmFacingAxisValues;
}

const FVRExpPalmFacingAxisValues& UVRExpHandTrackingSubsystem::GetPalmFacingAxisValuesRef(EVRExpHandType HandType, EVRExpHandTrackingSpace Space) const
{
	if (HandType == EVRExpHandType::HandRight)
	{
		return Space == EVRExpHandTrackingSpace::XRSpace ? RightXRPalmFacingAxisValues : RightWorldPalmFacingAxisValues;
	}

	return Space == EVRExpHandTrackingSpace::XRSpace ? LeftXRPalmFacingAxisValues : LeftWorldPalmFacingAxisValues;
}

UVRExpHandTrackingSubsystem::FPalmFacingActiveState& UVRExpHandTrackingSubsystem::GetMutablePalmFacingActiveState(EVRExpHandType HandType, EVRExpHandTrackingSpace Space)
{
	if (HandType == EVRExpHandType::HandRight)
	{
		return Space == EVRExpHandTrackingSpace::XRSpace ? RightXRPalmFacingActiveState : RightWorldPalmFacingActiveState;
	}

	return Space == EVRExpHandTrackingSpace::XRSpace ? LeftXRPalmFacingActiveState : LeftWorldPalmFacingActiveState;
}

const UVRExpHandTrackingSubsystem::FPalmFacingActiveState& UVRExpHandTrackingSubsystem::GetPalmFacingActiveStateRef(EVRExpHandType HandType, EVRExpHandTrackingSpace Space) const
{
	if (HandType == EVRExpHandType::HandRight)
	{
		return Space == EVRExpHandTrackingSpace::XRSpace ? RightXRPalmFacingActiveState : RightWorldPalmFacingActiveState;
	}

	return Space == EVRExpHandTrackingSpace::XRSpace ? LeftXRPalmFacingActiveState : LeftWorldPalmFacingActiveState;
}

EControllerHand UVRExpHandTrackingSubsystem::ToControllerHand(EVRExpHandType HandType)
{
	return HandType == EVRExpHandType::HandRight ? EControllerHand::Right : EControllerHand::Left;
}
