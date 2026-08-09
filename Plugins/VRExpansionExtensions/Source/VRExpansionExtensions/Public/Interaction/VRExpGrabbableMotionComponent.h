// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/HitResult.h"
#include "GameFramework/MovementComponent.h"
#include "GripMotionControllerComponent.h"
#include "Interaction/VRExpGripInteractionTypes.h"
#include "VRExpGrabbableMotionComponent.generated.h"

class AActor;
class UPrimitiveComponent;
class USceneComponent;
class UVRExpGrabbableMotionComponent;
class UVRExpGripEventRouterSubsystem;
struct FVRExpScopedMotionRuntimeMutation;
struct FVRExpPendingGripReleasedEvent
{
	FBPActorGripInformation GripInformation;
	bool bWasSocketed = false;
	bool bIsFinalGrip = false;
	bool bHadMovementAuthority = false;
};

/** 可抓取物体模型使用的本地轴方向。 */
UENUM(BlueprintType)
enum class EVRExpGrabbableMotionAxis : uint8
{
	X UMETA(DisplayName = "+X"),
	Y UMETA(DisplayName = "+Y"),
	Z UMETA(DisplayName = "+Z"),
	NegativeX UMETA(DisplayName = "-X"),
	NegativeY UMETA(DisplayName = "-Y"),
	NegativeZ UMETA(DisplayName = "-Z")
};

/** 将模型本地朝向修正到组件使用的 +X 前向、+Z 上向坐标约定。 */
USTRUCT(BlueprintType)
struct FVRExpGrabbableOrientationAdjustment
{
	GENERATED_BODY()

	FVRExpGrabbableOrientationAdjustment()
		: bEnableAxisAdjustment(false)
		, ForwardAxis(EVRExpGrabbableMotionAxis::X)
		, UpAxis(EVRExpGrabbableMotionAxis::Z)
		, bEnableRotationOffset(false)
		, RotationOffset(FRotator::ZeroRotator)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Orientation")
	bool bEnableAxisAdjustment;

