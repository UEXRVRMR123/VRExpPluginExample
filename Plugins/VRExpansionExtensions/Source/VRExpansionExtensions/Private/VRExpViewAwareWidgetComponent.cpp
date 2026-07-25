// Copyright ONEKLAB. All Rights Reserved.

#include "VRExpViewAwareWidgetComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UVRExpViewAwareWidgetComponent::UVRExpViewAwareWidgetComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetTickWhenOffscreen(true);
}

void UVRExpViewAwareWidgetComponent::OnRegister()
{
	Super::OnRegister();

	// Keep world-space widgets redrawing when occlusion-query visibility feedback is disabled.
	SetTickWhenOffscreen(true);

	bIsInPlayerView = true;
	PlayerViewCheckAccumulator = PlayerViewCheckInterval;
	TimeOutsidePlayerView = 0.0f;
}

void UVRExpViewAwareWidgetComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	if (bOnlyUpdateInPlayerView)
	{
		PlayerViewCheckAccumulator += DeltaTime;

		const float CheckInterval = FMath::Max(0.0f, PlayerViewCheckInterval);
		if (CheckInterval <= 0.0f || PlayerViewCheckAccumulator >= CheckInterval)
		{
			const float TimeSinceLastCheck = PlayerViewCheckAccumulator;
			PlayerViewCheckAccumulator = 0.0f;

			if (CalculateIsInPlayerView())
			{
				bIsInPlayerView = true;
				TimeOutsidePlayerView = 0.0f;
			}
			else
			{
				TimeOutsidePlayerView += FMath::Max(DeltaTime, TimeSinceLastCheck);
				bIsInPlayerView = TimeOutsidePlayerView < FMath::Max(0.0f, PlayerViewExitGracePeriod);
			}
		}
	}
	else
	{
		bIsInPlayerView = true;
		PlayerViewCheckAccumulator = 0.0f;
		TimeOutsidePlayerView = 0.0f;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool UVRExpViewAwareWidgetComponent::ShouldDrawWidget() const
{
	if (bOnlyUpdateInPlayerView && !bIsInPlayerView)
	{
		return false;
	}

	return Super::ShouldDrawWidget();
}

bool UVRExpViewAwareWidgetComponent::CalculateIsInPlayerView() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	const APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!IsValid(PlayerController))
	{
		// Fail open so a temporarily unavailable local view never freezes the widget.
		return true;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector ToWidget = Bounds.Origin - ViewLocation;
	const double DistanceSquared = ToWidget.SizeSquared();
	const double BoundsRadius = FMath::Max(0.0, Bounds.SphereRadius);

	if (DistanceSquared <= FMath::Square(BoundsRadius) || DistanceSquared <= UE_SMALL_NUMBER)
	{
		return true;
	}

	const double Distance = FMath::Sqrt(DistanceSquared);
	const FVector DirectionToWidget = ToWidget / Distance;
	const double ViewDot = FVector::DotProduct(ViewRotation.Vector(), DirectionToWidget);

	// Expand the cone by the component's angular radius so partially visible widgets keep updating.
	const double AngularRadius = FMath::Asin(FMath::Clamp(BoundsRadius / Distance, 0.0, 1.0));
	const double HalfAngle = FMath::DegreesToRadians(FMath::Clamp(PlayerViewHalfAngleDegrees, 1.0f, 89.0f));
	const double MaximumCenterAngle = FMath::Min(HalfAngle + AngularRadius, UE_PI);

	return ViewDot >= FMath::Cos(MaximumCenterAngle);
}
