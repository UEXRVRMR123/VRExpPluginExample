// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Interaction/VRExpGrabbableMotionComponent.h"
#include "VRExpPuzzleInsertionComponent.generated.h"

class AActor;
class AGrippableStaticMeshActor;
class UPrimitiveComponent;
class USceneComponent;

/** 一对一拼图嵌入流程的当前状态。 */
UENUM(BlueprintType)
enum class EVRExpPuzzleInsertionState : uint8
{
	Idle UMETA(DisplayName = "Idle", ToolTip = "等待正确拼图被抓取并进入触发碰撞体。"),
	WaitingForRelease UMETA(DisplayName = "Waiting For Release", ToolTip = "已禁止重新抓取，正在等待全部 Grip 释放。"),
	Approaching UMETA(DisplayName = "Approaching", ToolTip = "移动到指引嵌入轴外侧，并将拼图的同一本地轴对齐到指引嵌入轴；保留绕该轴的旋转。"),
	AligningUp UMETA(DisplayName = "Aligning Axis Rotation", ToolTip = "位置保持不变，只补齐绕配置嵌入轴的旋转，使拼图旋转与指引 Actor 完全一致。"),
	Inserting UMETA(DisplayName = "Inserting", ToolTip = "保持最终旋转，沿配置嵌入轴的反方向移动到最终嵌入位置。"),
	Completed UMETA(DisplayName = "Completed", ToolTip = "拼图已完成嵌入并被锁定。")
};

/** 一对一拼图三阶段嵌入的空间与时间配置。 */
USTRUCT(BlueprintType)
struct FVRExpPuzzleInsertionSettings
{
	GENERATED_BODY()

	FVRExpPuzzleInsertionSettings()
		: InsertionAxis(EVRExpGrabbableMotionAxis::Z)
		, ApproachDistance(20.0f)
		, ApproachDuration(0.30f)
		, UpAlignmentDuration(0.25f)
		, InsertionDuration(0.35f)
		, EaseExponent(2.0f)
		, InterpolationCurve(nullptr)
	{
	}

	/** 拼图与指引 Actor 共同使用的本地嵌入轴；该轴正方向指向嵌入位置外侧。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Puzzle Insertion")
	EVRExpGrabbableMotionAxis InsertionAxis;

	/** 第一阶段目标点沿指引 Actor 的 InsertionAxis 到最终嵌入点的距离，单位为厘米。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Puzzle Insertion", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float ApproachDistance;

	/** 第一阶段耗时，单位为秒；为零时直接完成该阶段。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Puzzle Insertion", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float ApproachDuration;

	/** 第二阶段绕配置嵌入轴补齐剩余旋转的耗时，单位为秒；为零时直接完成该阶段。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Puzzle Insertion", meta = (DisplayName = "Axis Rotation Alignment Duration", ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float UpAlignmentDuration;

	/** 第三阶段沿配置嵌入轴的反方向嵌入的耗时，单位为秒；为零时直接完成该阶段。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Puzzle Insertion", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float InsertionDuration;

	/** 未配置 InterpolationCurve 时，EaseInOut 插值使用的指数。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Puzzle Insertion", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float EaseExponent;

	/** 可选的归一化 0-1 插值曲线；输入为阶段进度，输出会限制到 0-1。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Puzzle Insertion")
	TObjectPtr<UCurveFloat> InterpolationCurve;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVRExpPuzzleInsertionStateChangedSignature, EVRExpPuzzleInsertionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVRExpPuzzleInsertionEventSignature);

/**
 * 挂在拼图指引 Actor 上，接管一个指定 AGrippableStaticMeshActor 的松手与三阶段嵌入流程。
 * 指引 Actor 与拼图 Actor 共用 InsertionAxis 作为本地嵌入轴；指引位置和旋转就是最终拼图位置和旋转，指引缩放不参与计算。
 */
UCLASS(Blueprintable, ClassGroup = (VRExp), meta = (BlueprintSpawnableComponent, DisplayName = "VRExp Puzzle Insertion Component"))
class VREXPANSIONEXTENSIONS_API UVRExpPuzzleInsertionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVRExpPuzzleInsertionComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 手动尝试启动嵌入；拼图必须处于被抓取状态。 */
	UFUNCTION(BlueprintCallable, Category = "VRExp|Puzzle Insertion")
	bool TryStartInsertion();

	/** 恢复拼图和触发碰撞体的初始状态；拼图仍被抓取时拒绝执行。 */
	UFUNCTION(BlueprintCallable, Category = "VRExp|Puzzle Insertion")
	bool ResetInsertion();

