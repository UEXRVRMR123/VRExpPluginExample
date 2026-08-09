// Fill out your copyright notice in the Description page of Project Settings.

#include "HandTracking/VRExpHandTrackingFunctionLibrary.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "HandTracking/VRExpHandTrackingSubsystem.h"

namespace VRExpHandTrackingMath
{
	struct FGestureFingerKeypoints
	{
		EHandKeypoint Metacarpal;
		EHandKeypoint Proximal;
		EHandKeypoint Tip;
	};

	static bool BuildGestureLocalPositions(const TArray<FVector>& WorldPositions, TArray<FVector>& OutLocalPositions, float& OutPalmWidth)
	{
		OutLocalPositions.Reset();
		OutPalmWidth = 0.0f;

		FVector WristPosition;
		FVector IndexProximalPosition;
		FVector MiddleProximalPosition;
		FVector RingProximalPosition;
		FVector LittleProximalPosition;
		if (!UVRExpHandTrackingFunctionLibrary::GetKeypointPosition(WorldPositions, EHandKeypoint::Wrist, WristPosition)
			|| !UVRExpHandTrackingFunctionLibrary::GetKeypointPosition(WorldPositions, EHandKeypoint::IndexProximal, IndexProximalPosition)
			|| !UVRExpHandTrackingFunctionLibrary::GetKeypointPosition(WorldPositions, EHandKeypoint::MiddleProximal, MiddleProximalPosition)
			|| !UVRExpHandTrackingFunctionLibrary::GetKeypointPosition(WorldPositions, EHandKeypoint::RingProximal, RingProximalPosition)
			|| !UVRExpHandTrackingFunctionLibrary::GetKeypointPosition(WorldPositions, EHandKeypoint::LittleProximal, LittleProximalPosition))
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
		if (UVRExpHandTrackingFunctionLibrary::GetKeypointPosition(WorldPositions, EHandKeypoint::IndexMetacarpal, IndexMetacarpalPosition)
			&& UVRExpHandTrackingFunctionLibrary::GetKeypointPosition(WorldPositions, EHandKeypoint::LittleMetacarpal, LittleMetacarpalPosition))
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

	static float CalculatePinchAxis(const TArray<FVector>& GestureLocalPositions, float PalmWidth, const FVRExpHandGestureRecognitionSettings& Settings)
	{
		FVector ThumbTipPosition;
		FVector IndexTipPosition;
		if (PalmWidth <= KINDA_SMALL_NUMBER
			|| !UVRExpHandTrackingFunctionLibrary::GetKeypointPosition(GestureLocalPositions, EHandKeypoint::ThumbTip, ThumbTipPosition)
			|| !UVRExpHandTrackingFunctionLibrary::GetKeypointPosition(GestureLocalPositions, EHandKeypoint::IndexTip, IndexTipPosition))
		{
			return 0.0f;
		}

		const float ClosedDistance = FMath::Max(0.0f, Settings.PinchClosedDistanceScale) * PalmWidth;
		const float OpenDistance = FMath::Max(ClosedDistance + KINDA_SMALL_NUMBER, FMath::Max(0.0f, Settings.PinchOpenDistanceScale) * PalmWidth);
		const float TipDistance = FVector::Distance(ThumbTipPosition, IndexTipPosition);
		const float DistanceAlpha = (TipDistance - ClosedDistance) / (OpenDistance - ClosedDistance);
		return FMath::Clamp(1.0f - DistanceAlpha, 0.0f, 1.0f);
	}

	static float CalculateFingerCurlAxis(const TArray<FVector>& GestureLocalPositions, const FGestureFingerKeypoints& FingerKeypoints, const FVRExpHandGestureRecognitionSettings& Settings)
	{
		FVector ProximalPosition;
		FVector TipPosition;
		if (!UVRExpHandTrackingFunctionLibrary::GetKeypointPosition(GestureLocalPositions, FingerKeypoints.Proximal, ProximalPosition)
			|| !UVRExpHandTrackingFunctionLibrary::GetKeypointPosition(GestureLocalPositions, FingerKeypoints.Tip, TipPosition))
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
		const float OpenAngle = FMath::Clamp(Settings.FingerOpenAngleDegrees, 0.0f, 180.0f);
		const float ClosedAngle = FMath::Clamp(Settings.FingerClosedAngleDegrees, 0.0f, 180.0f);
		if (ClosedAngle <= OpenAngle + KINDA_SMALL_NUMBER)
		{
			return FingerAngleDegrees >= ClosedAngle ? 1.0f : 0.0f;
		}

		const float AngleAlpha = (FingerAngleDegrees - OpenAngle) / (ClosedAngle - OpenAngle);
		return FMath::Clamp(AngleAlpha, 0.0f, 1.0f);
	}

	static float CalculateFistAxis(const TArray<FVector>& GestureLocalPositions, const FVRExpHandGestureRecognitionSettings& Settings)
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
			TotalCurlAxis += CalculateFingerCurlAxis(GestureLocalPositions, Finger, Settings);
		}

