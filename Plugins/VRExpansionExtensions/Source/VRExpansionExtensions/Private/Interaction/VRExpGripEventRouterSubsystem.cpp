#include "Interaction/VRExpGripEventRouterSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GripMotionControllerComponent.h"
#include "UObject/UObjectIterator.h"
#include "Detection/VRExpDetectableComponent.h"
#include "Interaction/VRExpGrabbableMotionComponent.h"
#include "Interaction/VRExpGripEventRouterControllerListener.h"

namespace
{
	constexpr float GripControllerRefreshIntervalSeconds = 1.0f;
}

void UVRExpGripEventRouterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GripControllerRefreshAccumulator = 0.0f;

	if (UWorld* World = GetWorld())
	{
		ActorSpawnedDelegateHandle = World->AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateUObject(
				this,
				&UVRExpGripEventRouterSubsystem::HandleActorSpawned));
	}

	RefreshGripControllers();
}

void UVRExpGripEventRouterSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (ActorSpawnedDelegateHandle.IsValid())
		{
			World->RemoveOnActorSpawnedHandler(ActorSpawnedDelegateHandle);
			ActorSpawnedDelegateHandle.Reset();
		}
	}

	UnbindAllGripControllers();
	ActiveGripReceivers.Reset();
	MotionComponentsByGripTarget.Reset();
	DetectablesByGripTarget.Reset();
	RegisteredMotionComponents.Reset();
	RegisteredDetectables.Reset();

	Super::Deinitialize();
}

void UVRExpGripEventRouterSubsystem::Tick(float DeltaTime)
{
	PruneInvalidControllersAndRoutes();

	GripControllerRefreshAccumulator += FMath::Max(0.0f, DeltaTime);
	if (GripControllerRefreshAccumulator >=
		GripControllerRefreshIntervalSeconds)
	{
		GripControllerRefreshAccumulator = 0.0f;
		RefreshGripControllers();
	}
}

bool UVRExpGripEventRouterSubsystem::IsTickable() const
{
	return !IsTemplate(RF_ClassDefaultObject) &&
		(!RegisteredMotionComponents.IsEmpty() ||
		 !RegisteredDetectables.IsEmpty());
}

UWorld* UVRExpGripEventRouterSubsystem::GetTickableGameObjectWorld() const
{
	return GetWorld();
}

ETickableTickType UVRExpGripEventRouterSubsystem::GetTickableTickType() const
{
	return IsTemplate(RF_ClassDefaultObject)
		? ETickableTickType::Never
		: ETickableTickType::Conditional;
}

TStatId UVRExpGripEventRouterSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(
		UVRExpGripEventRouterSubsystem,
		STATGROUP_Tickables);
}

bool UVRExpGripEventRouterSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UVRExpGripEventRouterSubsystem::RefreshGripControllers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	PruneInvalidControllersAndRoutes();

	for (TObjectIterator<UGripMotionControllerComponent> Iterator; Iterator; ++Iterator)
	{
		UGripMotionControllerComponent* GripController = *Iterator;
		if (IsValid(GripController) &&
			!GripController->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) &&
			GripController->GetWorld() == World)
		{
			BindGripController(GripController);
		}
	}

	RebuildTargetRegistries();
	RebuildActiveGripRoutes(true);
}

void UVRExpGripEventRouterSubsystem::RegisterMotionComponent(
	UVRExpGrabbableMotionComponent* MotionComponent)
{
	if (!IsValid(MotionComponent) || RegisteredMotionComponents.Contains(MotionComponent))
	{
		return;
	}

	RegisteredMotionComponents.Add(MotionComponent);
	AddMotionComponentToRegistry(MotionComponent);
	RebuildActiveGripRoutes(false);
}

void UVRExpGripEventRouterSubsystem::UnregisterMotionComponent(
	UVRExpGrabbableMotionComponent* MotionComponent)
{
	if (!MotionComponent)
	{
		return;
	}

	RegisteredMotionComponents.Remove(MotionComponent);
	RemoveMotionComponentFromActiveRoutes(MotionComponent);
	RebuildTargetRegistries();
}

