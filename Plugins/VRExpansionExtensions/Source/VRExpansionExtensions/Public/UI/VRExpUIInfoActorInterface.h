#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UI/VRExpUIInfoTypes.h"
#include "VRExpUIInfoActorInterface.generated.h"

class UVRExpDetectableComponent;

UINTERFACE(Blueprintable)
class VREXPANSIONEXTENSIONS_API UVRExpUIInfoActorInterface : public UInterface
{
    GENERATED_BODY()
};

class VREXPANSIONEXTENSIONS_API IVRExpUIInfoActorInterface
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "VRExpansionExtensions|UI Info")
    void OnUIInfoInitialized(UVRExpDetectableComponent *DetectableComponent,
                             const FVRExpDetectableInteractionContext &Context);

    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "VRExpansionExtensions|UI Info")
    void OnUIInfoPresentationChanged(EVRExpUIInfoInteractionSource InteractionSource,
                                     const FVRExpUIInfoPresentationSettings &PresentationSettings,
                                     const FVRExpDetectableInteractionContext &Context);

    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "VRExpansionExtensions|UI Info")
    void OnUIInfoVisibilityChanged(bool bVisible, EVRExpUIInfoVisibilityReason VisibilityReason,
                                   const FVRExpDetectableInteractionContext &Context);

    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "VRExpansionExtensions|UI Info")
    void OnUIInfoExitTransitionUpdated(float ExitAlpha,
                                       EVRExpUIInfoVisibilityReason VisibilityReason,
                                       const FVRExpDetectableInteractionContext &Context);

    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "VRExpansionExtensions|UI Info")
    void OnUIInfoAboutToDestroy(const FVRExpDetectableInteractionContext &Context);
};
