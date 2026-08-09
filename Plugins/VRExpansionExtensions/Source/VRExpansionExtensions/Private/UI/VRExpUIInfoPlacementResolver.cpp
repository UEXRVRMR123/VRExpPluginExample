#include "UI/VRExpUIInfoPlacementResolver.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

void FVRExpUIInfoPlacementResolver::NormalizeSettings(
    FVRExpUIInfoPresentationSettings &Settings)
{
    if (Settings.AnchorType == EVRExpUIInfoAnchorType::World)
    {
        Settings.BindingMode = EVRExpUIInfoBindingMode::ResolveOnce;
    }
    else if (
        Settings.PositionMode ==
            EVRExpUIInfoPositionMode::BetweenAnchorAndCamera &&
        Settings.BindingMode == EVRExpUIInfoBindingMode::Attach)
    {
        Settings.BindingMode = EVRExpUIInfoBindingMode::Follow;
    }
}

EVRExpUIInfoBindingMode
FVRExpUIInfoPlacementResolver::GetEffectiveBindingMode(
    const FVRExpUIInfoPresentationSettings &Settings)
{
    if (Settings.AnchorType == EVRExpUIInfoAnchorType::World)
    {
        return EVRExpUIInfoBindingMode::ResolveOnce;
    }
    if (Settings.PositionMode ==
            EVRExpUIInfoPositionMode::BetweenAnchorAndCamera &&
        Settings.BindingMode == EVRExpUIInfoBindingMode::Attach)
    {
        return EVRExpUIInfoBindingMode::Follow;
    }
    return Settings.BindingMode;
}

bool FVRExpUIInfoPlacementResolver::Resolve(
    const FVRExpUIInfoPlacementRequest &Request,
    FVRExpUIInfoPlacementResult &OutResult,
    FString &OutFailureReason)
{
    if (!ResolveBaseTarget(Request, OutResult, OutFailureReason))
    {
        return false;
    }

    return ApplyOrientation(
        Request,
        OutResult.Anchor,
        OutResult.TargetWorldTransform,
        OutResult,
        OutFailureReason);
}

bool FVRExpUIInfoPlacementResolver::ResolveBaseTarget(
    const FVRExpUIInfoPlacementRequest &Request,
    FVRExpUIInfoPlacementResult &OutResult,
    FString &OutFailureReason)
{
    OutResult = FVRExpUIInfoPlacementResult();
    OutFailureReason.Reset();
    if (!Request.Settings)
    {
        OutFailureReason = TEXT("Presentation settings are unavailable.");
        return false;
    }

    if (!ResolveAnchor(Request, OutResult.Anchor, OutFailureReason))
    {
        return false;
    }

    const FVRExpUIInfoPresentationSettings &Settings = *Request.Settings;
    if (Settings.AnchorType == EVRExpUIInfoAnchorType::World)
    {
        OutResult.TargetWorldTransform = Settings.WorldTransform;
    }
    else
    {
        OutResult.TargetWorldTransform =
            Settings.RelativeOffset * OutResult.Anchor.WorldTransform;
        if (Settings.ScaleMode == EVRExpUIInfoScaleMode::KeepWorldScale)
        {
            OutResult.TargetWorldTransform.SetScale3D(
                Settings.RelativeOffset.GetScale3D());
        }
    }

    return ApplyPositionMode(
            Request,
            OutResult.Anchor,
            OutResult.TargetWorldTransform,
            OutFailureReason);
}

