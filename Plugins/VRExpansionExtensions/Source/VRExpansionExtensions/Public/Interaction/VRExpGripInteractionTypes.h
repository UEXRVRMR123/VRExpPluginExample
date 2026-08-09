#pragma once

#include "CoreMinimal.h"
#include "VRBPDatatypes.h"
#include "VRExpGripInteractionTypes.generated.h"

class UGripMotionControllerComponent;
class USceneComponent;
class UVRExpGrabbableMotionComponent;

/** Configured motion used while the object is not being held. */
UENUM(BlueprintType)
enum class EVRExpGrabbableNormalMotionMode : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "Do not run a base path motion. Enabled floating or rotation effects may still run."),
	FollowSpline UMETA(DisplayName = "Follow Spline", ToolTip = "Move along the configured Spline component."),
	FollowTarget UMETA(DisplayName = "Follow Target", ToolTip = "Follow the configured target component."),
	OrbitTarget UMETA(DisplayName = "Orbit Target", ToolTip = "Orbit around the configured center component."),
	RandomWander UMETA(DisplayName = "Random Wander", ToolTip = "Wander around the captured motion origin.")
};

/** Configured behavior applied after the final authoritative grip is released. */
UENUM(BlueprintType)
enum class EVRExpGrabbableReleaseMotionMode : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "Do not take over motion after release."),
	ReturnToStart UMETA(DisplayName = "Return To Start", ToolTip = "Return to the transform captured at BeginPlay."),
	ReturnToSpline UMETA(DisplayName = "Return To Spline", ToolTip = "Return to the nearest point on the configured Spline."),
	ReturnToOrigin UMETA(DisplayName = "Return To Origin", ToolTip = "Return to the captured motion origin."),
	ContinueMotion UMETA(DisplayName = "Continue Motion", ToolTip = "Resume the configured normal motion immediately."),
	FallWithGravity UMETA(DisplayName = "Fall With Gravity", ToolTip = "Enable physics simulation on the UpdatedComponent."),
	FlyToTarget UMETA(DisplayName = "Fly To Target", ToolTip = "Fly to the configured target component.")
};

/** Current runtime state of a grabbable motion component. */
UENUM(BlueprintType)
enum class EVRExpGrabbableMotionState : uint8
{
	Idle UMETA(DisplayName = "Idle", ToolTip = "The component is not actively driving motion."),
	NormalMotion UMETA(DisplayName = "Normal Motion", ToolTip = "Configured spline, follow, orbit, wander, or motion effects are active."),
	Grabbed UMETA(DisplayName = "Grabbed", ToolTip = "At least one matching grip is active and normal motion is paused."),
	Releasing UMETA(DisplayName = "Releasing", ToolTip = "The component is executing its configured post-release motion."),
	Paused UMETA(DisplayName = "Paused", ToolTip = "Motion is manually paused.")
};

/** Coarse interaction phase derived from grip count and the detailed motion state. */
UENUM(BlueprintType)
enum class EVRExpGrabbableMotionPhase : uint8
{
	Unavailable UMETA(DisplayName = "Unavailable", ToolTip = "No authoritative Grabbable Motion Component is available."),
	Normal UMETA(DisplayName = "Normal", ToolTip = "The object is not gripped and is not executing release motion."),
	Grabbed UMETA(DisplayName = "Grabbed", ToolTip = "At least one authoritative grip is active."),
	Releasing UMETA(DisplayName = "Releasing", ToolTip = "The final grip ended and release motion is still executing.")
};

/** Derive the stable three-stage phase without measuring physical transform error. */
FORCEINLINE EVRExpGrabbableMotionPhase ResolveVRExpGrabbableMotionPhase(
	EVRExpGrabbableMotionState MotionState,
	int32 ActiveGripCount)
{
	if (ActiveGripCount > 0)
	{
		return EVRExpGrabbableMotionPhase::Grabbed;
	}

	return MotionState == EVRExpGrabbableMotionState::Releasing
		? EVRExpGrabbableMotionPhase::Releasing
		: EVRExpGrabbableMotionPhase::Normal;
}

/** Which controllers are allowed to route matching grips to a motion component. */
UENUM(BlueprintType)
enum class EVRExpGripControllerScope : uint8
{
	AllWorldControllers UMETA(
		DisplayName = "All World Controllers",
		ToolTip = "Accept matching grips from every grip controller registered in this World."),
	ManualControllers UMETA(
		DisplayName = "Manual Controllers",
		ToolTip = "Accept matching grips only from the configured Manual Grip Controllers array.")
};

