#pragma once

#include "CoreMinimal.h"
#include "UI/VRExpUIInfoTypes.h"

class AActor;
class USceneComponent;

/** Stateless anchor and transform input shared by runtime presentation and editor preview. */
struct VREXPANSIONEXTENSIONS_API FVRExpUIInfoPlacementRequest
{
    const FVRExpUIInfoPresentationSettings *Settings = nullptr;
    AActor *DetectableOwner = nullptr;
    USceneComponent *MotionUpdatedComponent = nullptr;
    USceneComponent *CameraComponent = nullptr;
    bool bHasViewTransform = false;
    FTransform ViewTransform = FTransform::Identity;
    bool bHasFallbackFacingRotation = false;
    FQuat FallbackFacingRotation = FQuat::Identity;
};

struct VREXPANSIONEXTENSIONS_API FVRExpUIInfoResolvedAnchor
{
    FTransform WorldTransform = FTransform::Identity;
    TWeakObjectPtr<AActor> Actor;
    TWeakObjectPtr<USceneComponent> Component;
    FName SocketName = NAME_None;
    bool bUsedPlayerViewPoint = false;
};

struct VREXPANSIONEXTENSIONS_API FVRExpUIInfoPlacementResult
{
    FVRExpUIInfoResolvedAnchor Anchor;
    FTransform TargetWorldTransform = FTransform::Identity;
    bool bHasFacingRotation = false;
    FQuat FacingRotation = FQuat::Identity;
    bool bUsedFallbackFacingRotation = false;
    bool bUsedDefaultFacingAxes = false;
};

/**
 * Shared, stateless UI placement solver.
 * Every value required by the algorithm is supplied in the request, so editor
 * preview and runtime presentation produce the same transform.
 */
class VREXPANSIONEXTENSIONS_API FVRExpUIInfoPlacementResolver
{
public:
    static void NormalizeSettings(FVRExpUIInfoPresentationSettings &Settings);

    static EVRExpUIInfoBindingMode GetEffectiveBindingMode(
        const FVRExpUIInfoPresentationSettings &Settings);

    static bool Resolve(
        const FVRExpUIInfoPlacementRequest &Request,
        FVRExpUIInfoPlacementResult &OutResult,
        FString &OutFailureReason);

    static bool ResolveBaseTarget(
        const FVRExpUIInfoPlacementRequest &Request,
        FVRExpUIInfoPlacementResult &OutResult,
        FString &OutFailureReason);

    static bool ResolveAnchor(
        const FVRExpUIInfoPlacementRequest &Request,
        FVRExpUIInfoResolvedAnchor &OutAnchor,
        FString &OutFailureReason);

    static bool ResolveComponentAnchor(
        AActor *DetectableOwner,
        const FComponentReference &ComponentReference,
        FName SocketName,
        FVRExpUIInfoResolvedAnchor &OutAnchor,
        FString &OutFailureReason);

    static FVector GetAxisVector(EVRExpUIInfoAxisDirection Axis);

    static bool ApplyOrientation(
        const FVRExpUIInfoPlacementRequest &Request,
        const FVRExpUIInfoResolvedAnchor &Anchor,
        FTransform &InOutTargetWorldTransform,
        FVRExpUIInfoPlacementResult &OutResult,
        FString &OutFailureReason);

private:
    static bool ApplyPositionMode(
        const FVRExpUIInfoPlacementRequest &Request,
        const FVRExpUIInfoResolvedAnchor &Anchor,
        FTransform &InOutTargetWorldTransform,
        FString &OutFailureReason);

    static bool ResolveUpVector(
        EVRExpUIInfoUpReference UpReference,
        const FVRExpUIInfoPlacementRequest &Request,
        const FVRExpUIInfoResolvedAnchor &Anchor,
        FVector &OutUpVector,
        FString &OutFailureReason);
};
