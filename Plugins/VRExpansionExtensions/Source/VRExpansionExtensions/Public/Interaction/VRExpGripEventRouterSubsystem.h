#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VRBPDatatypes.h"
#include "VRExpGripEventRouterSubsystem.generated.h"

class AActor;
class UGripMotionControllerComponent;
class UVRExpDetectableComponent;
class UVRExpGrabbableMotionComponent;
class UVRExpGripEventRouterControllerListener;

/**
 * World-local router that owns the only bindings to grip-controller events.
 * Motion components are authoritative receivers; detectable components register
 * directly only when they do not resolve a motion source.
 */
UCLASS()
class VREXPANSIONEXTENSIONS_API UVRExpGripEventRouterSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintCallable, Category = "VRExpansionExtensions|Grip")
	void RefreshGripControllers();

	void RegisterMotionComponent(UVRExpGrabbableMotionComponent* MotionComponent);
	void UnregisterMotionComponent(UVRExpGrabbableMotionComponent* MotionComponent);
	void RefreshMotionComponent(UVRExpGrabbableMotionComponent* MotionComponent);

	void RegisterDetectable(UVRExpDetectableComponent* DetectableComponent);
	void UnregisterDetectable(UVRExpDetectableComponent* DetectableComponent);
	void RefreshDetectable(UVRExpDetectableComponent* DetectableComponent);

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	struct FActiveGripKey
	{
		TWeakObjectPtr<UGripMotionControllerComponent> Controller;
		uint8 GripID = INVALID_VRGRIP_ID;

		bool operator==(const FActiveGripKey& Other) const
		{
			return Controller == Other.Controller && GripID == Other.GripID;
		}

		friend uint32 GetTypeHash(const FActiveGripKey& Key)
		{
			return HashCombine(GetTypeHash(Key.Controller), GetTypeHash(Key.GripID));
		}
	};

	struct FGripReceivers
	{
		TSet<TWeakObjectPtr<UVRExpGrabbableMotionComponent>> MotionComponents;
		TSet<TWeakObjectPtr<UVRExpDetectableComponent>> DetectableComponents;
		FBPActorGripInformation GripInformation;

		bool IsEmpty() const
		{
			return MotionComponents.IsEmpty() && DetectableComponents.IsEmpty();
		}
	};

	void HandleControllerGrip(
		UGripMotionControllerComponent* GripController,
		const FBPActorGripInformation& GripInformation);
	void HandleControllerDrop(
		UGripMotionControllerComponent* GripController,
		const FBPActorGripInformation& GripInformation,
		bool bWasSocketed);

	UFUNCTION()
	void HandleGripControllerOwnerDestroyed(AActor* DestroyedActor);

	void HandleActorSpawned(AActor* SpawnedActor);
	void BindGripController(UGripMotionControllerComponent* GripController);
	void UnbindAllGripControllers();
	void PruneInvalidControllersAndRoutes();

	void RebuildTargetRegistries();
	void AddMotionComponentToRegistry(UVRExpGrabbableMotionComponent* MotionComponent);
	void AddDetectableToRegistry(UVRExpDetectableComponent* DetectableComponent);
	void RebuildActiveGripRoutes(bool bResetRoutes);
	void RemoveMotionComponentFromActiveRoutes(UVRExpGrabbableMotionComponent* MotionComponent);
	void RemoveDetectableFromActiveRoutes(UVRExpDetectableComponent* DetectableComponent);

	void DispatchGripBegin(
		UGripMotionControllerComponent* GripController,
		const FBPActorGripInformation& GripInformation);
	void DispatchGripEnd(
		UGripMotionControllerComponent* GripController,
		const FBPActorGripInformation& GripInformation,
		bool bWasSocketed);
	void DispatchStoredGripEnd(
		const FActiveGripKey& GripKey,
		const FGripReceivers& Receivers,
		bool bWasSocketed);
	void GatherGripReceivers(
		UGripMotionControllerComponent* GripController,
		const FBPActorGripInformation& GripInformation,
		FGripReceivers& OutReceivers) const;
	void AddReceiversForGripTarget(
		UGripMotionControllerComponent* GripController,
		UObject* GripTarget,
		FGripReceivers& OutReceivers) const;

	TArray<TWeakObjectPtr<UGripMotionControllerComponent>> BoundGripControllers;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVRExpGripEventRouterControllerListener>>
		GripControllerListeners;

	TSet<TWeakObjectPtr<UVRExpGrabbableMotionComponent>> RegisteredMotionComponents;
	TSet<TWeakObjectPtr<UVRExpDetectableComponent>> RegisteredDetectables;
	TMap<TWeakObjectPtr<UObject>, TArray<TWeakObjectPtr<UVRExpGrabbableMotionComponent>>> MotionComponentsByGripTarget;
	TMap<TWeakObjectPtr<UObject>, TArray<TWeakObjectPtr<UVRExpDetectableComponent>>> DetectablesByGripTarget;
	TMap<FActiveGripKey, FGripReceivers> ActiveGripReceivers;
	FDelegateHandle ActorSpawnedDelegateHandle;
	float GripControllerRefreshAccumulator = 0.0f;

	friend class UVRExpGripEventRouterControllerListener;
};
