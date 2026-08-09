#include "Interaction/VRExpGripEventRouterControllerListener.h"

#include "GameFramework/Actor.h"
#include "GripMotionControllerComponent.h"
#include "Interaction/VRExpGripEventRouterSubsystem.h"

void UVRExpGripEventRouterControllerListener::Initialize(
    UVRExpGripEventRouterSubsystem *InRouter,
    UGripMotionControllerComponent *InGripController)
{
    Deinitialize();

    if (!IsValid(InRouter) || !IsValid(InGripController))
    {
        return;
    }

    Router = InRouter;
    GripController = InGripController;
    GripControllerOwner = InGripController->GetOwner();
    InGripController->OnGrippedObject.AddDynamic(
        this,
        &UVRExpGripEventRouterControllerListener::HandleControllerGrip);
    InGripController->OnDroppedObject.AddDynamic(
        this,
        &UVRExpGripEventRouterControllerListener::HandleControllerDrop);
}

void UVRExpGripEventRouterControllerListener::Deinitialize()
{
    if (GripController.IsValid())
    {
        GripController->OnGrippedObject.RemoveDynamic(
            this,
            &UVRExpGripEventRouterControllerListener::HandleControllerGrip);
        GripController->OnDroppedObject.RemoveDynamic(
            this,
            &UVRExpGripEventRouterControllerListener::HandleControllerDrop);
    }

    GripController.Reset();
    GripControllerOwner.Reset();
    Router.Reset();
}

UGripMotionControllerComponent *
UVRExpGripEventRouterControllerListener::GetGripController() const
{
    return GripController.Get();
}

AActor *UVRExpGripEventRouterControllerListener::GetGripControllerOwner() const
{
    return GripControllerOwner.Get();
}

void UVRExpGripEventRouterControllerListener::HandleControllerGrip(
    const FBPActorGripInformation &GripInformation)
{
    if (Router.IsValid() && GripController.IsValid())
    {
        Router->HandleControllerGrip(GripController.Get(), GripInformation);
    }
}

void UVRExpGripEventRouterControllerListener::HandleControllerDrop(
    const FBPActorGripInformation &GripInformation,
    bool bWasSocketed)
{
    if (Router.IsValid() && GripController.IsValid())
    {
        Router->HandleControllerDrop(GripController.Get(), GripInformation, bWasSocketed);
    }
}
