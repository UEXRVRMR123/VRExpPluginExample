// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/HitResult.h"
#include "GameFramework/MovementComponent.h"
#include "GripMotionControllerComponent.h"
#include "VRExpGrabbableMotionComponent.generated.h"

class AActor;
class UPrimitiveComponent;
class USceneComponent;

/** 可抓取物体未被抓取时的常态运动模式。 */
UENUM(BlueprintType)
enum class EVRExpGrabbableNormalMotionMode : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "不执行样条、跟随、环绕或游荡等基础运动；启用的浮动和自转效果仍可运行。"),
	FollowSpline UMETA(DisplayName = "Follow Spline", ToolTip = "沿指定 Spline 组件移动，可用于轨道、巡游、漂浮路径。"),
	FollowTarget UMETA(DisplayName = "Follow Target", ToolTip = "跟随指定目标组件，并保持配置的跟随距离。"),
	OrbitTarget UMETA(DisplayName = "Orbit Target", ToolTip = "围绕指定中心组件环绕运动。"),
	RandomWander UMETA(DisplayName = "Random Wander", ToolTip = "在运动原点附近随机游荡。")
};

/** 物体从 VRExpansionPlugin 抓取中释放后的运动处理模式。 */
UENUM(BlueprintType)
enum class EVRExpGrabbableReleaseMotionMode : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "释放后不接管运动，保持 VRExpansionPlugin 或物理系统当前状态。"),
	ReturnToStart UMETA(DisplayName = "Return To Start", ToolTip = "释放后返回组件 BeginPlay 时记录的初始 Transform。"),
	ReturnToSpline UMETA(DisplayName = "Return To Spline", ToolTip = "释放后返回 Spline 上距离当前位置最近的点，然后恢复常态运动。"),
	ReturnToOrigin UMETA(DisplayName = "Return To Origin", ToolTip = "释放后返回运动原点，通常是 Random Wander 启动时记录的位置。"),
	ContinueMotion UMETA(DisplayName = "Continue Motion", ToolTip = "释放后直接恢复当前配置的常态运动模式。"),
	FallWithGravity UMETA(DisplayName = "Fall With Gravity", ToolTip = "释放后开启 UpdatedComponent 的物理模拟，让物体受重力下落。"),
	FlyToTarget UMETA(DisplayName = "Fly To Target", ToolTip = "释放后飞向指定目标组件，到达后停止接管运动。")
};

/** 当前可抓取运动组件的运行状态。 */
UENUM(BlueprintType)
enum class EVRExpGrabbableMotionState : uint8
{
	Idle UMETA(DisplayName = "Idle", ToolTip = "空闲状态，组件当前不主动驱动物体运动。"),
	NormalMotion UMETA(DisplayName = "Normal Motion", ToolTip = "常态运动中，例如沿 Spline、跟随、环绕或随机游荡。"),
	Grabbed UMETA(DisplayName = "Grabbed", ToolTip = "物体正在被 VRExpansionPlugin 抓取，组件暂停自身运动控制。"),
	Releasing UMETA(DisplayName = "Releasing", ToolTip = "物体已释放，组件正在执行释放后的返回、飞行或其他接管运动。"),
	Paused UMETA(DisplayName = "Paused", ToolTip = "组件被手动暂停，保持当前状态但不继续 Tick 运动。")
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
	void RefreshGripControllerBindings();