		return FMath::Clamp(TotalCurlAxis / UE_ARRAY_COUNT(Fingers), 0.0f, 1.0f);
	}

	static FVector GetPalmRotationNormalAxis(EVRExpPalmRotationNormalAxis NormalAxis)
	{
		switch (NormalAxis)
		{
		case EVRExpPalmRotationNormalAxis::X:
			return FVector::ForwardVector;
		case EVRExpPalmRotationNormalAxis::Y:
			return FVector::RightVector;
		case EVRExpPalmRotationNormalAxis::Z:
		default:
			return FVector::UpVector;
		}
	}

	static bool BuildPalmNormalFromRotation(const TArray<FQuat>& WorldRotations, const FVRExpPalmFacingRecognitionSettings& Settings, FVector& OutPalmNormal)
	{
		OutPalmNormal = FVector::ZeroVector;

		const int32 PalmIndex = UVRExpHandTrackingFunctionLibrary::GetHandKeypointIndex(EHandKeypoint::Palm);
		if (!WorldRotations.IsValidIndex(PalmIndex))
		{
			return false;
		}

		FQuat PalmRotation = WorldRotations[PalmIndex];
		if (PalmRotation.ContainsNaN() || PalmRotation.SizeSquared() <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		PalmRotation.Normalize();
		OutPalmNormal = PalmRotation.RotateVector(GetPalmRotationNormalAxis(Settings.PalmRotationNormalAxis)).GetSafeNormal();
		return !OutPalmNormal.IsNearlyZero();
	}

	static bool BuildReconstructedPalmNormal(const TArray<FVector>& WorldPositions, EVRExpHandType HandType, FVector& OutPalmNormal)
	{
		OutPalmNormal = FVector::ZeroVector;

		FVector WristPosition;
		FVector IndexProximalPosition;
		FVector MiddleProximalPosition;
		FVector LittleProximalPosition;
		if (!UVRExpHandTrackingFunctionLibrary::GetKeypointPosition(WorldPositions, EHandKeypoint::Wrist, WristPosition)
			|| !UVRExpHandTrackingFunctionLibrary::GetKeypointPosition(WorldPositions, EHandKeypoint::IndexProximal, IndexProximalPosition)
			|| !UVRExpHandTrackingFunctionLibrary::GetKeypointPosition(WorldPositions, EHandKeypoint::MiddleProximal, MiddleProximalPosition)
			|| !UVRExpHandTrackingFunctionLibrary::GetKeypointPosition(WorldPositions, EHandKeypoint::LittleProximal, LittleProximalPosition))
		{
			return false;
		}

		const FVector ForwardAxis = (MiddleProximalPosition - WristPosition).GetSafeNormal();
		const FVector AcrossPalmAxis = (LittleProximalPosition - IndexProximalPosition).GetSafeNormal();
		if (ForwardAxis.IsNearlyZero() || AcrossPalmAxis.IsNearlyZero())
		{
			return false;
		}

		OutPalmNormal = FVector::CrossProduct(AcrossPalmAxis, ForwardAxis).GetSafeNormal();
		if (OutPalmNormal.IsNearlyZero())
		{
			return false;
		}

		if (HandType == EVRExpHandType::HandRight)
		{
			OutPalmNormal *= -1.0f;
		}

		return true;
	}

	static bool BuildPalmFacingNormal(const TArray<FVector>& WorldPositions, const TArray<FQuat>& WorldRotations, EVRExpHandType HandType, const FVRExpPalmFacingRecognitionSettings& Settings, FVector& OutPalmNormal)
	{
		bool bBuiltNormal = false;
		switch (Settings.PalmNormalCalculationMode)
		{
		case EVRExpPalmNormalCalculationMode::ReconstructFromKeypoints:
			bBuiltNormal = BuildReconstructedPalmNormal(WorldPositions, HandType, OutPalmNormal);
			break;
		case EVRExpPalmNormalCalculationMode::PalmRotationAxis:
		default:
			bBuiltNormal = BuildPalmNormalFromRotation(WorldRotations, Settings, OutPalmNormal);
			break;
		}

		if (!bBuiltNormal)
		{
			return false;
		}

		if (Settings.bInvertPalmFacingNormal)
		{
			OutPalmNormal *= -1.0f;
		}

		return true;
	}
}