void UVRExpGripEventRouterSubsystem::RefreshMotionComponent(
	UVRExpGrabbableMotionComponent* MotionComponent)
{
	if (!IsValid(MotionComponent))
	{
		return;
	}

	RegisteredMotionComponents.Add(MotionComponent);
	RemoveMotionComponentFromActiveRoutes(MotionComponent);
	RebuildTargetRegistries();
	RebuildActiveGripRoutes(false);
}

void UVRExpGripEventRouterSubsystem::RegisterDetectable(
	UVRExpDetectableComponent* DetectableComponent)
{
	if (!IsValid(DetectableComponent) || RegisteredDetectables.Contains(DetectableComponent))
	{
		return;
	}

	RegisteredDetectables.Add(DetectableComponent);
	AddDetectableToRegistry(DetectableComponent);
	RebuildActiveGripRoutes(false);
}

void UVRExpGripEventRouterSubsystem::UnregisterDetectable(
	UVRExpDetectableComponent* DetectableComponent)
{
	if (!DetectableComponent)
	{
		return;
	}

	RegisteredDetectables.Remove(DetectableComponent);
	RemoveDetectableFromActiveRoutes(DetectableComponent);
	RebuildTargetRegistries();
}

void UVRExpGripEventRouterSubsystem::RefreshDetectable(
	UVRExpDetectableComponent* DetectableComponent)
{
	if (!IsValid(DetectableComponent))
	{
		return;
	}

	RegisteredDetectables.Add(DetectableComponent);
	RemoveDetectableFromActiveRoutes(DetectableComponent);
	RebuildTargetRegistries();
	RebuildActiveGripRoutes(false);
}

void UVRExpGripEventRouterSubsystem::HandleControllerGrip(
	UGripMotionControllerComponent* GripController,
	const FBPActorGripInformation& GripInformation)
{
	DispatchGripBegin(GripController, GripInformation);
}

void UVRExpGripEventRouterSubsystem::HandleControllerDrop(
	UGripMotionControllerComponent* GripController,
	const FBPActorGripInformation& GripInformation,
	bool bWasSocketed)
{
	DispatchGripEnd(GripController, GripInformation, bWasSocketed);
}

void UVRExpGripEventRouterSubsystem::HandleGripControllerOwnerDestroyed(
	AActor* DestroyedActor)
{
	if (!DestroyedActor)
	{
		return;
	}

	TArray<TPair<FActiveGripKey, FGripReceivers>> RoutesToEnd;
	for (const TPair<FActiveGripKey, FGripReceivers>& Route :
		 ActiveGripReceivers)
	{
		UGripMotionControllerComponent* GripController =
			Route.Key.Controller.Get();
		if (GripController &&
			GripController->GetOwner() == DestroyedActor)
		{
			RoutesToEnd.Add(Route);
		}
	}

	for (const TPair<FActiveGripKey, FGripReceivers>& Route :
		 RoutesToEnd)
	{
		DispatchStoredGripEnd(Route.Key, Route.Value, false);
	}

	PruneInvalidControllersAndRoutes();
}

void UVRExpGripEventRouterSubsystem::HandleActorSpawned(AActor* SpawnedActor)
{
	if (!IsValid(SpawnedActor))
	{
		return;
	}

	TInlineComponentArray<UGripMotionControllerComponent*> GripControllers(SpawnedActor);
	for (UGripMotionControllerComponent* GripController : GripControllers)
	{
		BindGripController(GripController);
	}
}