	/** 模型自身的本地前向轴。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Orientation", meta = (EditCondition = "bEnableAxisAdjustment"))
	EVRExpGrabbableMotionAxis ForwardAxis;

	/** 模型自身的本地上轴，不能与 ForwardAxis 平行或反向平行。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Orientation", meta = (EditCondition = "bEnableAxisAdjustment"))
	EVRExpGrabbableMotionAxis UpAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Orientation")
	bool bEnableRotationOffset;

	/** 在轴向修正之前，基于组件标准 +X 前向、+Z 上向坐标系应用的额外本地旋转。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Orientation", meta = (EditCondition = "bEnableRotationOffset"))
	FRotator RotationOffset;
};

/** 运动附加效果使用的坐标空间。 */
UENUM(BlueprintType)
enum class EVRExpGrabbableMotionEffectSpace : uint8
{
	World UMETA(DisplayName = "World", ToolTip = "使用固定世界空间轴。"),
	Local UMETA(DisplayName = "Local", ToolTip = "使用 Actor 当前本地空间轴。")
};

/** UpdatedComponent 已附加时，配置方向量采用的坐标语义。 */
UENUM(BlueprintType)
enum class EVRExpGrabbableAttachedMotionDirectionMode : uint8
{
	PreserveWorldDirections UMETA(
		DisplayName = "Preserve World Directions",
		ToolTip = "世界重力、World 浮动和其他世界方向保持世界语义，再动态换算到当前父组件相对空间。"),
	FollowAttachmentParent UMETA(
		DisplayName = "Follow Attachment Parent",
		ToolTip = "所有配置方向都按当前附加父组件的局部坐标系解释。")
};

/** FallWithGravity 在 UpdatedComponent 已附加时采用的实现。 */
UENUM(BlueprintType)
enum class EVRExpGrabbableAttachedFallWithGravityMode : uint8
{
	RelativeKinematic UMETA(
		DisplayName = "Relative Kinematic",
		ToolTip = "保持附加并关闭物理模拟，在当前父组件相对空间执行运动学下落。"),
	DetachAndSimulatePhysics UMETA(
		DisplayName = "Detach And Simulate Physics",
		ToolTip = "保留旧行为：脱离附加层级并启用 Unreal Engine 真实物理模拟。")
};

/** 附加状态下运动学 FallWithGravity 的碰撞策略。 */
UENUM(BlueprintType)
enum class EVRExpGrabbableAttachedKinematicFallCollisionMode : uint8
{
	SweepAndStop UMETA(
		DisplayName = "Sweep And Stop",
		ToolTip = "强制 Sweep；首次阻挡即按释放运动完成处理。"),
	UseGlobalMovementSettings UMETA(
		DisplayName = "Use Global Movement Settings",
		ToolTip = "遵循 bSweepMovement 和 bStopOnBlockingHit。"),
	NoSweep UMETA(
		DisplayName = "No Sweep",
		ToolTip = "不执行 Sweep，持续相对空间运动学下落，直到再次抓取或调用 StopMotion。")
};

/** 目标样条线点低于物体时，运动学重力阶段采用的路径。 */
UENUM(BlueprintType)
enum class EVRExpGrabbableSplineFallMode : uint8
{
	VerticalThenApproach UMETA(
		DisplayName = "Vertical Then Approach",
		ToolTip = "先沿当前配置的上/下方向下降到目标高度，再靠近样条线点。附加时方向由 AttachedMotionDirectionMode 决定。"),
	ApproachWhileFalling UMETA(
		DisplayName = "Approach While Falling",
		ToolTip = "沿当前配置方向加速下落，同时在垂直于该方向的平面靠近样条线点。")
};

/** 目标样条线点高于物体时采用的返回路径。 */
UENUM(BlueprintType)
enum class EVRExpGrabbableSplineAboveTargetMode : uint8
{
	DirectApproach UMETA(
		DisplayName = "Direct Approach",
		ToolTip = "直接沿三维方向靠近样条线点。"),
	RiseVerticallyThenApproach UMETA(
		DisplayName = "Rise Vertically Then Approach",
		ToolTip = "先沿当前配置的上方向上升到目标高度，再靠近样条线点。")
};

/** 运动学重力使用的加速度来源。 */
UENUM(BlueprintType)
enum class EVRExpGrabbableKinematicGravitySource : uint8
{
	WorldGravity UMETA(
		DisplayName = "World Gravity",
		ToolTip = "使用当前 Physics Volume 或项目默认重力的绝对值，并乘以 World Gravity Scale。"),
	CustomAcceleration UMETA(
		DisplayName = "Custom Acceleration",
		ToolTip = "使用 Custom Gravity Acceleration 指定的固定向下加速度。")
};

/** 释放附加规则的执行时机。 */
UENUM(BlueprintType)
enum class EVRExpGrabbableReleaseAttachmentTiming : uint8
{
	OnRelease UMETA(
		DisplayName = "On Release",
		ToolTip = "最后一次有效抓取释放后立即尝试附加。"),
	OnReleaseMotionCompleted UMETA(
		DisplayName = "On Release Motion Completed",
		ToolTip = "等待释放运动完成、目标失效或移动被阻挡后再尝试附加。")
};

/** 恢复到初始化父组件时采用的变换规则。 */
UENUM(BlueprintType)
enum class EVRExpGrabbableInitialParentTransformRule : uint8
{
	KeepWorld UMETA(
		DisplayName = "Keep World",
		ToolTip = "附加时保持当前世界变换。"),
	RestoreInitialRelative UMETA(
		DisplayName = "Restore Initial Relative",
		ToolTip = "恢复 BeginPlay 时相对初始化父组件的完整相对变换。")
};

/** 附加到样条线组件时采用的变换规则。 */
UENUM(BlueprintType)
enum class EVRExpGrabbableSplineAttachmentTransformRule : uint8
{
	KeepWorld UMETA(
		DisplayName = "Keep World",
		ToolTip = "附加时保持当前世界变换。"),
	RestoreInitialRelativeToSpline UMETA(
		DisplayName = "Restore Initial Relative To Spline",
		ToolTip = "恢复 BeginPlay 时相对目标样条线组件的完整相对变换。"),
	SnapToClosestSplinePoint UMETA(
		DisplayName = "Snap To Closest Spline Point",
		ToolTip = "吸附到距离当前位置最近的样条线点，并沿用组件的样条线方向和旋转修正规则。")
};

/** 释放附加前对物理模拟的处理策略。 */
UENUM(BlueprintType)
enum class EVRExpGrabbableAttachmentPhysicsPolicy : uint8
{
	PreservePhysicsAndSkipIfSimulating UMETA(
		DisplayName = "Preserve Physics And Skip If Simulating",
		ToolTip = "保留当前物理状态；根组件正在模拟物理时跳过附加。"),
	DisablePhysicsAndAttach UMETA(
		DisplayName = "Disable Physics And Attach",
		ToolTip = "附加前关闭根组件物理模拟；附加失败时恢复原物理状态和速度。")
};

/** 单个 ReleaseMotionMode 使用的释放附加规则。 */
USTRUCT(BlueprintType)
struct FVRExpGrabbableReleaseAttachmentModeRule
{
	GENERATED_BODY()

	FVRExpGrabbableReleaseAttachmentModeRule()
		: FVRExpGrabbableReleaseAttachmentModeRule(
			true,
			EVRExpGrabbableReleaseAttachmentTiming::OnRelease,
			EVRExpGrabbableInitialParentTransformRule::KeepWorld,
			EVRExpGrabbableSplineAttachmentTransformRule::KeepWorld,
			EVRExpGrabbableAttachmentPhysicsPolicy::DisablePhysicsAndAttach)
	{
	}

	FVRExpGrabbableReleaseAttachmentModeRule(
		bool bInEnableAttachment,
		EVRExpGrabbableReleaseAttachmentTiming InTiming,
		EVRExpGrabbableInitialParentTransformRule InInitialParentTransformRule,
		EVRExpGrabbableSplineAttachmentTransformRule InSplineTransformRule,
		EVRExpGrabbableAttachmentPhysicsPolicy InPhysicsPolicy)
		: bEnableAttachment(bInEnableAttachment)
		, Timing(InTiming)
		, InitialParentTransformRule(InInitialParentTransformRule)
		, SplineTransformRule(InSplineTransformRule)
		, PhysicsPolicy(InPhysicsPolicy)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release Attachment")
	bool bEnableAttachment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release Attachment")
	EVRExpGrabbableReleaseAttachmentTiming Timing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release Attachment")
	EVRExpGrabbableInitialParentTransformRule InitialParentTransformRule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release Attachment")
	EVRExpGrabbableSplineAttachmentTransformRule SplineTransformRule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release Attachment")
	EVRExpGrabbableAttachmentPhysicsPolicy PhysicsPolicy;
};

/** 七种 ReleaseMotionMode 各自独立的释放附加规则。 */
USTRUCT(BlueprintType)
struct FVRExpGrabbableReleaseAttachmentRules
{
	GENERATED_BODY()