namespace VRExpHandTrackingHelpers
{
	static UWorld* ResolveWorldFromContext(const UObject* WorldContextObject)
	{
		if (WorldContextObject == nullptr)
		{
			return nullptr;
		}

		if (GEngine != nullptr)
		{
			return GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		}

		return WorldContextObject->GetWorld();
	}

	static bool GetLocalPlayerCameraParentTransform(const UObject* WorldContextObject, FTransform& OutParentTransform)
	{
		OutParentTransform = FTransform::Identity;

		UWorld* World = ResolveWorldFromContext(WorldContextObject);
		if (!IsValid(World))
		{
			return false;
		}

		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
		if (!IsValid(PlayerController))
		{
			return false;
		}

		APawn* LocalPawn = PlayerController->GetPawn();
		if (!IsValid(LocalPawn))
		{
			return false;
		}

		UCameraComponent* CameraComponent = LocalPawn->FindComponentByClass<UCameraComponent>();
		if (!IsValid(CameraComponent))
		{
			return false;
		}

		USceneComponent* CameraParent = CameraComponent->GetAttachParent();
		if (!IsValid(CameraParent))
		{
			return false;
		}

		OutParentTransform = CameraParent->GetComponentTransform();
		return true;
	}

	static bool GetTrackingSpaceUpVector(const UObject* WorldContextObject, EVRExpHandTrackingSpace Space, FVector& OutUpVector)
	{
		OutUpVector = FVector::ZeroVector;

		switch (Space)
		{
		case EVRExpHandTrackingSpace::WorldSpace:
			OutUpVector = FVector::UpVector;
			return true;

		case EVRExpHandTrackingSpace::XRSpace:
		{
			FTransform CameraParentTransform;
			if (!GetLocalPlayerCameraParentTransform(WorldContextObject, CameraParentTransform))
			{
				return false;
			}

			OutUpVector = CameraParentTransform.TransformVectorNoScale(FVector::UpVector).GetSafeNormal();
			return !OutUpVector.IsNearlyZero();
		}

		default:
			return false;
		}
	}
}

UVRExpHandTrackingSubsystem* UVRExpHandTrackingFunctionLibrary::GetHandTrackingSubsystem(const UObject* WorldContextObject)
{
	UWorld* World = VRExpHandTrackingHelpers::ResolveWorldFromContext(WorldContextObject);
	return IsValid(World) ? World->GetSubsystem<UVRExpHandTrackingSubsystem>() : nullptr;
}

bool UVRExpHandTrackingFunctionLibrary::GetTrackedHandState(const UObject* WorldContextObject, EVRExpHandType HandType, FVRExpTrackedHandState& OutState)
{
	if (UVRExpHandTrackingSubsystem* Subsystem = GetHandTrackingSubsystem(WorldContextObject))
	{
		return Subsystem->GetTrackedHandState(HandType, OutState);
	}

	OutState = FVRExpTrackedHandState();
	OutState.HandType = HandType;
	return false;
}