void UVRExpGripEventRouterSubsystem::BindGripController(
	UGripMotionControllerComponent* GripController)
{
	if (!IsValid(GripController) || GripController->GetWorld() != GetWorld())
	{
		return;
	}

	for (const TWeakObjectPtr<UGripMotionControllerComponent>& ExistingController : BoundGripControllers)
	{
		if (ExistingController.Get() == GripController)
		{
			return;
		}
	}

	UVRExpGripEventRouterControllerListener* Listener =
		NewObject<UVRExpGripEventRouterControllerListener>(this);
	Listener->Initialize(this, GripController);
	GripControllerListeners.Add(Listener);
	BoundGripControllers.Add(GripController);
	if (AActor* ControllerOwner = GripController->GetOwner())
	{
		ControllerOwner->OnDestroyed.AddUniqueDynamic(
			this,
			&UVRExpGripEventRouterSubsystem::HandleGripControllerOwnerDestroyed);
	}

	TArray<FBPActorGripInformation> ExistingGrips;
	GripController->GetAllGrips(ExistingGrips);
	TArray<TWeakObjectPtr<UVRExpGrabbableMotionComponent>>
		BatchedMotionComponents;
	if (!ExistingGrips.IsEmpty())
	{
		for (const TWeakObjectPtr<UVRExpGrabbableMotionComponent>&
			 MotionComponent : RegisteredMotionComponents)
		{
			if (MotionComponent.IsValid() &&
				MotionComponent->AcceptsGripController(GripController))
			{
				MotionComponent->BeginRuntimeMutation();
				MotionComponent->MarkRuntimeChange(
					EVRExpGrabbableMotionChangeFlags::RegistrationRebuilt);
				BatchedMotionComponents.Add(MotionComponent);
			}
		}
	}

	for (const FBPActorGripInformation& GripInformation : ExistingGrips)
	{
		DispatchGripBegin(GripController, GripInformation);
	}
	for (int32 Index = BatchedMotionComponents.Num() - 1;
		 Index >= 0;
		 --Index)
	{
		if (BatchedMotionComponents[Index].IsValid())
		{
			BatchedMotionComponents[Index]->EndRuntimeMutation();
		}
	}
}

void UVRExpGripEventRouterSubsystem::UnbindAllGripControllers()
{
	TSet<TWeakObjectPtr<AActor>> ControllerOwners;
	for (UVRExpGripEventRouterControllerListener* Listener :
		 GripControllerListeners)
	{
		if (IsValid(Listener))
		{
			if (AActor* ControllerOwner =
					Listener->GetGripControllerOwner())
			{
				ControllerOwners.Add(ControllerOwner);
			}
			Listener->Deinitialize();
		}
	}

	for (const TWeakObjectPtr<AActor>& ControllerOwner : ControllerOwners)
	{
		if (ControllerOwner.IsValid())
		{
			ControllerOwner->OnDestroyed.RemoveDynamic(
				this,
				&UVRExpGripEventRouterSubsystem::HandleGripControllerOwnerDestroyed);
		}
	}

	GripControllerListeners.Reset();
	BoundGripControllers.Reset();
}