	FVRExpGrabbableReleaseAttachmentRules()
		: NoneRule(
			true,
			EVRExpGrabbableReleaseAttachmentTiming::OnRelease,
			EVRExpGrabbableInitialParentTransformRule::KeepWorld,
			EVRExpGrabbableSplineAttachmentTransformRule::KeepWorld,
			EVRExpGrabbableAttachmentPhysicsPolicy::DisablePhysicsAndAttach)
		, ReturnToStartRule(
			true,
			EVRExpGrabbableReleaseAttachmentTiming::OnReleaseMotionCompleted,
			EVRExpGrabbableInitialParentTransformRule::KeepWorld,
			EVRExpGrabbableSplineAttachmentTransformRule::KeepWorld,
			EVRExpGrabbableAttachmentPhysicsPolicy::DisablePhysicsAndAttach)
		, ReturnToSplineRule(ReturnToStartRule)
		, ReturnToOriginRule(ReturnToStartRule)
		, ContinueMotionRule(NoneRule)
		, FallWithGravityRule(
			false,
			EVRExpGrabbableReleaseAttachmentTiming::OnRelease,
			EVRExpGrabbableInitialParentTransformRule::KeepWorld,
			EVRExpGrabbableSplineAttachmentTransformRule::KeepWorld,
			EVRExpGrabbableAttachmentPhysicsPolicy::PreservePhysicsAndSkipIfSimulating)
		, FlyToTargetRule(ReturnToStartRule)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release Attachment", meta = (DisplayName = "None"))
	FVRExpGrabbableReleaseAttachmentModeRule NoneRule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release Attachment", meta = (DisplayName = "Return To Start"))
	FVRExpGrabbableReleaseAttachmentModeRule ReturnToStartRule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release Attachment", meta = (DisplayName = "Return To Spline"))
	FVRExpGrabbableReleaseAttachmentModeRule ReturnToSplineRule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release Attachment", meta = (DisplayName = "Return To Origin"))
	FVRExpGrabbableReleaseAttachmentModeRule ReturnToOriginRule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release Attachment", meta = (DisplayName = "Continue Motion"))
	FVRExpGrabbableReleaseAttachmentModeRule ContinueMotionRule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release Attachment", meta = (DisplayName = "Fall With Gravity"))
	FVRExpGrabbableReleaseAttachmentModeRule FallWithGravityRule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release Attachment", meta = (DisplayName = "Fly To Target"))
	FVRExpGrabbableReleaseAttachmentModeRule FlyToTargetRule;
};

/** Actor 在主动运动期间使用的正弦浮动效果。 */
USTRUCT(BlueprintType)
struct FVRExpGrabbableFloatingEffectSettings
{
	GENERATED_BODY()

	FVRExpGrabbableFloatingEffectSettings()
		: bEnableFloating(false)
		, Space(EVRExpGrabbableMotionEffectSpace::World)
		, Axis(FVector::UpVector)
		, Amplitude(20.0f)
		, Frequency(1.0f)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Effects|Floating")
	bool bEnableFloating;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Effects|Floating", meta = (EditCondition = "bEnableFloating"))
	EVRExpGrabbableMotionEffectSpace Space;

	/** 浮动方向；运行时会进行归一化，零向量会跳过浮动。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Effects|Floating", meta = (EditCondition = "bEnableFloating"))
	FVector Axis;

	/** 单侧最大浮动距离，单位为厘米。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Effects|Floating", meta = (EditCondition = "bEnableFloating", ClampMin = "0.0", UIMin = "0.0"))
	float Amplitude;

	/** 每秒完成的浮动循环次数。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Effects|Floating", meta = (EditCondition = "bEnableFloating", ClampMin = "0.0", UIMin = "0.0"))
	float Frequency;
};

/** Actor 在主动运动期间使用的持续自转效果。 */
USTRUCT(BlueprintType)
struct FVRExpGrabbableRotationEffectSettings
{
	GENERATED_BODY()

	FVRExpGrabbableRotationEffectSettings()
		: bEnableRotationEffect(false)
		, Space(EVRExpGrabbableMotionEffectSpace::Local)
		, RotationRate(0.0f, 30.0f, 0.0f)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Effects|Rotation")
	bool bEnableRotationEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Effects|Rotation", meta = (EditCondition = "bEnableRotationEffect"))
	EVRExpGrabbableMotionEffectSpace Space;