FVRExpHandGestureAxisValues UVRExpHandTrackingFunctionLibrary::GetGestureAxisValues(const UObject* WorldContextObject, EVRExpHandType HandType)
{
	if (UVRExpHandTrackingSubsystem* Subsystem = GetHandTrackingSubsystem(WorldContextObject))
	{
		return Subsystem->GetGestureAxisValues(HandType);
	}

	return FVRExpHandGestureAxisValues();
}

float UVRExpHandTrackingFunctionLibrary::GetTrackedGestureAxis(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandGesture Gesture)
{
	if (UVRExpHandTrackingSubsystem* Subsystem = GetHandTrackingSubsystem(WorldContextObject))
	{
		return Subsystem->GetGestureAxis(HandType, Gesture);
	}

	return 0.0f;
}

FVRExpPalmFacingAxisValues UVRExpHandTrackingFunctionLibrary::GetTrackedPalmFacingAxisValues(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandTrackingSpace Space)
{
	if (UVRExpHandTrackingSubsystem* Subsystem = GetHandTrackingSubsystem(WorldContextObject))
	{
		return Subsystem->GetPalmFacingAxisValues(HandType, Space);
	}

	return FVRExpPalmFacingAxisValues();
}

float UVRExpHandTrackingFunctionLibrary::GetTrackedPalmFacingAxis(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction)
{
	if (UVRExpHandTrackingSubsystem* Subsystem = GetHandTrackingSubsystem(WorldContextObject))
	{
		return Subsystem->GetPalmFacingAxis(HandType, Space, Direction);
	}

	return 0.0f;
}

bool UVRExpHandTrackingFunctionLibrary::IsTrackedPalmFacingActive(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction)
{
	if (UVRExpHandTrackingSubsystem* Subsystem = GetHandTrackingSubsystem(WorldContextObject))
	{
		return Subsystem->IsPalmFacingActive(HandType, Space, Direction);
	}

	return false;
}

float UVRExpHandTrackingFunctionLibrary::GetGestureAxis(const FVRExpHandGestureAxisValues& AxisValues, EVRExpHandGesture Gesture)
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

bool UVRExpHandTrackingFunctionLibrary::IsGestureActive(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandGesture Gesture)
{
	if (UVRExpHandTrackingSubsystem* Subsystem = GetHandTrackingSubsystem(WorldContextObject))
	{
		return Subsystem->IsGestureActive(HandType, Gesture);
	}

	return false;
}

bool UVRExpHandTrackingFunctionLibrary::GetKeypointTransform(const FVRExpTrackedHandState& HandState, EHandKeypoint HandKeypoint, FTransform& OutTransform)
{
	const int32 KeypointIndex = GetHandKeypointIndex(HandKeypoint);
	if (!HandState.HandKeyTransforms.IsValidIndex(KeypointIndex))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	OutTransform = HandState.HandKeyTransforms[KeypointIndex];
	return true;
}

bool UVRExpHandTrackingFunctionLibrary::GetTrackedHandKeypointPosition(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EHandKeypoint HandKeypoint, FVector& OutPosition)
{
	FRotator UnusedRotation;
	return GetTrackedHandKeypointPositionAndRotation(WorldContextObject, HandType, Space, HandKeypoint, OutPosition, UnusedRotation);
}

bool UVRExpHandTrackingFunctionLibrary::GetTrackedHandKeypointRotation(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EHandKeypoint HandKeypoint, FRotator& OutRotation)
{
	FVector UnusedPosition;
	return GetTrackedHandKeypointPositionAndRotation(WorldContextObject, HandType, Space, HandKeypoint, UnusedPosition, OutRotation);
}