void UVRExpGripEventRouterSubsystem::PruneInvalidControllersAndRoutes()
{
	bool bRegistryChanged = false;
	TArray<TWeakObjectPtr<UVRExpGrabbableMotionComponent>>
		MotionComponentsToRefresh;
	for (auto Iterator = RegisteredMotionComponents.CreateIterator();
		 Iterator;
		 ++Iterator)
	{
		if (!Iterator->IsValid())
		{
			Iterator.RemoveCurrent();
			bRegistryChanged = true;
		}
		else if ((*Iterator)->HasGripRegistrationTargetChanged())
		{
			MotionComponentsToRefresh.Add(*Iterator);
		}
	}
	for (auto Iterator = RegisteredDetectables.CreateIterator();
		 Iterator;
		 ++Iterator)
	{
		if (!Iterator->IsValid())
		{
			Iterator.RemoveCurrent();
			bRegistryChanged = true;
		}
	}
	if (bRegistryChanged)
	{
		RebuildTargetRegistries();
	}
	for (const TWeakObjectPtr<UVRExpGrabbableMotionComponent>&
			 MotionComponent : MotionComponentsToRefresh)
	{
		if (MotionComponent.IsValid())
		{
			MotionComponent->RefreshGripRegistration();
		}
	}

	TArray<TPair<FActiveGripKey, FGripReceivers>> RoutesToEnd;
	for (TPair<FActiveGripKey, FGripReceivers>& Route :
		 ActiveGripReceivers)
	{
		for (auto Iterator =
				 Route.Value.MotionComponents.CreateIterator();
			 Iterator;
			 ++Iterator)
		{
			if (!Iterator->IsValid())
			{
				Iterator.RemoveCurrent();
			}
		}
		for (auto Iterator =
				 Route.Value.DetectableComponents.CreateIterator();
			 Iterator;
			 ++Iterator)
		{
			if (!Iterator->IsValid())
			{
				Iterator.RemoveCurrent();
			}
		}

		if (!Route.Key.Controller.IsValid())
		{
			RoutesToEnd.Add(Route);
		}
	}

	for (const TPair<FActiveGripKey, FGripReceivers>& Route :
		 RoutesToEnd)
	{
		DispatchStoredGripEnd(Route.Key, Route.Value, false);
	}

	TSet<TWeakObjectPtr<AActor>> OwnersToRefresh;
	for (int32 Index = GripControllerListeners.Num() - 1;
		 Index >= 0;
		 --Index)
	{
		UVRExpGripEventRouterControllerListener* Listener =
			GripControllerListeners[Index];
		if (!IsValid(Listener) ||
			!IsValid(Listener->GetGripController()))
		{
			if (IsValid(Listener))
			{
				OwnersToRefresh.Add(
					Listener->GetGripControllerOwner());
				Listener->Deinitialize();
			}
			GripControllerListeners.RemoveAtSwap(Index);
		}
	}

	for (const TWeakObjectPtr<AActor>& ControllerOwner :
		 OwnersToRefresh)
	{
		if (ControllerOwner.IsValid())
		{
			ControllerOwner->OnDestroyed.RemoveDynamic(
				this,
				&UVRExpGripEventRouterSubsystem::HandleGripControllerOwnerDestroyed);
		}
	}

	BoundGripControllers.Reset();
	for (UVRExpGripEventRouterControllerListener* Listener :
		 GripControllerListeners)
	{
		if (IsValid(Listener) &&
			IsValid(Listener->GetGripController()))
		{
			BoundGripControllers.AddUnique(
				Listener->GetGripController());
			if (AActor* ControllerOwner =
					Listener->GetGripControllerOwner())
			{
				ControllerOwner->OnDestroyed.AddUniqueDynamic(
					this,
					&UVRExpGripEventRouterSubsystem::HandleGripControllerOwnerDestroyed);
			}
		}
	}
}

void UVRExpGripEventRouterSubsystem::RebuildTargetRegistries()
{
	MotionComponentsByGripTarget.Reset();
	DetectablesByGripTarget.Reset();

	for (auto Iterator = RegisteredMotionComponents.CreateIterator(); Iterator; ++Iterator)
	{
		UVRExpGrabbableMotionComponent* MotionComponent = Iterator->Get();
		if (!IsValid(MotionComponent))
		{
			Iterator.RemoveCurrent();
			continue;
		}
		AddMotionComponentToRegistry(MotionComponent);
	}

	for (auto Iterator = RegisteredDetectables.CreateIterator(); Iterator; ++Iterator)
	{
		UVRExpDetectableComponent* DetectableComponent = Iterator->Get();
		if (!IsValid(DetectableComponent))
		{
			Iterator.RemoveCurrent();
			continue;
		}
		AddDetectableToRegistry(DetectableComponent);
	}
}

void UVRExpGripEventRouterSubsystem::AddMotionComponentToRegistry(
	UVRExpGrabbableMotionComponent* MotionComponent)
{
	if (!IsValid(MotionComponent))
	{
		return;
	}

	TArray<UObject*> GripTargets;
	MotionComponent->GetGripRoutingTargets(GripTargets);
	for (UObject* GripTarget : GripTargets)
	{
		if (IsValid(GripTarget))
		{
			MotionComponentsByGripTarget
				.FindOrAdd(TWeakObjectPtr<UObject>(GripTarget))
				.AddUnique(MotionComponent);
		}
	}
}