	/** Pitch、Yaw、Roll 轴的旋转速率，单位为度/秒。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Effects|Rotation", meta = (EditCondition = "bEnableRotationEffect"))
	FRotator RotationRate;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVRExpGrabbableMotionStateChangedSignature, EVRExpGrabbableMotionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FVRExpGrabbableMotionSourceInvalidatedSignature,
	UVRExpGrabbableMotionComponent*,
	MotionComponent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVRExpGrabbableReturnMotionCompletedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVRExpGrabbableMovementBlockedSignature, const FHitResult&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FVRExpGrabbableGripReleasedSignature, const FBPActorGripInformation&, GripInformation, bool, bWasSocketed, bool, bIsFinalGrip, bool, bHadMovementAuthority);

UCLASS(Blueprintable, ClassGroup = (VRExp), meta = (BlueprintSpawnableComponent, DisplayName = "VRExp Grabbable Motion Component"))
class VREXPANSIONEXTENSIONS_API UVRExpGrabbableMotionComponent : public UMovementComponent
{
	GENERATED_BODY()

public:
	UVRExpGrabbableMotionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void SetUpdatedComponent(
		USceneComponent* NewUpdatedComponent) override;

	UFUNCTION(BlueprintCallable, Category = "VRExp|Grabbable Motion")
	void SetNormalMotionMode(EVRExpGrabbableNormalMotionMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "VRExp|Grabbable Motion")
	void SetReleaseMotionMode(EVRExpGrabbableReleaseMotionMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "VRExp|Grabbable Motion")
	void StartMotion();

	UFUNCTION(BlueprintCallable, Category = "VRExp|Grabbable Motion")
	void StopMotion();

	UFUNCTION(BlueprintCallable, Category = "VRExp|Grabbable Motion")
	void PauseMotion();

	UFUNCTION(BlueprintCallable, Category = "VRExp|Grabbable Motion")
	void ResumeMotion();

	UFUNCTION(BlueprintCallable, Category = "VRExp|Grabbable Motion|Grip")
	void RefreshGripRegistration();

	UFUNCTION(BlueprintPure, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableMotionState GetMotionState() const
	{
		return CurrentMotionState;
	}

	UFUNCTION(BlueprintPure, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableMotionPhase GetMotionPhase() const
	{
		return ResolveVRExpGrabbableMotionPhase(
			CurrentMotionState,
			ActiveGrips.Num());
	}

	UFUNCTION(BlueprintPure, Category = "VRExp|Grabbable Motion|Grip")
	bool IsActivelyGripped() const
	{
		return !ActiveGrips.IsEmpty();
	}

	UFUNCTION(BlueprintPure, Category = "VRExp|Grabbable Motion|Grip")
	int32 GetActiveGripCount() const
	{
		return ActiveGrips.Num();
	}

	UFUNCTION(BlueprintPure, Category = "VRExp|Grabbable Motion|Grip")
	FVRExpGrabbableGripSnapshot GetGripSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "VRExp|Grabbable Motion")
	FVRExpGrabbableMotionRuntimeSnapshot GetRuntimeSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "VRExp|Grabbable Motion")
	USceneComponent* GetResolvedUpdatedComponent() const
	{
		return UpdatedComponent;
	}

	UPROPERTY(BlueprintAssignable, Category = "VRExp|Grabbable Motion")
	FVRExpGrabbableMotionStateChangedSignature OnMotionStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|Grabbable Motion")
	FVRExpGrabbableMotionRuntimeSnapshotChangedSignature OnRuntimeSnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|Grabbable Motion|Grip")
	FVRExpGrabbableGripStateChangedSignature OnGripStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|Grabbable Motion|Grip")
	FVRExpGrabbableMotionSourceInvalidatedSignature OnMotionSourceInvalidated;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|Grabbable Motion")
	FVRExpGrabbableReturnMotionCompletedSignature OnReturnMotionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|Grabbable Motion")
	FVRExpGrabbableMovementBlockedSignature OnMovementBlocked;

	/** 当匹配到本组件目标的 Grip 被释放时触发；组件内置释放处理会先执行，蓝图可在此事件中覆盖最终行为。多手抓取时 bIsFinalGrip 表示是否已经没有其他有效抓取。 */
	UPROPERTY(BlueprintAssignable, Category = "VRExp|Grabbable Motion|Grip")
	FVRExpGrabbableGripReleasedSignature OnGripReleased;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetNormalMotionMode, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableNormalMotionMode NormalMotionMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetReleaseMotionMode, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableReleaseMotionMode ReleaseMotionMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion")
	bool bAutoStartNormalMotion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion")
	bool bPauseNormalMotionWhenGrabbed;

