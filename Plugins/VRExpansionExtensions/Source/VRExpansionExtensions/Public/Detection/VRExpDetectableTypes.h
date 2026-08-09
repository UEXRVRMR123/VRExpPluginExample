#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Interaction/VRExpGripInteractionTypes.h"
#include "VRBPDatatypes.h"
#include "VRExpDetectableTypes.generated.h"

class UGripMotionControllerComponent;
class UPrimitiveComponent;
class USceneComponent;
class UVRExpDetectableComponent;
class UVRExpGrabbableMotionComponent;
class UVRExpHeadDetectionComponent;

UENUM(BlueprintType)
enum class EVRExpHeadDetectionMode : uint8
{
    SphereOverlap UMETA(DisplayName = "Sphere Overlap"),
    ForwardRay UMETA(DisplayName = "Forward Ray"),
    ForwardCone UMETA(DisplayName = "Forward Cone")
};

UENUM(BlueprintType)
enum class EVRExpHeadDetectionTargetMode : uint8
{
    SingleClosest UMETA(DisplayName = "Single Closest"),
    Multiple UMETA(DisplayName = "Multiple")
};

UENUM(BlueprintType)
enum class EVRExpHeadDetectionDebugState : uint8
{
    NotRunning UMETA(DisplayName = "Not Running"),
    NoQueryHits UMETA(DisplayName = "No Query Hits"),
    QueryHitsRejected UMETA(DisplayName = "Query Hits Rejected"),
    SingleTargetSelected UMETA(DisplayName = "Single Target Selected"),
    MultipleTargetsSelected UMETA(DisplayName = "Multiple Targets Selected")
};

UENUM(BlueprintType)
enum class EVRExpHeadDetectionRejectionReason : uint8
{
    None UMETA(DisplayName = "None"),
    ObjectTypeNotEnabled UMETA(DisplayName = "Object Type Not Enabled"),
    OutsideCone UMETA(DisplayName = "Outside Cone"),
    LineOfSightBlocked UMETA(DisplayName = "Line Of Sight Blocked"),
    NoDetectableComponent UMETA(DisplayName = "No Detectable Component"),
    PrimitiveModeRejected UMETA(DisplayName = "Primitive Mode Rejected"),
    OwnerRejected UMETA(DisplayName = "Owner Rejected")
};

UENUM(BlueprintType)
enum class EVRExpDetectablePrimitiveMode : uint8
{
    OwnerAnyPrimitive UMETA(
        DisplayName = "Owner Any Primitive",
        ToolTip = "Accept any Primitive Component owned by the detectable actor."),
    OwnerRootPrimitive UMETA(
        DisplayName = "Owner Root Primitive",
        ToolTip = "Accept only the detectable actor's root component when it is a Primitive Component."),
    AutoGripInterfacePrimitive UMETA(
        DisplayName = "Auto Grip Interface Primitive",
        ToolTip = "Accept Primitive Components that implement VRGripInterface; if none exist, fall back to the root Primitive Component."),
    ExplicitPrimitives UMETA(
        DisplayName = "Explicit Primitives",
        ToolTip = "Accept only the Primitive Components listed on the detectable component. An empty list accepts nothing.")
};

UENUM(BlueprintType)
enum class EVRExpDetectableGripSourceMode : uint8
{
    Disabled UMETA(
        DisplayName = "Disabled",
        ToolTip = "Do not evaluate grip interactions for this detectable component."),
    Auto UMETA(
        DisplayName = "Auto",
        ToolTip = "Use the unique Grabbable Motion Component when available; fall back to Detectable Targets only when none exists. Multiple candidates are treated as ambiguous."),
    DetectableTargets UMETA(
        DisplayName = "Detectable Targets",
        ToolTip = "Route grips directly from the detectable owner's VRGripInterface targets."),
    GrabbableMotionComponent UMETA(
        DisplayName = "Grabbable Motion Component",
        ToolTip = "Require a configured or uniquely resolved Grabbable Motion Component as the authoritative grip source.")
};

UENUM(BlueprintType)
enum class EVRExpDetectableGripSource : uint8
{
    None UMETA(DisplayName = "None"),
    DetectableTargets UMETA(DisplayName = "Detectable Targets"),
    GrabbableMotionComponent UMETA(DisplayName = "Grabbable Motion Component")
};