void UVRExpGripEventRouterSubsystem::AddDetectableToRegistry(
	UVRExpDetectableComponent* DetectableComponent)
{
	if (!IsValid(DetectableComponent))
	{
		return;
	}

	TArray<UObject*> GripTargets;
	DetectableComponent->GetGripTargets(GripTargets);
	for (UObject* GripTarget : GripTargets)
	{
		if (IsValid(GripTarget))
		{
			DetectablesByGripTarget
				.FindOrAdd(TWeakObjectPtr<UObject>(GripTarget))
				.AddUnique(DetectableComponent);
		}
	}
}

void UVRExpGripEventRouterSubsystem::RebuildActiveGripRoutes(bool bResetRoutes)
{
	TMap<FActiveGripKey, FBPActorGripInformation> CurrentGrips;

	for (const TWeakObjectPtr<UGripMotionControllerComponent>& GripController : BoundGripControllers)
	{
		if (!GripController.IsValid())
		{
			continue;
		}

		TArray<FBPActorGripInformation> ControllerGrips;
		GripController->GetAllGrips(ControllerGrips);
		for (const FBPActorGripInformation& GripInformation : ControllerGrips)
		{
			if (GripInformation.IsValid())
			{
				CurrentGrips.Add(
					FActiveGripKey{
						GripController.Get(),
						GripInformation.GripID},
					GripInformation);
			}
		}
	}

	if (bResetRoutes)
	{
		TArray<TPair<FActiveGripKey, FGripReceivers>> RoutesToEnd;
		for (const TPair<FActiveGripKey, FGripReceivers>& Route :
			 ActiveGripReceivers)
		{
			if (!CurrentGrips.Contains(Route.Key))
			{
				RoutesToEnd.Add(Route);
			}
		}

		for (const TPair<FActiveGripKey, FGripReceivers>& Route :
			 RoutesToEnd)
		{
			DispatchStoredGripEnd(Route.Key, Route.Value, false);
		}
	}

	for (const TPair<FActiveGripKey, FBPActorGripInformation>& Grip :
		 CurrentGrips)
	{
		DispatchGripBegin(
			Grip.Key.Controller.Get(),
			Grip.Value);
	}
}

void UVRExpGripEventRouterSubsystem::RemoveMotionComponentFromActiveRoutes(
	UVRExpGrabbableMotionComponent* MotionComponent)
{
	for (auto Iterator = ActiveGripReceivers.CreateIterator(); Iterator; ++Iterator)
	{
		Iterator.Value().MotionComponents.Remove(MotionComponent);
		if (Iterator.Value().IsEmpty())
		{
			Iterator.RemoveCurrent();
		}
	}
}

void UVRExpGripEventRouterSubsystem::RemoveDetectableFromActiveRoutes(
	UVRExpDetectableComponent* DetectableComponent)
{
	for (auto Iterator = ActiveGripReceivers.CreateIterator(); Iterator; ++Iterator)
	{
		Iterator.Value().DetectableComponents.Remove(DetectableComponent);
		if (Iterator.Value().IsEmpty())
		{
			Iterator.RemoveCurrent();
		}
	}
}

void UVRExpGripEventRouterSubsystem::DispatchGripBegin(
	UGripMotionControllerComponent* GripController,
	const FBPActorGripInformation& GripInformation)
{
	if (!IsValid(GripController) || !GripInformation.IsValid())
	{
		return;
	}

	FGripReceivers Receivers;
	GatherGripReceivers(GripController, GripInformation, Receivers);
	const FActiveGripKey GripKey{GripController, GripInformation.GripID};
	FGripReceivers& StoredReceivers = ActiveGripReceivers.FindOrAdd(GripKey);
	StoredReceivers.GripInformation = GripInformation;

	for (const TWeakObjectPtr<UVRExpGrabbableMotionComponent>& Receiver : Receivers.MotionComponents)
	{
		if (Receiver.IsValid() && !StoredReceivers.MotionComponents.Contains(Receiver))
		{
			StoredReceivers.MotionComponents.Add(Receiver);
			Receiver->NotifyRoutedGripBegin(GripController, GripInformation);
		}
	}

	for (const TWeakObjectPtr<UVRExpDetectableComponent>& Receiver : Receivers.DetectableComponents)
	{
		if (Receiver.IsValid() && !StoredReceivers.DetectableComponents.Contains(Receiver))
		{
			StoredReceivers.DetectableComponents.Add(Receiver);
			Receiver->NotifyGripBegin(GripController, GripInformation);
		}
	}
}

