#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VRExpUIInfoPresentationSubsystem.generated.h"

class AActor;
class APlayerController;
class UVRExpDetectableUIInfoTriggerLogic;

UCLASS()
class VREXPANSIONEXTENSIONS_API UVRExpUIInfoPresentationSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase &Collection) override;
    virtual void Deinitialize() override;
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override;
    virtual UWorld *GetTickableGameObjectWorld() const override;
    virtual ETickableTickType GetTickableTickType() const override;
    virtual TStatId GetStatId() const override;

    UFUNCTION(BlueprintPure, Category = "VRExpansionExtensions|UI Info")
    AActor *GetDisplayedExclusiveUIActor(APlayerController *LocalPlayerController) const;

    uint64 AllocateActivationSequence();
    void RegisterTickLogic(UVRExpDetectableUIInfoTriggerLogic *TriggerLogic);
    void UnregisterTickLogic(UVRExpDetectableUIInfoTriggerLogic *TriggerLogic);

    void UpdateExclusiveClaim(UVRExpDetectableUIInfoTriggerLogic *TriggerLogic,
                              APlayerController *LocalPlayerController, int32 Priority,
                              uint64 ActivationSequence, bool bEligible);
    void RemoveExclusiveClaim(UVRExpDetectableUIInfoTriggerLogic *TriggerLogic);

private:
    struct FExclusivePresentationClaim
    {
        TWeakObjectPtr<UVRExpDetectableUIInfoTriggerLogic> TriggerLogic;
        TWeakObjectPtr<APlayerController> LocalPlayerController;
        int32 Priority = 0;
        uint64 ActivationSequence = 0;
        bool bEligible = true;
    };

    void PruneInvalidEntries();
    void RefreshExclusivePresentation(APlayerController *LocalPlayerController);
    void RefreshAllExclusivePresentations();

    TSet<TWeakObjectPtr<UVRExpDetectableUIInfoTriggerLogic>> TickLogics;
    TArray<FExclusivePresentationClaim> ExclusiveClaims;
    TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<UVRExpDetectableUIInfoTriggerLogic>> ExclusiveWinners;
    uint64 NextActivationSequence = 0;
};