	/** 是否允许本组件在移动 UpdatedComponent 时更新其旋转；关闭后组件只更新位置，不覆盖外部系统设置的当前旋转。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Orientation")
	bool bUpdateRotationDuringMotion;

	/** UpdatedComponent 已附加时，重力、World 效果和轨道方向是保持世界方向还是跟随当前父组件。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Orientation")
	EVRExpGrabbableAttachedMotionDirectionMode AttachedMotionDirectionMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Orientation", meta = (EditCondition = "bUpdateRotationDuringMotion"))
	FVRExpGrabbableOrientationAdjustment OrientationAdjustment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Effects")
	FVRExpGrabbableFloatingEffectSettings FloatingEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Effects", meta = (EditCondition = "bUpdateRotationDuringMotion"))
	FVRExpGrabbableRotationEffectSettings RotationEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion")
	bool bSweepMovement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion", meta = (EditCondition = "bSweepMovement"))
	bool bStopOnBlockingHit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Physics")
	bool bDisablePhysicsDuringKinematicMotion;

	/** FallWithGravity 在 UpdatedComponent 当前已附加时使用相对运动学，或脱离后使用真实物理。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Physics",
		meta = (EditCondition = "ReleaseMotionMode == EVRExpGrabbableReleaseMotionMode::FallWithGravity", EditConditionHides))
	EVRExpGrabbableAttachedFallWithGravityMode AttachedFallWithGravityMode;

	/** RelativeKinematic 下落使用的碰撞策略。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Physics",
		meta = (EditCondition = "ReleaseMotionMode == EVRExpGrabbableReleaseMotionMode::FallWithGravity && AttachedFallWithGravityMode == EVRExpGrabbableAttachedFallWithGravityMode::RelativeKinematic", EditConditionHides))
	EVRExpGrabbableAttachedKinematicFallCollisionMode AttachedKinematicFallCollisionMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Grip",
		meta = (DisplayName = "Grip Controller Scope",
			ToolTip = "Accept matching grips from all controllers in this World or only from the manual controller list. Call Refresh Grip Registration after runtime changes."))
	EVRExpGripControllerScope GripControllerScope;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Grip",
		meta = (DisplayName = "Manual Grip Controllers",
			EditCondition = "GripControllerScope == EVRExpGripControllerScope::ManualControllers",
			EditConditionHides,
			ToolTip = "Only these controller components may route matching grips while the scope is Manual Controllers."))
	TArray<TObjectPtr<UGripMotionControllerComponent>> ManualGripControllers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Grip",
		meta = (DisplayName = "Match Owner Actor",
			ToolTip = "Treat a grip whose target Actor is this component's Owner as a matching grip."))
	bool bMatchOwnerActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Grip",
		meta = (DisplayName = "Match Updated Component",
			ToolTip = "Treat a grip on the current UpdatedComponent as a matching grip. Runtime UpdatedComponent changes refresh routing automatically."))
	bool bMatchUpdatedComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Grip",
		meta = (DisplayName = "Match Owner Components",
			ToolTip = "Treat a grip on any component owned by this component's Owner Actor as a matching grip."))
	bool bMatchOwnerComponents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline",
		meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline || ReleaseMotionMode == EVRExpGrabbableReleaseMotionMode::ReturnToSpline || bAttachOwnerToSplineOnRelease"))
	bool bUseManualSplineReference;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline",
		meta = (EditCondition = "(NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline || ReleaseMotionMode == EVRExpGrabbableReleaseMotionMode::ReturnToSpline || bAttachOwnerToSplineOnRelease) && bUseManualSplineReference", EditConditionHides))
	bool bUseSplineActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline",
		meta = (EditCondition = "(NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline || ReleaseMotionMode == EVRExpGrabbableReleaseMotionMode::ReturnToSpline || bAttachOwnerToSplineOnRelease) && bUseManualSplineReference && bUseSplineActor", EditConditionHides))
	TObjectPtr<AActor> SplineActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline",
		meta = (EditCondition = "(NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline || ReleaseMotionMode == EVRExpGrabbableReleaseMotionMode::ReturnToSpline || bAttachOwnerToSplineOnRelease) && bUseManualSplineReference && !bUseSplineActor", EditConditionHides))
	FName SplineComponentName;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion|Spline")
	TObjectPtr<USplineComponent> SplineToFollow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline"))
	bool bTeleportToSplineOnStart;

	/** 未瞬移到样条线时，是否使用运动学重力的分阶段接近路径。不会开启物理模拟。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline|Kinematic Gravity",
		meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline && !bTeleportToSplineOnStart", EditConditionHides))
	bool bUseKinematicGravityOnSplineStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline", ClampMin = "0.0", UIMin = "0.0"))
	float SplineSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline", ClampMin = "0.1", UIMin = "0.1"))
	float SplineArrivalThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline"))
	bool bLoopSpline;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline",
		meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline || ReleaseMotionMode == EVRExpGrabbableReleaseMotionMode::ReturnToSpline || bAttachOwnerToSplineOnRelease", ToolTip = "沿 Spline 反向移动，并让物体朝向实际移动方向。"))
	bool bReverseDirection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Follow", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowTarget", EditConditionHides))
	TObjectPtr<USceneComponent> TargetToFollow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Follow", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowTarget", ClampMin = "0.0", UIMin = "0.0"))
	float FollowSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Follow", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowTarget", ClampMin = "0.0", UIMin = "0.0"))
	float FollowDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Follow", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowTarget && bUpdateRotationDuringMotion"))
	bool bOrientToTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Orbit", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::OrbitTarget", EditConditionHides))
	TObjectPtr<USceneComponent> OrbitCenter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Orbit", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::OrbitTarget", ClampMin = "0.0", UIMin = "0.0"))
	float OrbitRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Orbit", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::OrbitTarget"))
	float OrbitSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Orbit", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::OrbitTarget"))
	FVector OrbitAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Orbit", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::OrbitTarget"))
	float OrbitHeightOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Wander", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::RandomWander", ClampMin = "0.0", UIMin = "0.0"))
	float WanderRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Wander", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::RandomWander", ClampMin = "0.0", UIMin = "0.0"))
	float WanderSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Wander", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::RandomWander", ClampMin = "0.0", UIMin = "0.0"))
	float WanderChangeInterval;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release", meta = (EditCondition = "ReleaseMotionMode == EVRExpGrabbableReleaseMotionMode::ReturnToSpline"))
	bool bTeleportToSplineOnRelease;

	/** 未瞬移返回样条线时，是否使用运动学重力的分阶段接近路径。不会开启物理模拟。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline|Kinematic Gravity",
		meta = (EditCondition = "ReleaseMotionMode == EVRExpGrabbableReleaseMotionMode::ReturnToSpline && !bTeleportToSplineOnRelease", EditConditionHides))
	bool bUseKinematicGravityOnSplineRelease;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline|Kinematic Gravity",
		meta = (EditCondition = "bUseKinematicGravityOnSplineStart || bUseKinematicGravityOnSplineRelease", EditConditionHides))
	EVRExpGrabbableSplineFallMode SplineFallMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline|Kinematic Gravity",
		meta = (EditCondition = "bUseKinematicGravityOnSplineStart || bUseKinematicGravityOnSplineRelease", EditConditionHides))
	EVRExpGrabbableSplineAboveTargetMode SplineAboveTargetMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline|Kinematic Gravity",
		meta = (EditCondition = "bUseKinematicGravityOnSplineStart || bUseKinematicGravityOnSplineRelease", EditConditionHides))
	EVRExpGrabbableKinematicGravitySource KinematicGravitySource;

	/** WorldGravity 模式使用的非负倍率。倍率或有效世界重力接近零时跳过下落阶段。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline|Kinematic Gravity",
		meta = (EditCondition = "(bUseKinematicGravityOnSplineStart || bUseKinematicGravityOnSplineRelease) && KinematicGravitySource == EVRExpGrabbableKinematicGravitySource::WorldGravity", EditConditionHides, ClampMin = "0.0", UIMin = "0.0"))
	float WorldGravityScale;

	/** CustomAcceleration 模式使用的向下加速度大小，单位为厘米/秒平方。接近零时跳过下落阶段。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline|Kinematic Gravity",
		meta = (EditCondition = "(bUseKinematicGravityOnSplineStart || bUseKinematicGravityOnSplineRelease) && KinematicGravitySource == EVRExpGrabbableKinematicGravitySource::CustomAcceleration", EditConditionHides, ClampMin = "0.0", UIMin = "0.0", Units = "cm/s^2"))
	float CustomGravityAcceleration;

	/** 运动学重力的最大向下速度，单位为厘米/秒。无效或接近零时跳过下落阶段。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline|Kinematic Gravity",
		meta = (EditCondition = "bUseKinematicGravityOnSplineStart || bUseKinematicGravityOnSplineRelease", EditConditionHides, ClampMin = "0.0", UIMin = "0.0", Units = "cm/s"))
	float MaxFallSpeed;

	/** 高度阶段是否朝向实际移动方向；bUpdateRotationDuringMotion 仍是总开关。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline|Kinematic Gravity",
		meta = (EditCondition = "(bUseKinematicGravityOnSplineStart || bUseKinematicGravityOnSplineRelease) && bUpdateRotationDuringMotion", EditConditionHides))
	bool bOrientDuringSplineHeightMotion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ReturnSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release")
	bool bSmoothReturn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release", meta = (EditCondition = "ReleaseMotionMode == EVRExpGrabbableReleaseMotionMode::ReturnToStart"))
	TObjectPtr<UCurveFloat> ReturnCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release", meta = (EditCondition = "ReleaseMotionMode == EVRExpGrabbableReleaseMotionMode::FlyToTarget", EditConditionHides))
	TObjectPtr<USceneComponent> FlyToTargetComponent;

	/** 最后一次有效抓取释放时，是否允许恢复到 BeginPlay 时缓存的实际父组件和 Socket。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release Attachment")
	bool bReattachOwnerToInitialParentComponentOnRelease;

	/** 最后一次有效抓取释放时，是否优先尝试附加到有效的外部 SplineToFollow。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release Attachment")
	bool bAttachOwnerToSplineOnRelease;

	/** 每种 ReleaseMotionMode 的启用状态、执行时机、变换规则和物理策略。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release Attachment",
		meta = (EditCondition = "bReattachOwnerToInitialParentComponentOnRelease || bAttachOwnerToSplineOnRelease", EditConditionHides))
	FVRExpGrabbableReleaseAttachmentRules ReleaseAttachmentRules;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableMotionState CurrentMotionState;

private:
	/** 运动目标的持久锚点；有参考父组件时保存相对变换，同时保留最后有效世界变换兜底。 */
	struct FMotionAnchor
	{
		TWeakObjectPtr<USceneComponent> ReferenceParent;
		FName ReferenceSocketName = NAME_None;
		FTransform RelativeTransform = FTransform::Identity;
		FTransform LastValidWorldTransform = FTransform::Identity;
		bool bRelativeToReferenceParent = false;
	};