void UVRExpGripEventRouterSubsystem::DispatchGripEnd(
	UGripMotionControllerComponent* GripController,
	const FBPActorGripInformation& GripInformation,
	bool bWasSocketed)
{
	if (!GripController || GripInformation.GripID == INVALID_VRGRIP_ID)
	{
		return;
	}

	const FActiveGripKey GripKey{GripController, GripInformation.GripID};
	FGripReceivers Receivers;
	if (const FGripReceivers* StoredReceivers = ActiveGripReceivers.Find(GripKey))
	{
		Receivers = *StoredReceivers;
		Receivers.GripInformation = GripInformation;
	}
	else
	{
		GatherGripReceivers(GripController, GripInformation, Receivers);
		Receivers.GripInformation = GripInformation;
	}

	DispatchStoredGripEnd(GripKey, Receivers, bWasSocketed);
}

void UVRExpGripEventRouterSubsystem::DispatchStoredGripEnd(
	const FActiveGripKey& GripKey,
	const FGripReceivers& Receivers,
	bool bWasSocketed)
{
	UGripMotionControllerComponent* GripController =
		GripKey.Controller.Get();
	const FBPActorGripInformation& GripInformation =
		Receivers.GripInformation;

	for (const TWeakObjectPtr<UVRExpGrabbableMotionComponent>& Receiver :
		 Receivers.MotionComponents)
	{
		if (Receiver.IsValid())
		{
			Receiver->NotifyRoutedGripEnd(GripController, GripInformation, bWasSocketed);
		}
	}

	for (const TWeakObjectPtr<UVRExpDetectableComponent>& Receiver :
		 Receivers.DetectableComponents)
	{
		if (Receiver.IsValid())
		{
			Receiver->NotifyGripEnd(GripController, GripInformation, bWasSocketed);
		}
	}

	ActiveGripReceivers.Remove(GripKey);
}

void UVRExpGripEventRouterSubsystem::GatherGripReceivers(
	UGripMotionControllerComponent* GripController,
	const FBPActorGripInformation& GripInformation,
	FGripReceivers& OutReceivers) const
{
	AddReceiversForGripTarget(GripController, GripInformation.GrippedObject, OutReceivers);
	AddReceiversForGripTarget(GripController, GripInformation.GetGrippedActor(), OutReceivers);
	AddReceiversForGripTarget(GripController, GripInformation.GetGrippedComponent(), OutReceivers);
}

void UVRExpGripEventRouterSubsystem::AddReceiversForGripTarget(
	UGripMotionControllerComponent* GripController,
	UObject* GripTarget,
	FGripReceivers& OutReceivers) const
{
	if (!GripTarget)
	{
		return;
	}

	if (const TArray<TWeakObjectPtr<UVRExpGrabbableMotionComponent>>* MotionReceivers =
			MotionComponentsByGripTarget.Find(TWeakObjectPtr<UObject>(GripTarget)))
	{
		for (const TWeakObjectPtr<UVRExpGrabbableMotionComponent>& Receiver : *MotionReceivers)
		{
			if (Receiver.IsValid() && Receiver->AcceptsGripController(GripController))
			{
				OutReceivers.MotionComponents.Add(Receiver);
			}
		}
	}

	if (const TArray<TWeakObjectPtr<UVRExpDetectableComponent>>* DetectableReceivers =
			DetectablesByGripTarget.Find(TWeakObjectPtr<UObject>(GripTarget)))
	{
		for (const TWeakObjectPtr<UVRExpDetectableComponent>& Receiver : *DetectableReceivers)
		{
			if (Receiver.IsValid())
			{
				OutReceivers.DetectableComponents.Add(Receiver);
			}
		}
	}
}