bool FVRExpUIInfoPlacementResolver::ResolveAnchor(
    const FVRExpUIInfoPlacementRequest &Request,
    FVRExpUIInfoResolvedAnchor &OutAnchor,
    FString &OutFailureReason)
{
    OutAnchor = FVRExpUIInfoResolvedAnchor();
    if (!Request.Settings)
    {
        OutFailureReason = TEXT("Presentation settings are unavailable.");
        return false;
    }

    const FVRExpUIInfoPresentationSettings &Settings = *Request.Settings;
    switch (Settings.AnchorType)
    {
    case EVRExpUIInfoAnchorType::Camera:
        if (IsValid(Request.CameraComponent))
        {
            OutAnchor.Component = Request.CameraComponent;
            OutAnchor.Actor = Request.CameraComponent->GetOwner();
            OutAnchor.WorldTransform =
                Request.CameraComponent->GetComponentTransform();
            return true;
        }
        if (GetEffectiveBindingMode(Settings) ==
            EVRExpUIInfoBindingMode::Attach)
        {
            OutFailureReason =
                TEXT("Attach binding requires a valid local camera component.");
            return false;
        }
        if (!Request.bHasViewTransform)
        {
            OutFailureReason = TEXT("The local player view is unavailable.");
            return false;
        }
        OutAnchor.WorldTransform = Request.ViewTransform;
        OutAnchor.bUsedPlayerViewPoint = true;
        return true;

    case EVRExpUIInfoAnchorType::DetectableOwner:
        if (!IsValid(Request.DetectableOwner))
        {
            OutFailureReason = TEXT("The detectable owner is unavailable.");
            return false;
        }
        OutAnchor.Actor = Request.DetectableOwner;
        OutAnchor.Component = Request.DetectableOwner->GetRootComponent();
        OutAnchor.WorldTransform =
            Request.DetectableOwner->GetActorTransform();
        if (GetEffectiveBindingMode(Settings) ==
                EVRExpUIInfoBindingMode::Attach &&
            !OutAnchor.Component.IsValid())
        {
            OutFailureReason =
                TEXT("Attach binding requires the detectable owner to have a root component.");
            return false;
        }
        return true;

    case EVRExpUIInfoAnchorType::MotionUpdatedComponent:
        if (!IsValid(Request.MotionUpdatedComponent))
        {
            OutFailureReason =
                TEXT("The authoritative Grabbable Motion Component or its UpdatedComponent is unavailable.");
            return false;
        }
        OutAnchor.Actor = Request.MotionUpdatedComponent->GetOwner();
        OutAnchor.Component = Request.MotionUpdatedComponent;
        OutAnchor.WorldTransform =
            Request.MotionUpdatedComponent->GetComponentTransform();
        return true;

    case EVRExpUIInfoAnchorType::Component:
        return ResolveComponentAnchor(
            Request.DetectableOwner,
            Settings.AttachComponent,
            Settings.AttachSocketName,
            OutAnchor,
            OutFailureReason);

    case EVRExpUIInfoAnchorType::World:
        OutAnchor.WorldTransform = Settings.WorldTransform;
        return true;

    default:
        OutFailureReason = TEXT("AnchorType is invalid.");
        return false;
    }
}

bool FVRExpUIInfoPlacementResolver::ResolveComponentAnchor(
    AActor *DetectableOwner,
    const FComponentReference &ComponentReference,
    FName SocketName,
    FVRExpUIInfoResolvedAnchor &OutAnchor,
    FString &OutFailureReason)
{
    OutAnchor = FVRExpUIInfoResolvedAnchor();
    if (!IsValid(DetectableOwner))
    {
        OutFailureReason = TEXT("The detectable owner is unavailable.");
        return false;
    }

    USceneComponent *SceneComponent =
        Cast<USceneComponent>(ComponentReference.GetComponent(DetectableOwner));
    if (!IsValid(SceneComponent))
    {
        OutFailureReason =
            TEXT("The configured anchor component could not be resolved.");
        return false;
    }
    if (!SocketName.IsNone() && !SceneComponent->DoesSocketExist(SocketName))
    {
        OutFailureReason = FString::Printf(
            TEXT("Socket '%s' does not exist on component '%s'."),
            *SocketName.ToString(),
            *GetNameSafe(SceneComponent));
        return false;
    }

    OutAnchor.Actor = SceneComponent->GetOwner();
    OutAnchor.Component = SceneComponent;
    OutAnchor.SocketName = SocketName;
    OutAnchor.WorldTransform =
        SocketName.IsNone()
            ? SceneComponent->GetComponentTransform()
            : SceneComponent->GetSocketTransform(SocketName, RTS_World);
    return true;
}