	UFUNCTION(BlueprintPure, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableMotionState GetMotionState() const
	{
		return CurrentMotionState;
	}

	UPROPERTY(BlueprintAssignable, Category = "VRExp|Grabbable Motion")
	FVRExpGrabbableMotionStateChangedSignature OnMotionStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|Grabbable Motion")
	FVRExpGrabbableReturnMotionCompletedSignature OnReturnMotionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|Grabbable Motion")
	FVRExpGrabbableMovementBlockedSignature OnMovementBlocked;

	/** 当匹配到本组件目标的 Grip 被释放时触发；组件内置释放处理会先执行，蓝图可在此事件中覆盖最终行为。多手抓取时 bIsFinalGrip 表示是否已经没有其他有效抓取。 */
	UPROPERTY(BlueprintAssignable, Category = "VRExp|Grabbable Motion|Grip")
	FVRExpGrabbableGripReleasedSignature OnGripReleased;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableNormalMotionMode NormalMotionMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableReleaseMotionMode ReleaseMotionMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion")
	bool bAutoStartNormalMotion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion")
	bool bPauseNormalMotionWhenGrabbed;

	/** 是否允许本组件在移动 UpdatedComponent 时更新其旋转；关闭后组件只更新位置，不覆盖外部系统设置的当前旋转。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Orientation")
	bool bUpdateRotationDuringMotion;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Grip")
	bool bAutoBindGripControllers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Grip")
	TArray<TObjectPtr<UGripMotionControllerComponent>> ManualGripControllers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Grip")
	bool bMatchOwnerActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Grip")
	bool bMatchUpdatedComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Grip")
	bool bMatchOwnerComponents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline"))
	bool bUseManualSplineReference;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline && bUseManualSplineReference", EditConditionHides))
	bool bUseSplineActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline && bUseManualSplineReference && bUseSplineActor", EditConditionHides))
	TObjectPtr<AActor> SplineActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline && bUseManualSplineReference && !bUseSplineActor", EditConditionHides))
	FName SplineComponentName;

	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion|Spline")
	TObjectPtr<USplineComponent> SplineToFollow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline"))
	bool bTeleportToSplineOnStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline", ClampMin = "0.0", UIMin = "0.0"))
	float SplineSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline", ClampMin = "0.1", UIMin = "0.1"))
	float SplineArrivalThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline"))
	bool bLoopSpline;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Spline", meta = (EditCondition = "NormalMotionMode == EVRExpGrabbableNormalMotionMode::FollowSpline", ToolTip = "沿 Spline 反向移动，并让物体朝向实际移动方向。"))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ReturnSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release")
	bool bSmoothReturn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release", meta = (EditCondition = "ReleaseMotionMode == EVRExpGrabbableReleaseMotionMode::ReturnToStart"))
	TObjectPtr<UCurveFloat> ReturnCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Grabbable Motion|Release", meta = (EditCondition = "ReleaseMotionMode == EVRExpGrabbableReleaseMotionMode::FlyToTarget", EditConditionHides))
	TObjectPtr<USceneComponent> FlyToTargetComponent;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "VRExp|Grabbable Motion")
	EVRExpGrabbableMotionState CurrentMotionState;

private:
	struct FVRExpActiveGrip
	{
		TWeakObjectPtr<UGripMotionControllerComponent> Controller;
		uint8 GripID = INVALID_VRGRIP_ID;
		bool bHadMovementAuthority = false;
	};

	UFUNCTION()
	void HandleControllerGrip(const FBPActorGripInformation& GripInformation);

	UFUNCTION()
	void HandleControllerDrop(const FBPActorGripInformation& GripInformation, bool bWasSocketed);

	void ResolveUpdatedComponent();
	void BindGripController(UGripMotionControllerComponent* GripController);
	void UnbindGripControllers();
	bool DoesGripMatchTarget(const FBPActorGripInformation& GripInformation) const;
	void AddActiveGrip(UGripMotionControllerComponent* GripController, const FBPActorGripInformation& GripInformation, bool bHasMovementAuthority);
	void RebuildActiveGripsFromControllers();
	void PruneInvalidActiveGrips();

	void SetMotionState(EVRExpGrabbableMotionState NewState);
	void StartNormalMotion();
	void StartReleaseMotion();
	void StartReleaseMotionFromDrop();
	void FinishReleaseMotion(bool bResumeNormalMotion);

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

	bool InitializeSplineReference();
	void GenerateNewWanderTarget();
	void PrepareForKinematicMotion();
	void SetUpdatedPrimitiveSimulatePhysics(bool bSimulate);
	bool ShouldSweepMovement() const;
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
	bool MoveUpdatedComponentTo(const FVector& TargetLocation, const FQuat& TargetRotation, float DeltaTime, bool bForceNoSweep = false, ETeleportType Teleport = ETeleportType::None);

	FTransform GetUpdatedComponentTransform() const;
	FVector GetUpdatedComponentLocation() const;
	FQuat GetUpdatedComponentQuat() const;
	FVector GetMotionBaseLocation() const;
	FQuat GetMotionBaseQuat() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGripMotionControllerComponent>> BoundGripControllers;

	TArray<FVRExpActiveGrip> ActiveGrips;
	EVRExpGrabbableMotionState MotionStateBeforePause;
	FTransform InitialTransform;
	FVector WanderOrigin;
	FVector CurrentWanderTarget;
	FVector ReturnStartLocation;
	FRotator ReturnStartRotation;
	FVector ReturnToSplineTargetLocation;
	float ReturnToSplineTargetProgress;
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
	mutable bool bWarnedAboutSweepWithoutPrimitive;
};
