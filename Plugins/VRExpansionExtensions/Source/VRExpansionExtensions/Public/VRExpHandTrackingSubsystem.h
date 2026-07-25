// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "VRExpHandTrackingTypes.h"
#include "VRExpHandTrackingSubsystem.generated.h"

UCLASS()
class VREXPANSIONEXTENSIONS_API UVRExpHandTrackingSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UVRExpHandTrackingSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual bool IsTickableInEditor() const;
	virtual bool IsTickableWhenPaused() const override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking")
	bool GetTrackedHandState(EVRExpHandType HandType, FVRExpTrackedHandState& OutState) const;

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|Gesture")
	FVRExpHandGestureAxisValues GetGestureAxisValues(EVRExpHandType HandType) const;

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|Gesture")
	float GetGestureAxis(EVRExpHandType HandType, EVRExpHandGesture Gesture) const;

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|Gesture")
	bool IsGestureActive(EVRExpHandType HandType, EVRExpHandGesture Gesture) const;

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|PalmFacing")
	FVRExpPalmFacingAxisValues GetPalmFacingAxisValues(EVRExpHandType HandType, EVRExpHandTrackingSpace Space) const;

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|PalmFacing")
	float GetPalmFacingAxis(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction) const;

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|PalmFacing")
	bool IsPalmFacingActive(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction) const;

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking")
	bool GetKeypointTransform(EVRExpHandType HandType, EHandKeypoint HandKeypoint, FTransform& OutTransform) const;

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|Gesture")
	FVRExpHandGestureRecognitionSettings GetGestureRecognitionSettings() const;

	UFUNCTION(BlueprintCallable, Category = "VRExp|HandTracking|Gesture")
	void SetGestureRecognitionSettings(const FVRExpHandGestureRecognitionSettings& NewSettings);

	UFUNCTION(BlueprintPure, Category = "VRExp|HandTracking|PalmFacing")
	FVRExpPalmFacingRecognitionSettings GetPalmFacingRecognitionSettings() const;

	UFUNCTION(BlueprintCallable, Category = "VRExp|HandTracking|PalmFacing")
	void SetPalmFacingRecognitionSettings(const FVRExpPalmFacingRecognitionSettings& NewSettings);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|Gesture")
	FVRExpHandGestureRecognitionSettings GestureRecognitionSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|HandTracking|PalmFacing")
	FVRExpPalmFacingRecognitionSettings PalmFacingRecognitionSettings;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|HandTracking|Gesture")
	FVRExpHandGestureHandEvent OnGestureStarted;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|HandTracking|Gesture")
	FVRExpHandGestureHandEvent OnGestureEnded;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|HandTracking|Gesture")
	FVRExpHandGestureHandEvent OnGestureAxisChanged;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|HandTracking|PalmFacing")
	FVRExpPalmFacingHandEvent OnPalmFacingStarted;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|HandTracking|PalmFacing")
	FVRExpPalmFacingHandEvent OnPalmFacingEnded;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|HandTracking|PalmFacing")
	FVRExpPalmFacingHandEvent OnPalmFacingAxisChanged;

private:
	struct FGestureActiveState
	{
		bool bPinchGestureActive = false;
		bool bFistGestureActive = false;
		bool bOpenPalmGestureActive = false;
	};

	struct FPalmFacingActiveState
	{
		bool bPalmUpActive = false;
		bool bPalmDownActive = false;
	};

	void UpdateTrackedHand(EVRExpHandType HandType);
	void ApplyGestureAxisValues(EVRExpHandType HandType, const FVRExpHandGestureAxisValues& OldAxisValues, const FVRExpHandGestureAxisValues& NewAxisValues);
	void UpdateGestureState(EVRExpHandType HandType, EVRExpHandGesture Gesture, float NewAxisValue);
	bool IsGestureActiveInternal(EVRExpHandType HandType, EVRExpHandGesture Gesture) const;
	void SetGestureActiveInternal(EVRExpHandType HandType, EVRExpHandGesture Gesture, bool bActive);

	void ApplyPalmFacingAxisValues(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, const FVRExpPalmFacingAxisValues& OldAxisValues, const FVRExpPalmFacingAxisValues& NewAxisValues);
	void UpdatePalmFacingState(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction, float NewAxisValue);
	bool IsPalmFacingActiveInternal(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction) const;
	void SetPalmFacingActiveInternal(EVRExpHandType HandType, EVRExpHandTrackingSpace Space, EVRExpPalmFacingDirection Direction, bool bActive);

	FVRExpTrackedHandState& GetMutableTrackedHandState(EVRExpHandType HandType);
	const FVRExpTrackedHandState& GetTrackedHandStateRef(EVRExpHandType HandType) const;
	FGestureActiveState& GetMutableGestureActiveState(EVRExpHandType HandType);
	const FGestureActiveState& GetGestureActiveStateRef(EVRExpHandType HandType) const;
	FVRExpPalmFacingAxisValues& GetMutablePalmFacingAxisValues(EVRExpHandType HandType, EVRExpHandTrackingSpace Space);
	const FVRExpPalmFacingAxisValues& GetPalmFacingAxisValuesRef(EVRExpHandType HandType, EVRExpHandTrackingSpace Space) const;
	FPalmFacingActiveState& GetMutablePalmFacingActiveState(EVRExpHandType HandType, EVRExpHandTrackingSpace Space);
	const FPalmFacingActiveState& GetPalmFacingActiveStateRef(EVRExpHandType HandType, EVRExpHandTrackingSpace Space) const;

	static EControllerHand ToControllerHand(EVRExpHandType HandType);

	FVRExpTrackedHandState LeftHandState;
	FVRExpTrackedHandState RightHandState;
	FGestureActiveState LeftGestureActiveState;
	FGestureActiveState RightGestureActiveState;
	FVRExpPalmFacingAxisValues LeftWorldPalmFacingAxisValues;
	FVRExpPalmFacingAxisValues LeftXRPalmFacingAxisValues;
	FVRExpPalmFacingAxisValues RightWorldPalmFacingAxisValues;
	FVRExpPalmFacingAxisValues RightXRPalmFacingAxisValues;
	FPalmFacingActiveState LeftWorldPalmFacingActiveState;
	FPalmFacingActiveState LeftXRPalmFacingActiveState;
	FPalmFacingActiveState RightWorldPalmFacingActiveState;
	FPalmFacingActiveState RightXRPalmFacingActiveState;
};
