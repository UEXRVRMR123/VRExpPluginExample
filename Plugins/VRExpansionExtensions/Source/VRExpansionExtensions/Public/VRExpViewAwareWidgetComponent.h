// Copyright ONEKLAB. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "VRExpViewAwareWidgetComponent.generated.h"

/**
 * World-space widget component that keeps its render target updating when
 * renderer visibility feedback is unavailable, with optional player-view redraw culling.
 */
UCLASS(ClassGroup = (VRExp), meta = (BlueprintSpawnableComponent, DisplayName = "VRExp View Aware Widget Component"))
class VREXPANSIONEXTENSIONS_API UVRExpViewAwareWidgetComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UVRExpViewAwareWidgetComponent(const FObjectInitializer& ObjectInitializer);

	/** Only redraw while the component bounds intersect the player's view cone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Widget Update")
	bool bOnlyUpdateInPlayerView = true;

	/** Half angle of the conservative player view cone used for redraw culling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Widget Update",
		meta = (EditCondition = "bOnlyUpdateInPlayerView", ClampMin = "1.0", ClampMax = "89.0", UIMin = "30.0", UIMax = "80.0", Units = "Degrees"))
	float PlayerViewHalfAngleDegrees = 60.0f;

	/** Interval between player-view checks. Zero checks every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Widget Update",
		meta = (EditCondition = "bOnlyUpdateInPlayerView", ClampMin = "0.0", UIMin = "0.0", UIMax = "0.5", Units = "Seconds"))
	float PlayerViewCheckInterval = 0.05f;

	/** Time to keep redrawing after leaving the view cone to avoid edge flicker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Widget Update",
		meta = (EditCondition = "bOnlyUpdateInPlayerView", ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", Units = "Seconds"))
	float PlayerViewExitGracePeriod = 0.15f;

	UFUNCTION(BlueprintPure, Category = "VRExp|Widget Update")
	bool IsInPlayerView() const { return bIsInPlayerView; }

protected:
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual bool ShouldDrawWidget() const override;

private:
	bool CalculateIsInPlayerView() const;

	bool bIsInPlayerView = true;
	float PlayerViewCheckAccumulator = 0.0f;
	float TimeOutsidePlayerView = 0.0f;
};