UENUM(BlueprintType)
enum class EVRExpGrabbableGripChangePhase : uint8
{
	None UMETA(Hidden),
	Began UMETA(DisplayName = "Began"),
	Ended UMETA(DisplayName = "Ended")
};

UENUM(BlueprintType, meta = (
	Bitflags,
	UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EVRExpGrabbableMotionChangeFlags : uint8
{
	None = 0 UMETA(Hidden),
	Grip = 1 << 0 UMETA(DisplayName = "Grip"),
	MotionState = 1 << 1 UMETA(DisplayName = "Motion State"),
	NormalMotionMode = 1 << 2 UMETA(DisplayName = "Normal Motion Mode"),
	ReleaseMotionMode = 1 << 3 UMETA(DisplayName = "Release Motion Mode"),
	UpdatedComponent = 1 << 4 UMETA(DisplayName = "Updated Component"),
	RegistrationRebuilt = 1 << 5 UMETA(DisplayName = "Registration Rebuilt")
};
ENUM_CLASS_FLAGS(EVRExpGrabbableMotionChangeFlags);

/** One authoritative active grip owned by a grabbable motion component. */
USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpGrabbableActiveGrip
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grip")
	TObjectPtr<UGripMotionControllerComponent> GripController = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grip")
	FBPActorGripInformation GripInformation;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grip")
	bool bHasMovementAuthority = false;

	bool Matches(const UGripMotionControllerComponent* Controller, uint8 GripID) const
	{
		return GripController == Controller && GripInformation.GripID == GripID;
	}
};

/** Complete grip state after one routed grip transition. */
USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpGrabbableGripSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grip")
	TArray<FVRExpGrabbableActiveGrip> ActiveGrips;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grip")
	FVRExpGrabbableActiveGrip ChangedGrip;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grip")
	EVRExpGrabbableGripChangePhase ChangePhase = EVRExpGrabbableGripChangePhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grip")
	bool bWasSocketed = false;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grip")
	bool bIsFirstGrip = false;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grip")
	bool bIsFinalGrip = false;
};

/** One coherent view of all motion and grip state after an atomic runtime change. */
USTRUCT(BlueprintType)
struct VREXPANSIONEXTENSIONS_API FVRExpGrabbableMotionRuntimeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion")
	TObjectPtr<UVRExpGrabbableMotionComponent> MotionComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion")
	TObjectPtr<USceneComponent> UpdatedComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableMotionState MotionState = EVRExpGrabbableMotionState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableMotionState PreviousMotionState = EVRExpGrabbableMotionState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableMotionPhase MotionPhase = EVRExpGrabbableMotionPhase::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableMotionPhase PreviousMotionPhase = EVRExpGrabbableMotionPhase::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableNormalMotionMode NormalMotionMode = EVRExpGrabbableNormalMotionMode::None;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableNormalMotionMode PreviousNormalMotionMode = EVRExpGrabbableNormalMotionMode::None;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableReleaseMotionMode ReleaseMotionMode = EVRExpGrabbableReleaseMotionMode::None;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableReleaseMotionMode PreviousReleaseMotionMode = EVRExpGrabbableReleaseMotionMode::None;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion|Grip")
	TArray<FVRExpGrabbableActiveGrip> ActiveGrips;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion|Grip")
	int32 ActiveGripCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion|Grip")
	bool bIsActivelyGripped = false;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion|Grip")
	bool bHasAnyMovementAuthority = false;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion|Grip")
	FVRExpGrabbableActiveGrip ChangedGrip;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion|Grip")
	EVRExpGrabbableGripChangePhase GripChangePhase = EVRExpGrabbableGripChangePhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion|Grip")
	bool bWasSocketed = false;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableMotionChangeFlags ChangeFlags = EVRExpGrabbableMotionChangeFlags::None;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion")
	int64 Sequence = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FVRExpGrabbableGripStateChangedSignature,
	const FVRExpGrabbableGripSnapshot&,
	Snapshot);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FVRExpGrabbableMotionRuntimeSnapshotChangedSignature,
	const FVRExpGrabbableMotionRuntimeSnapshot&,
	Snapshot);