bool UVRExpHandTrackingFunctionLibrary::GetTrackedHandKeypointPositionAndRotation(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EHandKeypoint HandKeypoint, FVector& OutPosition, FRotator& OutRotation)
{
	OutPosition = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;

	FVRExpTrackedHandState HandState;
	if (!GetTrackedHandState(WorldContextObject, HandType, HandState))
	{
		return false;
	}

	FTransform KeypointWorldTransform;
	if (!GetKeypointTransform(HandState, HandKeypoint, KeypointWorldTransform))
	{
		return false;
	}
	KeypointWorldTransform.NormalizeRotation();

	switch (Space)
	{
	case EVRExpHandTrackingSpace::WorldSpace:
		OutPosition = KeypointWorldTransform.GetLocation();
		OutRotation = KeypointWorldTransform.GetRotation().Rotator();
		return true;

	case EVRExpHandTrackingSpace::XRSpace:
	{
		FTransform CameraParentTransform;
		if (!VRExpHandTrackingHelpers::GetLocalPlayerCameraParentTransform(WorldContextObject, CameraParentTransform))
		{
			return false;
		}

		OutPosition = CameraParentTransform.InverseTransformPosition(KeypointWorldTransform.GetLocation());
		OutRotation = CameraParentTransform.InverseTransformRotation(KeypointWorldTransform.GetRotation()).Rotator();
		return true;
	}

	default:
		return false;
	}
}

int32 UVRExpHandTrackingFunctionLibrary::GetHandKeypointIndex(EHandKeypoint HandKeypoint)
{
	return static_cast<int32>(static_cast<uint8>(HandKeypoint));
}

bool UVRExpHandTrackingFunctionLibrary::GetKeypointPosition(const TArray<FVector>& Positions, EHandKeypoint HandKeypoint, FVector& OutPosition)
{
	const int32 KeypointIndex = GetHandKeypointIndex(HandKeypoint);
	if (!Positions.IsValidIndex(KeypointIndex))
	{
		OutPosition = FVector::ZeroVector;
		return false;
	}

	OutPosition = Positions[KeypointIndex];
	return true;
}

bool UVRExpHandTrackingFunctionLibrary::CalculateGestureAxisValues(const TArray<FVector>& WorldPositions, const FVRExpHandGestureRecognitionSettings& Settings, FVRExpHandGestureAxisValues& OutAxisValues)
{
	OutAxisValues = FVRExpHandGestureAxisValues();

	if (!Settings.bEnableGestureRecognition)
	{
		return false;
	}

	TArray<FVector> GestureLocalPositions;
	float PalmWidth = 0.0f;
	if (!VRExpHandTrackingMath::BuildGestureLocalPositions(WorldPositions, GestureLocalPositions, PalmWidth))
	{
		return false;
	}

	OutAxisValues.PinchAxis = VRExpHandTrackingMath::CalculatePinchAxis(GestureLocalPositions, PalmWidth, Settings);
	OutAxisValues.FistAxis = VRExpHandTrackingMath::CalculateFistAxis(GestureLocalPositions, Settings);
	OutAxisValues.OpenPalmAxis = 1.0f - OutAxisValues.FistAxis;
	OutAxisValues.bGestureDataValid = true;
	return true;
}

bool UVRExpHandTrackingFunctionLibrary::CalculatePalmFacingAxisValues(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandTrackingSpace Space, const TArray<FVector>& WorldPositions, const TArray<FQuat>& WorldRotations, const FVRExpPalmFacingRecognitionSettings& Settings, FVRExpPalmFacingAxisValues& OutAxisValues)
{
	OutAxisValues = FVRExpPalmFacingAxisValues();

	FVector PalmNormal;
	if (!VRExpHandTrackingMath::BuildPalmFacingNormal(WorldPositions, WorldRotations, HandType, Settings, PalmNormal))
	{
		return false;
	}

	FVector TrackingSpaceUpVector;
	if (!VRExpHandTrackingHelpers::GetTrackingSpaceUpVector(WorldContextObject, Space, TrackingSpaceUpVector))
	{
		return false;
	}

	const float PalmUpDot = FVector::DotProduct(PalmNormal, TrackingSpaceUpVector);
	OutAxisValues.PalmUpAxis = FMath::Clamp(PalmUpDot, 0.0f, 1.0f);
	OutAxisValues.PalmDownAxis = FMath::Clamp(-PalmUpDot, 0.0f, 1.0f);
	OutAxisValues.bPalmFacingDataValid = true;
	return true;
}