	enum class ESplineApproachPhase : uint8
	{
		None,
		Falling,
		Rising,
		Approaching
	};

	void ResolveUpdatedComponent();
	bool DoesGripMatchTarget(const FBPActorGripInformation& GripInformation) const;
	void GetGripRoutingTargets(TArray<UObject*>& OutGripTargets) const;
	bool AcceptsGripController(const UGripMotionControllerComponent* GripController) const;
	bool HasGripRegistrationTargetChanged() const;
	void NotifyRoutedGripBegin(
		UGripMotionControllerComponent* GripController,
		const FBPActorGripInformation& GripInformation);
	void NotifyRoutedGripEnd(
		UGripMotionControllerComponent* GripController,
		const FBPActorGripInformation& GripInformation,
		bool bWasSocketed);
	static bool ShouldStartReleaseMotionFromGripEnd(
		bool bIsFinalGrip,
		bool bWasSocketed,
		bool bHadMovementAuthority);
	int32 FindActiveGripIndex(
		const UGripMotionControllerComponent* GripController,
		uint8 GripID) const;
	FVRExpGrabbableGripSnapshot MakeGripSnapshot(
		const FVRExpGrabbableActiveGrip& ChangedGrip,
		EVRExpGrabbableGripChangePhase ChangePhase,
		bool bWasSocketed,
		bool bIsFirstGrip,
		bool bIsFinalGrip) const;
	FVRExpGrabbableMotionRuntimeSnapshot MakeRuntimeSnapshot(
		const FVRExpGrabbableMotionRuntimeSnapshot* PreviousSnapshot,
		EVRExpGrabbableMotionChangeFlags ChangeFlags,
		const FVRExpGrabbableActiveGrip& ChangedGrip,
		EVRExpGrabbableGripChangePhase GripChangePhase,
		bool bWasSocketed,
		int64 Sequence) const;
	void BeginRuntimeMutation();
	void EndRuntimeMutation();
	void MarkRuntimeChange(EVRExpGrabbableMotionChangeFlags ChangeFlags);
	void RecordGripRuntimeChange(
		const FVRExpGrabbableActiveGrip& ChangedGrip,
		EVRExpGrabbableGripChangePhase ChangePhase,
		bool bWasSocketed,
		bool bIsFirstGrip,
		bool bIsFinalGrip);
	void PruneInvalidActiveGrips();

