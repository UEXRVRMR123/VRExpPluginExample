#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VRBPDatatypes.h"
#include "VRExpGripEventRouterControllerListener.generated.h"

class UGripMotionControllerComponent;
class AActor;
class UVRExpGripEventRouterSubsystem;

UCLASS(Transient)
class UVRExpGripEventRouterControllerListener
    : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UVRExpGripEventRouterSubsystem *InRouter, UGripMotionControllerComponent *InGripController);
    void Deinitialize();

    UGripMotionControllerComponent *GetGripController() const;
    AActor *GetGripControllerOwner() const;

private:
    UFUNCTION()
    void HandleControllerGrip(const FBPActorGripInformation &GripInformation);

    UFUNCTION()
    void HandleControllerDrop(const FBPActorGripInformation &GripInformation, bool bWasSocketed);

    TWeakObjectPtr<UVRExpGripEventRouterSubsystem> Router;
    TWeakObjectPtr<UGripMotionControllerComponent> GripController;
    TWeakObjectPtr<AActor> GripControllerOwner;
};
