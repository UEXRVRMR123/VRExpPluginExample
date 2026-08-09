// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HandTracking/VRExpHandTrackingTypes.h"
#include "VRExpHandTrackingFunctionLibrary.generated.h"

class UVRExpHandTrackingSubsystem;

UCLASS()
class VREXPANSIONEXTENSIONS_API UVRExpHandTrackingFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking", meta = (WorldContext = "WorldContextObject"))
	static UVRExpHandTrackingSubsystem* GetHandTrackingSubsystem(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking", meta = (WorldContext = "WorldContextObject"))
	static bool GetTrackedHandState(const UObject* WorldContextObject, EVRExpHandType HandType, FVRExpTrackedHandState& OutState);

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|Gesture", meta = (WorldContext = "WorldContextObject"))
	static FVRExpHandGestureAxisValues GetGestureAxisValues(const UObject* WorldContextObject, EVRExpHandType HandType);

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|Gesture", meta = (WorldContext = "WorldContextObject"))
	static float GetTrackedGestureAxis(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandGesture Gesture);

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|PalmFacing", meta = (WorldContext = "WorldContextObject"))
	static FVRExpPalmFacingAxisValues GetTrackedPalmFacingAxisValues(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandTrackingSpace Space);

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|PalmFacing", meta = (WorldContext = "WorldContextObject"))
	static float GetTrackedPalmFacingAxis(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction);

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|PalmFacing", meta = (WorldContext = "WorldContextObject"))
	static bool IsTrackedPalmFacingActive(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction);

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|Gesture")
	static float GetGestureAxis(const FVRExpHandGestureAxisValues& AxisValues, EVRExpHandGesture Gesture);

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|Gesture", meta = (WorldContext = "WorldContextObject"))
	static bool IsGestureActive(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandGesture Gesture);

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking")
	static bool GetKeypointTransform(const FVRExpTrackedHandState& HandState, EHandKeypoint HandKeypoint, FTransform& OutTransform);

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking", meta = (WorldContext = "WorldContextObject"))
	static bool GetTrackedHandKeypointPosition(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EHandKeypoint HandKeypoint, FVector& OutPosition);

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking", meta = (WorldContext = "WorldContextObject"))
	static bool GetTrackedHandKeypointRotation(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EHandKeypoint HandKeypoint, FRotator& OutRotation);

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking", meta = (WorldContext = "WorldContextObject"))
	static bool GetTrackedHandKeypointPositionAndRotation(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EHandKeypoint HandKeypoint, FVector& OutPosition, FRotator& OutRotation);

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking")
	static int32 GetHandKeypointIndex(EHandKeypoint HandKeypoint);

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking")
	static bool GetKeypointPosition(const TArray<FVector>& Positions, EHandKeypoint HandKeypoint, FVector& OutPosition);

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|Gesture", meta = (AutoCreateRefTerm = "WorldPositions,Settings"))
	static bool CalculateGestureAxisValues(const TArray<FVector>& WorldPositions, const FVRExpHandGestureRecognitionSettings& Settings, FVRExpHandGestureAxisValues& OutAxisValues);

	static bool CalculatePalmFacingAxisValues(const UObject* WorldContextObject, EVRExpHandType HandType, EVRExpHandTrackingSpace Space, const TArray<FVector>& WorldPositions, const TArray<FQuat>& WorldRotations, const FVRExpPalmFacingRecognitionSettings& Settings, FVRExpPalmFacingAxisValues& OutAxisValues);
};