	void SetMotionState(
		EVRExpGrabbableMotionState NewState,
		bool bBroadcastChange = true);
	void StartNormalMotion();
	void StartReleaseMotion();
	void StartReleaseMotionFromDrop();
	void FinishReleaseMotion(bool bResumeNormalMotion);
	void PrepareReleaseAttachmentBeforeMotion(
		EVRExpGrabbableReleaseMotionMode ReleasedMode);
	void FinalizeReleaseAttachmentAfterMotionStart();
	void CaptureInitialReleaseAttachmentState();
	USplineComponent* ResolveConfiguredSplineComponent() const;
	const FVRExpGrabbableReleaseAttachmentModeRule& GetReleaseAttachmentRule(
		EVRExpGrabbableReleaseMotionMode Mode) const;
	void HandleReleaseAttachmentAfterDrop(
		EVRExpGrabbableReleaseMotionMode ReleasedMode);
	void CancelPendingReleaseAttachment();
	void CompletePendingReleaseAttachment();
	bool TryApplyReleaseAttachment(
		EVRExpGrabbableReleaseMotionMode ReleasedMode,
		const FVRExpGrabbableReleaseAttachmentModeRule& Rule,
		bool bAllowSplineAttachment,
		bool bAllowInitialParentAttachment);
	bool ResolveReleaseAttachmentTarget(
		USceneComponent* OwnerRoot,
		bool bAllowSplineAttachment,
		bool bAllowInitialParentAttachment,
		USceneComponent*& OutTarget,
		FName& OutSocketName,
		USplineComponent*& OutSplineTarget);
	bool IsValidReleaseAttachmentTarget(
		const USceneComponent* OwnerRoot,
		const USceneComponent* Target) const;
	FQuat GetSplineAttachmentRotation(
		const USplineComponent* SplineComponent,
		float DistanceAlongSpline,
		const FQuat& CurrentRootRotation);

	void TickNormalMotion(float DeltaTime);
	void TickReleaseMotion(float DeltaTime);
	void TickSplineMotion(float DeltaTime);
	void TickFollowMotion(float DeltaTime);
	void TickOrbitMotion(float DeltaTime);
	void TickWanderMotion(float DeltaTime);
	void TickReturnToStart(float DeltaTime);
	void TickReturnToOrigin(float DeltaTime);
	void TickReturnToSpline(float DeltaTime);
	void TickFlyToTarget(float DeltaTime);
	void TickAttachedKinematicFall(float DeltaTime);

	bool InitializeSplineReference();
	bool CaptureSplineApproachTarget(const FVector& CurrentLocation);
	bool RefreshSplineApproachTarget();
	void InitializeKinematicSplineApproach(const FVector& CurrentLocation);
	void ResetSplineApproach();
	bool TickKinematicSplineApproach(float DeltaTime, float ApproachSpeed, bool bSmoothApproach);
	float ResolveKinematicGravityAcceleration() const;
	FVector InterpolateSplineApproach(
		const FVector& CurrentLocation,
		const FVector& TargetLocation,
		float DeltaTime,
		float ApproachSpeed,
		bool bSmoothApproach) const;
	FQuat ResolveSplineApproachRotation(
		const FVector& CurrentLocation,
		const FVector& TargetLocation,
		bool bIsHeightPhase);
	void GenerateNewWanderTarget();
	void RefreshCurrentMotionSpace(bool bForceRefresh = false);
	void RebuildMotionTickPrerequisites();
	void ClearMotionTickPrerequisites();
	bool IsUsingParentRelativeMotionSpace() const;
	FVector ConvertWorldLocationToMotionSpace(const FVector& WorldLocation) const;
	FQuat ConvertWorldRotationToMotionSpace(const FQuat& WorldRotation) const;
	FVector ConvertWorldVectorToMotionSpace(const FVector& WorldVector) const;
	FVector ConvertMotionLocationToWorld(const FVector& MotionLocation) const;
	FQuat ConvertMotionRotationToWorld(const FQuat& MotionRotation) const;
	FVector GetConfiguredDirectionInMotionSpace(const FVector& Direction) const;
	FVector GetKinematicUpAxisInMotionSpace() const;
	FMotionAnchor CaptureMotionAnchor(
		const FTransform& WorldTransform,
		USceneComponent* ExplicitReferenceParent = nullptr,
		FName ExplicitReferenceSocketName = NAME_None) const;
	FTransform ResolveMotionAnchorWorldTransform(FMotionAnchor& Anchor);
	FTransform ResolveMotionAnchorTransform(FMotionAnchor& Anchor);
	void PrepareForKinematicMotion();
	void SetUpdatedPrimitiveSimulatePhysics(bool bSimulate);
	bool HasConfiguredMotionEffects() const;
	static bool IsMotionEffectState(EVRExpGrabbableMotionState State);
	bool IsFloatingEffectActive() const;
	bool IsRotationEffectActive() const;
	bool HasActiveMotionEffects() const;
	void RefreshMotionEffectRuntimeState();
	void AdvanceMotionEffects(float DeltaTime);
	void ResetFloatingEffectTracking();
	void ResetRotationEffectTracking();
	void BakeMotionEffectsIntoBase();
	FVector CalculateFloatingOffset(const FQuat& FinalRotation);
	FQuat ApplyRotationEffect(const FQuat& BaseRotation) const;
	FQuat GetSplineMotionRotation(float DistanceAlongSpline);
	FQuat ApplyOrientationAdjustment(const FQuat& BaseRotation);
	static FVector GetAxisVector(EVRExpGrabbableMotionAxis Axis);
	static bool TryBuildAxisCorrection(const FVRExpGrabbableOrientationAdjustment& Adjustment, FQuat& OutAxisCorrection);
	bool MoveUpdatedComponentTo(
		const FVector& TargetLocation,
		const FQuat& TargetRotation,
		float DeltaTime,
		bool bForceNoSweep = false,
		ETeleportType Teleport = ETeleportType::None,
		bool bForceSweep = false,
		bool bCompleteReleaseOnBlockingHit = true);