bool FVRExpUIInfoPlacementResolver::ApplyPositionMode(
    const FVRExpUIInfoPlacementRequest &Request,
    const FVRExpUIInfoResolvedAnchor &Anchor,
    FTransform &InOutTargetWorldTransform,
    FString &OutFailureReason)
{
    const FVRExpUIInfoPresentationSettings &Settings = *Request.Settings;
    if (Settings.PositionMode ==
        EVRExpUIInfoPositionMode::AnchorRelative)
    {
        return true;
    }
    if (Settings.PositionMode !=
        EVRExpUIInfoPositionMode::BetweenAnchorAndCamera)
    {
        OutFailureReason = TEXT("PositionMode is invalid.");
        return false;
    }
    if (Settings.AnchorType == EVRExpUIInfoAnchorType::Camera)
    {
        OutFailureReason =
            TEXT("BetweenAnchorAndCamera requires a non-Camera anchor.");
        return false;
    }
    if (!Request.bHasViewTransform)
    {
        OutFailureReason =
            TEXT("BetweenAnchorAndCamera requires a valid local player view.");
        return false;
    }

    const FVector AnchorPoint = Anchor.WorldTransform.GetLocation();
    const FVector CameraPoint = Request.ViewTransform.GetLocation();
    const float SegmentLength =
        FVector::Distance(AnchorPoint, CameraPoint);
    const float MinAnchorDistance =
        FMath::Max(0.0f, Settings.MinDistanceFromAnchor);
    const float MinCameraDistance =
        FMath::Max(0.0f, Settings.MinDistanceFromCamera);
    if (SegmentLength <= KINDA_SMALL_NUMBER ||
        SegmentLength + KINDA_SMALL_NUMBER <
            MinAnchorDistance + MinCameraDistance)
    {
        OutFailureReason = FString::Printf(
            TEXT("Anchor-camera distance %.2f cm cannot satisfy minimum anchor/camera clearances %.2f/%.2f cm."),
            SegmentLength,
            MinAnchorDistance,
            MinCameraDistance);
        return false;
    }

    FVector PositionUp;
    if (!ResolveUpVector(
            Settings.PositionUpReference,
            Request,
            Anchor,
            PositionUp,
            OutFailureReason))
    {
        return false;
    }

    const float MinimumRatio = MinAnchorDistance / SegmentLength;
    const float MaximumRatio =
        1.0f - MinCameraDistance / SegmentLength;
    const float SafeRatio = FMath::Clamp(
        Settings.BetweenRatio,
        MinimumRatio,
        MaximumRatio);
    InOutTargetWorldTransform.SetLocation(
        FMath::Lerp(AnchorPoint, CameraPoint, SafeRatio) +
        PositionUp.GetSafeNormal(
            KINDA_SMALL_NUMBER,
            FVector::UpVector) *
            Settings.BetweenVerticalOffset);
    return true;
}

