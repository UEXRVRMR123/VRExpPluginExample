#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Detection/VRExpDetectableTypes.h"
#include "VRExpUIInfoTypes.generated.h"

class AActor;
class UCurveFloat;
class USceneComponent;

UENUM(BlueprintType)
enum class EVRExpUIInfoAnchorType : uint8
{
    Camera UMETA(DisplayName = "Camera",
                 ToolTip = "Use the matching local player's camera as the placement anchor."),
    DetectableOwner UMETA(DisplayName = "Detectable Owner",
                           ToolTip = "Use the Actor that owns the detectable component as the placement anchor."),
    MotionUpdatedComponent UMETA(
        DisplayName = "Motion Updated Component",
        ToolTip = "Use the authoritative Grabbable Motion Component's UpdatedComponent as the placement anchor."),
    Component UMETA(DisplayName = "Component",
                    ToolTip = "Use a configured component or socket on the detectable owner as the placement anchor."),
    World UMETA(DisplayName = "World",
                ToolTip = "Use an explicit world transform. Binding is always resolved once for this anchor.")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoPositionMode : uint8
{
    AnchorRelative UMETA(
        DisplayName = "Anchor Relative",
        ToolTip = "Use the configured transform relative to the selected anchor."),
    BetweenAnchorAndCamera UMETA(
        DisplayName = "Between Anchor And Camera",
        ToolTip = "Place the UI between the resolved anchor point and the matching local camera.")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoBindingMode : uint8
{
    ResolveOnce UMETA(DisplayName = "Resolve Once",
                      ToolTip = "Resolve one world-space target when the presentation starts, then stop following the anchor."),
    Follow UMETA(DisplayName = "Follow",
                 ToolTip = "Keep the UI Actor detached and recompute its world-space target every presentation tick."),
    Attach UMETA(DisplayName = "Attach",
                 ToolTip = "Attach the UI Actor to the resolved anchor component or socket and apply a relative offset.")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoOrientationMode : uint8
{
    InheritAnchor UMETA(DisplayName = "Inherit Anchor",
                        ToolTip = "Use the anchor rotation plus the configured transform rotation."),
    FaceCamera UMETA(DisplayName = "Face Camera",
                     ToolTip = "Rotate the configured local facing axis toward the local camera."),
    FaceCameraYawOnly UMETA(DisplayName = "Face Camera (Planar)",
                            ToolTip = "Face the local camera after projecting the view direction onto the configured up-reference plane."),
    FixedWorld UMETA(DisplayName = "Fixed World Rotation",
                     ToolTip = "Use the configured absolute world rotation without inheriting anchor rotation.")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoAxisDirection : uint8
{
    PositiveX UMETA(DisplayName = "+X"),
    NegativeX UMETA(DisplayName = "-X"),
    PositiveY UMETA(DisplayName = "+Y"),
    NegativeY UMETA(DisplayName = "-Y"),
    PositiveZ UMETA(DisplayName = "+Z"),
    NegativeZ UMETA(DisplayName = "-Z")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoUpReference : uint8
{
    WorldUp UMETA(DisplayName = "World Up",
                  ToolTip = "Use world +Z as the desired up direction."),
    CameraUp UMETA(DisplayName = "Camera Up",
                   ToolTip = "Use the matching local camera's up direction."),
    AnchorUp UMETA(DisplayName = "Anchor Up",
                   ToolTip = "Use the resolved anchor's up direction.")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoScaleMode : uint8
{
    KeepWorldScale UMETA(DisplayName = "Keep World Scale",
                         ToolTip = "Treat the configured scale as world scale and do not inherit parent scale."),
    InheritAnchorScale UMETA(DisplayName = "Inherit Anchor Scale",
                             ToolTip = "Treat the configured scale as relative scale and inherit anchor scale.")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoMissingAnchorPolicy : uint8
{
    HideAndRetry UMETA(DisplayName = "Hide And Retry",
                       ToolTip = "Hide the UI, leave exclusive arbitration, and retry until the target becomes available."),
    KeepLastAndRetry UMETA(DisplayName = "Keep Last And Retry",
                           ToolTip = "Keep the last valid transform and visibility while retrying the missing target.")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoPresentationPolicy : uint8
{
    Independent UMETA(DisplayName = "Independent",
                      ToolTip = "Display independently without competing with other UI Info presentations."),
    ExclusivePerLocalPlayer UMETA(DisplayName = "Exclusive Per Local Player",
                                  ToolTip = "Compete for one exclusive UI Info presentation slot per local player.")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoEntryMode : uint8
{
    AtTarget UMETA(DisplayName = "At Presentation Target",
                   ToolTip = "Spawn directly at the currently resolved presentation target."),
    AtOwner UMETA(DisplayName = "At Detectable Owner",
                  ToolTip = "Spawn from a transform relative to the detectable owner."),
    AtComponent UMETA(DisplayName = "At Component",
                      ToolTip = "Spawn from a transform relative to a configured component or socket."),
    AtWorldTransform UMETA(DisplayName = "At World Transform",
                           ToolTip = "Spawn at an explicit world transform.")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoInteractionSource : uint8
{
    None UMETA(Hidden),
    HeadDetection UMETA(DisplayName = "Head Detection"),
    Grip UMETA(DisplayName = "Grip"),
    GracePeriod UMETA(DisplayName = "Grace Period"),
    Motion UMETA(DisplayName = "Motion"),
    Release UMETA(DisplayName = "Release")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoHeadPresentationMode : uint8
{
    WhileDetected UMETA(
        DisplayName = "While Detected",
        ToolTip = "Preserve the existing behavior: present while Head Detection remains eligible, then use the configured inactivity delays."),
    TimedWithCooldown UMETA(
        DisplayName = "Timed With Cooldown",
        ToolTip = "Start a fixed visible window when the Head presentation first becomes visible, then suppress Head presentation during a cooldown.")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoHeadDetectionLossBehavior : uint8
{
    KeepVisibleUntilDurationExpires UMETA(
        DisplayName = "Keep Visible Until Duration Expires",
        ToolTip = "Keep the timed Head presentation eligible until its visible duration expires, even after Head Detection ends."),
    HideAndStartCooldown UMETA(
        DisplayName = "Hide And Start Cooldown",
        ToolTip = "End the timed Head presentation and start cooldown as soon as Head Detection ends.")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoHeadCooldownCompletionBehavior : uint8
{
    ReactivateIfStillDetected UMETA(
        DisplayName = "Reactivate If Still Detected",
        ToolTip = "When cooldown ends, immediately re-evaluate Head presentation if Head Detection is still active."),
    RequireNewDetection UMETA(
        DisplayName = "Require New Detection",
        ToolTip = "When cooldown ends while Head Detection is active, require Head Detection to end and begin again before presenting.")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoReleaseBehavior : uint8
{
    DeferToCurrentInteraction UMETA(
        DisplayName = "Defer To Current Interaction",
        ToolTip = "Preserve legacy behavior: after the final grip, select Head Detection or Motion without an independent Release source."),
    HideDuringRelease UMETA(
        DisplayName = "Hide During Release",
        ToolTip = "Keep the spawned UI Actor alive but hidden until the release phase ends."),
    KeepGripPresentation UMETA(
        DisplayName = "Keep Grip Presentation",
        ToolTip = "Publish Release as the interaction source while continuing to use Grip Presentation Settings."),
    UseReleasePresentation UMETA(
        DisplayName = "Use Release Presentation",
        ToolTip = "Publish Release as the interaction source and use independent Release Presentation Settings.")
};

FORCEINLINE EVRExpUIInfoInteractionSource
ResolveVRExpUIInfoInteractionSource(
    bool bIsGripped,
    EVRExpGrabbableMotionPhase MotionPhase,
    EVRExpUIInfoReleaseBehavior ReleaseBehavior,
    bool bIsHeadDetected,
    bool bEnableMotionOnlyPresentation,
    bool bHasMotionComponent)
{
    if (bIsGripped)
    {
        return EVRExpUIInfoInteractionSource::Grip;
    }
    if (MotionPhase == EVRExpGrabbableMotionPhase::Releasing &&
        ReleaseBehavior !=
            EVRExpUIInfoReleaseBehavior::DeferToCurrentInteraction)
    {
        return EVRExpUIInfoInteractionSource::Release;
    }
    if (bIsHeadDetected)
    {
        return EVRExpUIInfoInteractionSource::HeadDetection;
    }
    if (bEnableMotionOnlyPresentation && bHasMotionComponent)
    {
        return EVRExpUIInfoInteractionSource::Motion;
    }
    return EVRExpUIInfoInteractionSource::None;
}

UENUM(BlueprintType)
enum class EVRExpUIInfoEditorPreviewMode : uint8
{
    Disabled UMETA(
        DisplayName = "Disabled",
        ToolTip = "Do not create or draw editor preview data."),
    TransformMarker UMETA(
        DisplayName = "Transform Marker",
        ToolTip = "Draw a lightweight coordinate system, bounds marker, and source label."),
    UIActor UMETA(
        DisplayName = "UI Actor",
        ToolTip = "Create a transient editor-only Child Actor preview. This explicitly runs the UI Actor Blueprint Construction Script.")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoEditorPreviewSource : uint8
{
    Head UMETA(DisplayName = "Head"),
    Grip UMETA(DisplayName = "Grip"),
    Release UMETA(DisplayName = "Release"),
    Motion UMETA(DisplayName = "Motion")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoEditorPreviewStatus : uint8
{
    Disabled UMETA(DisplayName = "Disabled"),
    Active UMETA(DisplayName = "Active"),
    MissingUIActorClass UMETA(DisplayName = "Missing UI Actor Class"),
    NoMatchingViewport UMETA(DisplayName = "No Matching Viewport"),
    AmbiguousMotionSource UMETA(DisplayName = "Ambiguous Motion Source"),
    PlacementFailed UMETA(DisplayName = "Placement Failed")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoVisibilityReason : uint8
{
    None UMETA(Hidden),
    Activated UMETA(DisplayName = "Activated"),
    RestoredAfterPriorityLoss UMETA(DisplayName = "Restored After Priority Loss"),
    PriorityDisplaced UMETA(DisplayName = "Priority Displaced"),
    InactivityTimeout UMETA(DisplayName = "Inactivity Timeout"),
    PlacementTargetUnavailable UMETA(DisplayName = "Placement Target Unavailable"),
    Deinitialized UMETA(DisplayName = "Deinitialized"),
    HiddenDuringRelease UMETA(DisplayName = "Hidden During Release"),
    HeadVisibleDurationExpired UMETA(DisplayName = "Head Visible Duration Expired"),
    HeadDetectionLost UMETA(DisplayName = "Head Detection Lost")
};

UENUM(BlueprintType)
enum class EVRExpUIInfoExitMode : uint8
{
    Immediate UMETA(
        DisplayName = "Immediate",
        ToolTip = "Hide the UI Actor immediately without an exit transition."),
    Transition UMETA(
        DisplayName = "Transition",
        ToolTip = "Keep the UI Actor rendered while the configured exit transition completes.")
};

USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpUIInfoExitSettings
{
    GENERATED_BODY()

    FVRExpUIInfoExitSettings()
        : Mode(EVRExpUIInfoExitMode::Transition),
          DurationSeconds(0.2f),
          bFadeWidgetOpacity(true),
          bApplyRelativeTransform(true),
          RelativeTransform(FQuat::Identity, FVector::ZeroVector, FVector(0.9f)),
          EaseExponent(2.0f),
          InterpolationCurve(nullptr)
    {
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit",
              meta = (DisplayName = "Exit Mode",
                      ToolTip = "Hide immediately or play the configured exit transition."))
    EVRExpUIInfoExitMode Mode;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit",
              meta = (DisplayName = "Exit Duration",
                      EditCondition = "Mode == EVRExpUIInfoExitMode::Transition",
                      EditConditionHides,
                      ClampMin = "0.0",
                      UIMin = "0.0",
                      Units = "Seconds",
                      ToolTip = "Duration of the exit transition. Zero falls back to immediate hiding."))
    float DurationSeconds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit",
              meta = (DisplayName = "Fade Widget Opacity",
                      EditCondition = "Mode == EVRExpUIInfoExitMode::Transition",
                      EditConditionHides,
                      ToolTip = "Fade the root User Widget of every Widget Component while preserving its original render opacity."))
    bool bFadeWidgetOpacity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit",
              meta = (DisplayName = "Apply Relative Transform",
                      EditCondition = "Mode == EVRExpUIInfoExitMode::Transition",
                      EditConditionHides,
                      ToolTip = "Animate from the current world transform toward the configured actor-local exit transform."))
    bool bApplyRelativeTransform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit",
              meta = (DisplayName = "Exit Relative Transform",
                      EditCondition = "Mode == EVRExpUIInfoExitMode::Transition && bApplyRelativeTransform",
                      EditConditionHides,
                      ToolTip = "Actor-local transform composed with the world transform captured when the exit transition begins."))
    FTransform RelativeTransform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit",
              meta = (DisplayName = "Ease Exponent",
                      EditCondition = "Mode == EVRExpUIInfoExitMode::Transition",
                      EditConditionHides,
                      ClampMin = "0.01",
                      UIMin = "0.01",
                      ToolTip = "Ease-in-out exponent used when no interpolation curve is configured."))
    float EaseExponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit",
              meta = (DisplayName = "Interpolation Curve",
                      EditCondition = "Mode == EVRExpUIInfoExitMode::Transition",
                      EditConditionHides,
                      ToolTip = "Optional normalized 0-1 curve. Curve output is clamped to 0-1."))
    TObjectPtr<UCurveFloat> InterpolationCurve;
};

USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpUIInfoPresentationSettings
{
    GENERATED_BODY()

    FVRExpUIInfoPresentationSettings()
        : AnchorType(EVRExpUIInfoAnchorType::Camera),
          BindingMode(EVRExpUIInfoBindingMode::Follow),
          PositionMode(EVRExpUIInfoPositionMode::AnchorRelative),
          RelativeOffset(FQuat::Identity, FVector(100.0f, 0.0f, 0.0f), FVector::OneVector),
          WorldTransform(FTransform::Identity),
          AttachSocketName(NAME_None),
          BetweenRatio(0.25f),
          MinDistanceFromAnchor(10.0f),
          MinDistanceFromCamera(30.0f),
          BetweenVerticalOffset(10.0f),
          PositionUpReference(EVRExpUIInfoUpReference::WorldUp),
          OrientationMode(EVRExpUIInfoOrientationMode::InheritAnchor),
          LocalFacingAxis(EVRExpUIInfoAxisDirection::PositiveX),
          LocalUpAxis(EVRExpUIInfoAxisDirection::PositiveZ),
          UpReference(EVRExpUIInfoUpReference::WorldUp),
          FacingRotationOffset(FRotator::ZeroRotator),
          FixedWorldRotation(FRotator::ZeroRotator),
          ScaleMode(EVRExpUIInfoScaleMode::KeepWorldScale),
          MissingAnchorPolicy(EVRExpUIInfoMissingAnchorPolicy::HideAndRetry),
          PresentationPolicy(EVRExpUIInfoPresentationPolicy::ExclusivePerLocalPlayer),
          Priority(0),
          bSmoothLocation(true),
          LocationInterpSpeed(8.0f),
          bSmoothRotation(true),
          RotationInterpSpeed(8.0f),
          bSmoothScale(true),
          ScaleInterpSpeed(8.0f)
    {
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor",
              meta = (DisplayName = "Anchor Type",
                      ToolTip = "Select the source used to resolve the presentation target."))
    EVRExpUIInfoAnchorType AnchorType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor",
              meta = (DisplayName = "Binding Mode",
                      EditCondition = "AnchorType != EVRExpUIInfoAnchorType::World",
                      EditConditionHides,
                      ToolTip = "Resolve once, continuously follow without attachment, or attach to the anchor hierarchy."))
    EVRExpUIInfoBindingMode BindingMode;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position",
              meta = (DisplayName = "Position Mode",
                      ToolTip = "Use the relative offset location or dynamically place the UI on the resolved anchor-to-local-camera segment. Between mode requires a non-Camera anchor."))
    EVRExpUIInfoPositionMode PositionMode;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform",
              meta = (DisplayName = "Relative Offset",
                      EditCondition = "AnchorType != EVRExpUIInfoAnchorType::World",
                      EditConditionHides,
                      ToolTip = "Transform relative to Camera, Detectable Owner, Motion Updated Component, or configured Component. Between mode ignores its translation but retains its rotation and scale."))
    FTransform RelativeOffset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform",
              meta = (DisplayName = "World Transform",
                      EditCondition = "AnchorType == EVRExpUIInfoAnchorType::World",
                      EditConditionHides,
                      ToolTip = "Absolute target transform used by the World anchor. Face Camera and Fixed World Rotation can override its rotation."))
    FTransform WorldTransform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor",
              meta = (DisplayName = "Anchor Component",
                      EditCondition = "AnchorType == EVRExpUIInfoAnchorType::Component",
                      EditConditionHides,
                      ToolTip = "Component resolved relative to the detectable owner."))
    FComponentReference AttachComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor",
              meta = (DisplayName = "Anchor Socket",
                      EditCondition = "AnchorType == EVRExpUIInfoAnchorType::Component",
                      EditConditionHides,
                      ToolTip = "Optional socket on the configured anchor component."))
    FName AttachSocketName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position",
              meta = (DisplayName = "Between Ratio",
                      EditCondition = "AnchorType != EVRExpUIInfoAnchorType::Camera && PositionMode == EVRExpUIInfoPositionMode::BetweenAnchorAndCamera",
                      EditConditionHides,
                      ClampMin = "0.01",
                      ClampMax = "0.99",
                      ToolTip = "Position along the anchor-to-camera segment before minimum-distance clamping. Zero is the anchor and one is the camera."))
    float BetweenRatio;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position",
              meta = (DisplayName = "Minimum Distance From Anchor",
                      EditCondition = "AnchorType != EVRExpUIInfoAnchorType::Camera && PositionMode == EVRExpUIInfoPositionMode::BetweenAnchorAndCamera",
                      EditConditionHides,
                      ClampMin = "0.0",
                      Units = "cm",
                      ToolTip = "Minimum world-space distance in centimeters between the UI and resolved anchor point."))
    float MinDistanceFromAnchor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position",
              meta = (DisplayName = "Minimum Distance From Camera",
                      EditCondition = "AnchorType != EVRExpUIInfoAnchorType::Camera && PositionMode == EVRExpUIInfoPositionMode::BetweenAnchorAndCamera",
                      EditConditionHides,
                      ClampMin = "0.0",
                      Units = "cm",
                      ToolTip = "Minimum world-space distance in centimeters between the UI and local camera."))
    float MinDistanceFromCamera;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position",
              meta = (DisplayName = "Between Vertical Offset",
                      EditCondition = "AnchorType != EVRExpUIInfoAnchorType::Camera && PositionMode == EVRExpUIInfoPositionMode::BetweenAnchorAndCamera",
                      EditConditionHides,
                      Units = "cm",
                      ToolTip = "Additional world-space offset in centimeters along Position Up Reference after segment placement."))
    float BetweenVerticalOffset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position",
              meta = (DisplayName = "Position Up Reference",
                      EditCondition = "AnchorType != EVRExpUIInfoAnchorType::Camera && PositionMode == EVRExpUIInfoPositionMode::BetweenAnchorAndCamera",
                      EditConditionHides,
                      ToolTip = "World, camera, or anchor up direction used by Between Vertical Offset."))
    EVRExpUIInfoUpReference PositionUpReference;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation",
              meta = (DisplayName = "Orientation Mode",
                      ToolTip = "Choose whether rotation comes from the anchor, faces the camera, or remains fixed in world space."))
    EVRExpUIInfoOrientationMode OrientationMode;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation",
              meta = (DisplayName = "Local Facing Axis",
                      EditCondition = "OrientationMode == EVRExpUIInfoOrientationMode::FaceCamera || OrientationMode == EVRExpUIInfoOrientationMode::FaceCameraYawOnly",
                      EditConditionHides,
                      ToolTip = "Local UI Actor axis that points toward the camera."))
    EVRExpUIInfoAxisDirection LocalFacingAxis;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation",
              meta = (DisplayName = "Local Up Axis",
                      EditCondition = "OrientationMode == EVRExpUIInfoOrientationMode::FaceCamera || OrientationMode == EVRExpUIInfoOrientationMode::FaceCameraYawOnly",
                      EditConditionHides,
                      ToolTip = "Local UI Actor axis aligned as closely as possible with the selected up reference."))
    EVRExpUIInfoAxisDirection LocalUpAxis;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation",
              meta = (DisplayName = "Up Reference",
                      EditCondition = "OrientationMode == EVRExpUIInfoOrientationMode::FaceCamera || OrientationMode == EVRExpUIInfoOrientationMode::FaceCameraYawOnly",
                      EditConditionHides,
                      ToolTip = "World, camera, or anchor direction used to stabilize camera-facing rotation."))
    EVRExpUIInfoUpReference UpReference;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation",
              meta = (DisplayName = "Facing Rotation Offset",
                      EditCondition = "OrientationMode == EVRExpUIInfoOrientationMode::FaceCamera || OrientationMode == EVRExpUIInfoOrientationMode::FaceCameraYawOnly",
                      EditConditionHides,
                      ToolTip = "Additional local rotation applied after building the camera-facing orientation."))
    FRotator FacingRotationOffset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation",
              meta = (DisplayName = "Fixed World Rotation",
                      EditCondition = "OrientationMode == EVRExpUIInfoOrientationMode::FixedWorld",
                      EditConditionHides,
                      ToolTip = "Absolute world rotation used while location continues to follow its configured target."))
    FRotator FixedWorldRotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale",
              meta = (DisplayName = "Scale Mode",
                      EditCondition = "AnchorType != EVRExpUIInfoAnchorType::World",
                      EditConditionHides,
                      ToolTip = "Keep a stable world scale or inherit scale from the selected anchor."))
    EVRExpUIInfoScaleMode ScaleMode;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Failure",
              meta = (DisplayName = "Missing Anchor Policy",
                      ToolTip = "Choose whether an unavailable anchor or camera-facing dependency hides the UI or preserves the last valid presentation while retrying."))
    EVRExpUIInfoMissingAnchorPolicy MissingAnchorPolicy;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Priority",
              meta = (DisplayName = "Presentation Policy",
                      ToolTip = "Display independently or compete for one exclusive slot per local player."))
    EVRExpUIInfoPresentationPolicy PresentationPolicy;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Priority",
              meta = (DisplayName = "Exclusive Priority",
                      EditCondition = "PresentationPolicy == EVRExpUIInfoPresentationPolicy::ExclusivePerLocalPlayer",
                      EditConditionHides,
                      ToolTip = "Higher values win. Equal priorities use the most recent interaction."))
    int32 Priority;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing",
              meta = (DisplayName = "Smooth Location",
                      ToolTip = "Interpolate the UI Actor's world-space location toward the resolved target each presentation tick."))
    bool bSmoothLocation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing",
              meta = (DisplayName = "Location Interp Speed",
                      ClampMin = "0.0",
                      EditCondition = "bSmoothLocation",
                      EditConditionHides,
                      ToolTip = "World-space location interpolation speed evaluated on presentation ticks. Zero snaps directly to the target."))
    float LocationInterpSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing",
              meta = (DisplayName = "Smooth Rotation",
                      ToolTip = "Interpolate the UI Actor's world-space rotation toward the resolved target each presentation tick."))
    bool bSmoothRotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing",
              meta = (DisplayName = "Rotation Interp Speed",
                      ClampMin = "0.0",
                      EditCondition = "bSmoothRotation",
                      EditConditionHides,
                      ToolTip = "World-space quaternion interpolation speed evaluated on presentation ticks. Zero snaps directly to the target."))
    float RotationInterpSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing",
              meta = (DisplayName = "Smooth Scale",
                      ToolTip = "Interpolate the UI Actor's world-space scale toward the resolved target each presentation tick."))
    bool bSmoothScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing",
              meta = (DisplayName = "Scale Interp Speed",
                      ClampMin = "0.0",
                      EditCondition = "bSmoothScale",
                      EditConditionHides,
                      ToolTip = "World-space scale interpolation speed evaluated on presentation ticks. Zero snaps directly to the target."))
    float ScaleInterpSpeed;
};

USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpUIInfoEntrySettings
{
    GENERATED_BODY()

    FVRExpUIInfoEntrySettings()
        : EntryMode(EVRExpUIInfoEntryMode::AtTarget),
          RelativeOffset(FTransform::Identity),
          WorldTransform(FTransform::Identity),
          EntrySocketName(NAME_None)
    {
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entry",
              meta = (DisplayName = "Entry Mode",
                      ToolTip = "Initial transform used only when a new UI Actor is spawned."))
    EVRExpUIInfoEntryMode EntryMode;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entry",
              meta = (DisplayName = "Relative Offset",
                      EditCondition = "EntryMode == EVRExpUIInfoEntryMode::AtOwner || EntryMode == EVRExpUIInfoEntryMode::AtComponent",
                      EditConditionHides,
                      ToolTip = "Transform relative to the detectable owner or configured entry component."))
    FTransform RelativeOffset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entry",
              meta = (DisplayName = "World Transform",
                      EditCondition = "EntryMode == EVRExpUIInfoEntryMode::AtWorldTransform",
                      EditConditionHides,
                      ToolTip = "Absolute spawn transform."))
    FTransform WorldTransform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entry",
              meta = (DisplayName = "Entry Component",
                      EditCondition = "EntryMode == EVRExpUIInfoEntryMode::AtComponent",
                      EditConditionHides,
                      ToolTip = "Component resolved relative to the detectable owner."))
    FComponentReference EntryComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entry",
              meta = (DisplayName = "Entry Socket",
                      EditCondition = "EntryMode == EVRExpUIInfoEntryMode::AtComponent",
                      EditConditionHides,
                      ToolTip = "Optional socket on the configured entry component."))
    FName EntrySocketName;
};

USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpUIInfoRuntimeDebugState
{
    GENERATED_BODY()

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpDetectableActivationMatchSource MatchedActivationSource =
        EVRExpDetectableActivationMatchSource::None;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpUIInfoInteractionSource InteractionSource = EVRExpUIInfoInteractionSource::None;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpUIInfoAnchorType AnchorType = EVRExpUIInfoAnchorType::Camera;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpUIInfoBindingMode BindingMode = EVRExpUIInfoBindingMode::Follow;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpUIInfoPositionMode PositionMode = EVRExpUIInfoPositionMode::AnchorRelative;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpUIInfoOrientationMode OrientationMode = EVRExpUIInfoOrientationMode::InheritAnchor;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpUIInfoPresentationPolicy PresentationPolicy =
        EVRExpUIInfoPresentationPolicy::ExclusivePerLocalPlayer;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    bool bAnchorResolved = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    bool bTargetResolved = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    bool bUIActorVisible = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    bool bExclusiveClaimEligible = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    bool bExclusivePresentationGranted = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    int32 ExclusivePriority = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpDetectableGripSource GripSource = EVRExpDetectableGripSource::None;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpGrabbableMotionState MotionState = EVRExpGrabbableMotionState::Idle;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpGrabbableMotionPhase MotionPhase = EVRExpGrabbableMotionPhase::Unavailable;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpGrabbableMotionPhase PreviousMotionPhase = EVRExpGrabbableMotionPhase::Unavailable;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpGrabbableNormalMotionMode NormalMotionMode =
        EVRExpGrabbableNormalMotionMode::None;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpGrabbableReleaseMotionMode ReleaseMotionMode =
        EVRExpGrabbableReleaseMotionMode::None;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpGrabbableMotionChangeFlags MotionChangeFlags =
        EVRExpGrabbableMotionChangeFlags::None;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    int64 MotionSnapshotSequence = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    TWeakObjectPtr<UVRExpGrabbableMotionComponent> GrabbableMotionComponent;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    TWeakObjectPtr<USceneComponent> MotionUpdatedComponent;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    int32 ActiveGripCount = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    bool bChangedGripHasMovementAuthority = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    bool bHasAnyMovementAuthority = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    FString ActivationFailureReason;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    TWeakObjectPtr<AActor> AnchorActor;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    TWeakObjectPtr<USceneComponent> AnchorComponent;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    FName AnchorSocketName = NAME_None;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    FTransform AnchorWorldTransform = FTransform::Identity;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    FTransform TargetWorldTransform = FTransform::Identity;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    FTransform CurrentWorldTransform = FTransform::Identity;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    float PositionError = -1.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    float RotationErrorDegrees = -1.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    float ScaleError = -1.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    float HideRemainingSeconds = -1.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    float DestroyRemainingSeconds = -1.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    bool bExitTransitionActive = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    bool bExitTransitionReversing = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    float ExitTransitionAlpha = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    float ExitTransitionRemainingSeconds = -1.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpUIInfoVisibilityReason ExitVisibilityReason = EVRExpUIInfoVisibilityReason::None;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    float HeadVisibleRemainingSeconds = -1.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    float HeadCooldownRemainingSeconds = -1.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    bool bAwaitingFreshHeadDetection = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpUIInfoVisibilityReason LastVisibilityReason = EVRExpUIInfoVisibilityReason::None;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    FString FailureReason;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVRExpUIInfoActorSpawnedEvent, AActor *, UIActor,
                                             const FVRExpDetectableInteractionContext &, Context);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
    FVRExpUIInfoPresentationChangedEvent, AActor *, UIActor,
    EVRExpUIInfoInteractionSource, InteractionSource,
    const FVRExpUIInfoPresentationSettings &, PresentationSettings,
    const FVRExpDetectableInteractionContext &, Context);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
    FVRExpUIInfoVisibilityChangedEvent, AActor *, UIActor, bool, bVisible,
    EVRExpUIInfoVisibilityReason, VisibilityReason,
    const FVRExpDetectableInteractionContext &, Context);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVRExpUIInfoActorDestroyedEvent, AActor *, UIActor,
                                             const FVRExpDetectableInteractionContext &, Context);