	FTransform GetUpdatedComponentTransform() const;
	FVector GetUpdatedComponentLocation() const;
	FQuat GetUpdatedComponentQuat() const;
	FVector GetMotionBaseLocation() const;
	FQuat GetMotionBaseQuat() const;

	UPROPERTY(Transient)
	TArray<FVRExpGrabbableActiveGrip> ActiveGrips;
	TWeakObjectPtr<USceneComponent> LastGripRegisteredUpdatedComponent;
	EVRExpGrabbableMotionState MotionStateBeforePause;
	FTransform InitialTransform;
	FMotionAnchor InitialMotionAnchor;
	FMotionAnchor WanderOriginAnchor;
	FMotionAnchor WanderTargetAnchor;
	TWeakObjectPtr<USceneComponent> CurrentMotionParentComponent;
	FName CurrentMotionParentSocketName;
	FTransform CurrentMotionParentWorldTransform;
	bool bCurrentMotionSpaceIsRelative;
	TArray<TWeakObjectPtr<UActorComponent>> MotionTickPrerequisiteComponents;
	TArray<TWeakObjectPtr<AActor>> MotionTickPrerequisiteActors;
	UPROPERTY(Transient)
	TWeakObjectPtr<USceneComponent> InitialOwnerRootComponent;
	UPROPERTY(Transient)
	TWeakObjectPtr<USceneComponent> InitialAttachParentComponent;
	UPROPERTY(Transient)
	TWeakObjectPtr<USplineComponent> InitialSplineAttachmentComponent;
	FName InitialAttachSocketName;
	FTransform InitialOwnerRelativeTransform;
	FTransform InitialOwnerRelativeToSplineTransform;
	FVector WanderOrigin;
	FVector CurrentWanderTarget;
	FVector ReturnStartLocation;
	FRotator ReturnStartRotation;
	FVector SplineApproachTargetLocation;
	float SplineApproachTargetProgress;
	float SplineApproachFallSpeed;
	float AttachedKinematicFallSpeed;
	ESplineApproachPhase SplineApproachPhase;
	float ReturnProgress;
	float ReturnTotalDistance;
	float CurrentSplineProgress;
	float CurrentOrbitAngle;
	float WanderTimer;
	float FloatingEffectPhase;
	FVector AppliedFloatingOffset;
	FQuat AccumulatedRotationEffect;
	FQuat AppliedRotationEffect;
	EVRExpGrabbableMotionEffectSpace AppliedRotationEffectSpace;
	EVRExpGrabbableMotionEffectSpace CachedFloatingEffectSpace;
	EVRExpGrabbableMotionEffectSpace CachedRotationEffectSpace;
	bool bIsMovingToSpline;
	bool bMovementAppliedThisTick;
	bool bFloatingEffectWasEnabled;
	bool bRotationEffectWasActive;
	bool bWarnedAboutInvalidOrientationAxes;
	bool bWarnedAboutInvalidFloatingAxis;
	bool bHasInitialParentAttachment;
	bool bHasInitialRelativeToSplineTransform;
	bool bHasPendingReleaseAttachment;
	bool bAttachedKinematicFallActive;
	bool bPreservePendingReleaseAttachmentDuringMotionStart;
	EVRExpGrabbableReleaseMotionMode PendingReleaseAttachmentMode;
	FVRExpGrabbableReleaseAttachmentModeRule PendingReleaseAttachmentRule;
	bool bPendingAttachOwnerToSpline;
	bool bPendingReattachOwnerToInitialParent;
	bool bGripRegistrationInitialized;
	bool bRefreshingGripRegistration;
	int32 RuntimeMutationDepth;
	int64 RuntimeSnapshotSequence;
	FVRExpGrabbableMotionRuntimeSnapshot RuntimeMutationStartSnapshot;
	EVRExpGrabbableMotionChangeFlags PendingRuntimeChangeFlags;
	FVRExpGrabbableActiveGrip PendingChangedGrip;
	EVRExpGrabbableGripChangePhase PendingGripChangePhase;
	bool bPendingGripWasSocketed;
	bool bPendingGripIsFirst;
	bool bPendingGripIsFinal;
	bool bPendingLegacyGripBroadcast;
	bool bPendingLegacyMotionStateBroadcast;
	TArray<FVRExpPendingGripReleasedEvent> PendingGripReleasedEvents;
	mutable bool bWarnedAboutSweepWithoutPrimitive;

	friend class UVRExpGripEventRouterSubsystem;
	friend struct FVRExpScopedMotionRuntimeMutation;
	friend struct FVRExpGrabbableMotionTestAccessor;
};