bool FVRExpUIInfoPlacementResolver::ApplyOrientation(
    const FVRExpUIInfoPlacementRequest &Request,
    const FVRExpUIInfoResolvedAnchor &Anchor,
    FTransform &InOutTargetWorldTransform,
    FVRExpUIInfoPlacementResult &OutResult,
    FString &OutFailureReason)
{
    const FVRExpUIInfoPresentationSettings &Settings = *Request.Settings;
    if (Settings.OrientationMode ==
        EVRExpUIInfoOrientationMode::InheritAnchor)
    {
        return true;
    }
    if (Settings.OrientationMode ==
        EVRExpUIInfoOrientationMode::FixedWorld)
    {
        InOutTargetWorldTransform.SetRotation(
            Settings.FixedWorldRotation.Quaternion());
        return true;
    }
    if (Settings.OrientationMode !=
            EVRExpUIInfoOrientationMode::FaceCamera &&
        Settings.OrientationMode !=
            EVRExpUIInfoOrientationMode::FaceCameraYawOnly)
    {
        OutFailureReason = TEXT("OrientationMode is invalid.");
        return false;
    }
    if (!Request.bHasViewTransform)
    {
        OutFailureReason =
            TEXT("Camera-facing orientation requires a valid local player view.");
        return false;
    }

    FVector DesiredUp;
    if (!ResolveUpVector(
            Settings.UpReference,
            Request,
            Anchor,
            DesiredUp,
            OutFailureReason))
    {
        return false;
    }
    DesiredUp = DesiredUp.GetSafeNormal();
    if (DesiredUp.IsNearlyZero())
    {
        DesiredUp = FVector::UpVector;
    }

    FVector DesiredFacing =
        Request.ViewTransform.GetLocation() -
        InOutTargetWorldTransform.GetLocation();
    if (Settings.OrientationMode ==
        EVRExpUIInfoOrientationMode::FaceCameraYawOnly)
    {
        DesiredFacing =
            FVector::VectorPlaneProject(DesiredFacing, DesiredUp);
    }
    DesiredFacing = DesiredFacing.GetSafeNormal();
    if (DesiredFacing.IsNearlyZero())
    {
        OutFailureReason =
            TEXT("Camera-facing direction is degenerate; the fallback rotation is retained.");
        if (Request.bHasFallbackFacingRotation)
        {
            InOutTargetWorldTransform.SetRotation(
                Request.FallbackFacingRotation);
            OutResult.bUsedFallbackFacingRotation = true;
            OutResult.bHasFacingRotation = true;
            OutResult.FacingRotation =
                Request.FallbackFacingRotation;
        }
        return true;
    }

    FVector LocalFacing = GetAxisVector(Settings.LocalFacingAxis);
    FVector LocalUp = GetAxisVector(Settings.LocalUpAxis);
    if (FMath::Abs(FVector::DotProduct(LocalFacing, LocalUp)) >
        1.0f - KINDA_SMALL_NUMBER)
    {
        LocalFacing = FVector::ForwardVector;
        LocalUp = FVector::UpVector;
        OutResult.bUsedDefaultFacingAxes = true;
    }

    FVector DesiredProjectedUp =
        FVector::VectorPlaneProject(DesiredUp, DesiredFacing).GetSafeNormal();
    if (DesiredProjectedUp.IsNearlyZero())
    {
        DesiredProjectedUp =
            FVector::VectorPlaneProject(FVector::UpVector, DesiredFacing)
                .GetSafeNormal();
    }
    if (DesiredProjectedUp.IsNearlyZero())
    {
        DesiredProjectedUp =
            FVector::VectorPlaneProject(
                Request.ViewTransform.GetUnitAxis(EAxis::Y),
                DesiredFacing)
                .GetSafeNormal();
    }
    if (DesiredProjectedUp.IsNearlyZero())
    {
        OutFailureReason =
            TEXT("A stable camera-facing up direction could not be constructed.");
        return false;
    }

    const FQuat FacingAlignment =
        FQuat::FindBetweenNormals(LocalFacing, DesiredFacing);
    const FVector AlignedLocalUp =
        FVector::VectorPlaneProject(
            FacingAlignment.RotateVector(LocalUp),
            DesiredFacing)
            .GetSafeNormal();
    if (AlignedLocalUp.IsNearlyZero())
    {
        OutFailureReason =
            TEXT("The configured local UI axes cannot construct a valid facing basis.");
        return false;
    }

    const float TwistAngle = FMath::Atan2(
        FVector::DotProduct(
            DesiredFacing,
            FVector::CrossProduct(
                AlignedLocalUp,
                DesiredProjectedUp)),
        FVector::DotProduct(
            AlignedLocalUp,
            DesiredProjectedUp));
    const FQuat FacingRotation =
        (FQuat(DesiredFacing, TwistAngle) * FacingAlignment)
            .GetNormalized();
    const FQuat FinalRotation =
        (FacingRotation *
         Settings.FacingRotationOffset.Quaternion())
            .GetNormalized();
    InOutTargetWorldTransform.SetRotation(FinalRotation);
    OutResult.bHasFacingRotation = true;
    OutResult.FacingRotation = FinalRotation;
    return true;
}

bool FVRExpUIInfoPlacementResolver::ResolveUpVector(
    EVRExpUIInfoUpReference UpReference,
    const FVRExpUIInfoPlacementRequest &Request,
    const FVRExpUIInfoResolvedAnchor &Anchor,
    FVector &OutUpVector,
    FString &OutFailureReason)
{
    switch (UpReference)
    {
    case EVRExpUIInfoUpReference::WorldUp:
        OutUpVector = FVector::UpVector;
        return true;
    case EVRExpUIInfoUpReference::CameraUp:
        if (!Request.bHasViewTransform)
        {
            OutFailureReason =
                TEXT("Camera Up requires a valid local player view.");
            return false;
        }
        OutUpVector = Request.ViewTransform.GetUnitAxis(EAxis::Z);
        return true;
    case EVRExpUIInfoUpReference::AnchorUp:
        OutUpVector = Anchor.WorldTransform.GetUnitAxis(EAxis::Z);
        return true;
    default:
        OutFailureReason = TEXT("UpReference is invalid.");
        return false;
    }
}

FVector FVRExpUIInfoPlacementResolver::GetAxisVector(
    EVRExpUIInfoAxisDirection Axis)
{
    switch (Axis)
    {
    case EVRExpUIInfoAxisDirection::PositiveX:
        return FVector::ForwardVector;
    case EVRExpUIInfoAxisDirection::NegativeX:
        return -FVector::ForwardVector;
    case EVRExpUIInfoAxisDirection::PositiveY:
        return FVector::RightVector;
    case EVRExpUIInfoAxisDirection::NegativeY:
        return -FVector::RightVector;
    case EVRExpUIInfoAxisDirection::PositiveZ:
        return FVector::UpVector;
    case EVRExpUIInfoAxisDirection::NegativeZ:
        return -FVector::UpVector;
    default:
        return FVector::ForwardVector;
    }
}