	/** 设置当前指引唯一对应的拼图；在 BeginPlay 后通过蓝图赋值时会立即刷新事件绑定与重置基准。 */
	UFUNCTION(BlueprintCallable, BlueprintSetter, Category = "VRExp|Puzzle Insertion")
	void SetPuzzleActor(AGrippableStaticMeshActor* NewPuzzleActor);

	UFUNCTION(BlueprintPure, Category = "VRExp|Puzzle Insertion")
	EVRExpPuzzleInsertionState GetInsertionState() const
	{
		return CurrentState;
	}

	UFUNCTION(BlueprintPure, Category = "VRExp|Puzzle Insertion")
	bool IsInsertionComplete() const
	{
		return CurrentState == EVRExpPuzzleInsertionState::Completed;
	}

	/** 当前指引 Actor 唯一对应的拼图实例。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetPuzzleActor, Category = "VRExp|Puzzle Insertion", meta = (DisplayName = "Puzzle Actor (One-to-One)"))
	TObjectPtr<AGrippableStaticMeshActor> PuzzleActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Puzzle Insertion")
	FVRExpPuzzleInsertionSettings InsertionSettings;

	/** 强制 DropGrip 后等待最终释放确认的最长时间，单位为秒。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Puzzle Insertion", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float ReleaseConfirmationTimeout;

	/** 完成后是否使用 KeepWorldTransform 将拼图附着到指引 Actor。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Puzzle Insertion")
	bool bAttachToGuideOnComplete;

	/** ResetInsertion 后是否重新调用拼图运动组件的 StartMotion。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExp|Puzzle Insertion")
	bool bRestartPuzzleMotionOnReset;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|Puzzle Insertion")
	FVRExpPuzzleInsertionStateChangedSignature OnInsertionStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|Puzzle Insertion")
	FVRExpPuzzleInsertionEventSignature OnInsertionStarted;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|Puzzle Insertion")
	FVRExpPuzzleInsertionEventSignature OnInsertionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|Puzzle Insertion")
	FVRExpPuzzleInsertionEventSignature OnInsertionFailed;

	UPROPERTY(BlueprintAssignable, Category = "VRExp|Puzzle Insertion")
	FVRExpPuzzleInsertionEventSignature OnInsertionReset;

private:
	UFUNCTION()
	void HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandlePuzzleGripReleased(const FBPActorGripInformation& GripInformation, bool bWasSocketed, bool bIsFinalGrip, bool bHadMovementAuthority);

	bool ResolveAndCacheReferences();
	bool QueryPuzzleHeld(TArray<FBPGripPair>* OutHoldingControllers = nullptr) const;
	bool RequestReleaseAllGrips();
	void BeginInsertionMotion();
	void AdvanceInsertion(float DeltaTime);
	void ApplyCurrentPhase(float Alpha);
	void AdvanceToNextPhase();
	void CompleteInsertion();
	void FailInsertion(const FString& Reason);
	void RestoreAfterFailedRelease();
	void SetInsertionState(EVRExpPuzzleInsertionState NewState);
	float GetCurrentPhaseDuration() const;
	float EvaluateInterpolationAlpha(float LinearAlpha) const;
	void SetPuzzleLocationAndRotation(const FVector& Location, const FQuat& Rotation);
	void StopPuzzlePhysics();
	void RestoreInitialAttachmentAndTransform();

	UPROPERTY(Transient)
	TObjectPtr<UVRExpGrabbableMotionComponent> PuzzleMotionComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> ResolvedTriggerComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> PuzzleRootPrimitive;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "VRExp|Puzzle Insertion", meta = (AllowPrivateAccess = "true"))
	EVRExpPuzzleInsertionState CurrentState;

	FTransform InitialPuzzleWorldTransform;
	FTransform InitialPuzzleRelativeTransform;
	TWeakObjectPtr<USceneComponent> InitialAttachParent;
	FName InitialAttachSocketName;
	ECollisionEnabled::Type InitialTriggerCollisionEnabled;
	bool bInitialPuzzleSimulatesPhysics;
	bool bInitialPuzzleDenyGripping;
	bool bInitialTriggerGenerateOverlapEvents;
	bool bInitialStateCached;

	bool bDenyGrippingBeforeAttempt;
	bool bTriggerOverlapBeforeAttempt;
	bool bLoggedInitializationFailure;
	bool bLoggedTriggerConfigurationWarning;
	bool bLoggedResetWhileHeld;

	float ReleaseWaitElapsed;
	float PhaseElapsed;
	FVector PhaseStartLocation;
	FQuat PhaseStartRotation;
	FVector ApproachLocation;
	FQuat ApproachRotation;
	FVector FinalInsertionLocation;
	FQuat FinalInsertionRotation;
	FVector InsertionAxisWorld;
};