FORCEINLINE EVRExpDetectableGripSource ResolveVRExpAutomaticGripSource(
    int32 ValidMotionComponentCount,
    bool bHasDirectGripTargets)
{
    if (ValidMotionComponentCount == 1)
    {
        return EVRExpDetectableGripSource::GrabbableMotionComponent;
    }
    if (ValidMotionComponentCount > 1)
    {
        return EVRExpDetectableGripSource::None;
    }
    return bHasDirectGripTargets
               ? EVRExpDetectableGripSource::DetectableTargets
               : EVRExpDetectableGripSource::None;
}

UENUM(BlueprintType)
enum class EVRExpDetectableActivationMatchSource : uint8
{
    None UMETA(DisplayName = "None"),
    HeadDetection UMETA(DisplayName = "Head Detection"),
    Grip UMETA(DisplayName = "Grip"),
    Release UMETA(DisplayName = "Release"),
    Motion UMETA(DisplayName = "Motion")
};

USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpDetectableTriggerRuntimeDebugState
{
    GENERATED_BODY()

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpDetectableActivationMatchSource MatchedActivationSource =
        EVRExpDetectableActivationMatchSource::None;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    int64 MotionSnapshotSequence = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpGrabbableMotionPhase MotionPhase =
        EVRExpGrabbableMotionPhase::Unavailable;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    EVRExpGrabbableMotionPhase PreviousMotionPhase =
        EVRExpGrabbableMotionPhase::Unavailable;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    FString FirstFailureReason;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug")
    bool bFinalActive = false;
};

UENUM(BlueprintType)
enum class EVRExpDetectableChangeSource : uint8
{
    None UMETA(Hidden),
    HeadDetection UMETA(DisplayName = "Head Detection"),
    Grip UMETA(DisplayName = "Grip"),
    MotionState UMETA(DisplayName = "Motion State")
};

UENUM(BlueprintType)
enum class EVRExpDetectableChangePhase : uint8
{
    None UMETA(Hidden),
    Began UMETA(DisplayName = "Began"),
    Ended UMETA(DisplayName = "Ended"),
    Updated UMETA(DisplayName = "Updated")
};

USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpDetectableInteractionContext
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    bool bIsHeadDetected = false;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    bool bIsGripped = false;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    bool bIsInteracting = false;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    EVRExpDetectableChangeSource ChangeSource = EVRExpDetectableChangeSource::None;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    EVRExpDetectableChangePhase ChangePhase = EVRExpDetectableChangePhase::None;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    TObjectPtr<UVRExpDetectableComponent> DetectableComponent = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    TObjectPtr<UVRExpHeadDetectionComponent> HeadDetector = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    TObjectPtr<UPrimitiveComponent> DetectedPrimitive = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    TObjectPtr<UGripMotionControllerComponent> GripController = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    FBPActorGripInformation GripInformation;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    bool bWasSocketed = false;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    bool bChangedGripHasMovementAuthority = false;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    EVRExpDetectableGripSource GripSource = EVRExpDetectableGripSource::None;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    TArray<FVRExpGrabbableActiveGrip> ActiveGrips;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    int32 ActiveGripCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    TObjectPtr<UVRExpGrabbableMotionComponent> GrabbableMotionComponent = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    TObjectPtr<USceneComponent> MotionUpdatedComponent = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    EVRExpGrabbableMotionState MotionState = EVRExpGrabbableMotionState::Idle;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    EVRExpGrabbableMotionPhase MotionPhase = EVRExpGrabbableMotionPhase::Unavailable;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    EVRExpGrabbableMotionPhase PreviousMotionPhase = EVRExpGrabbableMotionPhase::Unavailable;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    EVRExpGrabbableNormalMotionMode NormalMotionMode =
        EVRExpGrabbableNormalMotionMode::None;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    EVRExpGrabbableReleaseMotionMode ReleaseMotionMode =
        EVRExpGrabbableReleaseMotionMode::None;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    bool bHasAnyMovementAuthority = false;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    EVRExpGrabbableMotionChangeFlags MotionChangeFlags =
        EVRExpGrabbableMotionChangeFlags::None;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    int64 MotionSnapshotSequence = 0;

    UPROPERTY(BlueprintReadOnly, Category = "VRExpansionExtensions|Detection")
    FVRExpGrabbableMotionRuntimeSnapshot MotionSnapshot;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVRExpDetectableInteractionEvent,
                                            const FVRExpDetectableInteractionContext &, Context);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVRExpHeadDetectionEvent, UVRExpDetectableComponent *, DetectableComponent,
                                             UPrimitiveComponent *, DetectedPrimitive);
