#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Detection/VRExpDetectableTypes.h"
#include "VRExpDetectableTriggerLogicBase.generated.h"

class UVRExpDetectableComponent;

UCLASS(Abstract, NotBlueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class VREXPANSIONEXTENSIONS_API UVRExpDetectableTriggerLogicBase : public UObject
{
    GENERATED_BODY()

public:
    UVRExpDetectableTriggerLogicBase();

    void Initialize(UVRExpDetectableComponent *InDetectableComponent,
                    const FVRExpDetectableInteractionContext &InitialContext);

    void Deinitialize(const FVRExpDetectableInteractionContext &FinalContext);

    void EvaluateContext(const FVRExpDetectableInteractionContext &Context);

    UFUNCTION(BlueprintPure, Category = "VRExpansionExtensions|Detection")
    bool IsActive() const { return bIsActive; }

    UFUNCTION(BlueprintPure, Category = "VRExpansionExtensions|Detection")
    FVRExpDetectableTriggerRuntimeDebugState GetTriggerRuntimeDebugState() const
    {
        return TriggerRuntimeDebugState;
    }

protected:
    virtual void OnInitialized(const FVRExpDetectableInteractionContext &InitialContext);
    virtual void OnActivated(const FVRExpDetectableInteractionContext &Context);
    virtual void OnActiveContextUpdated(const FVRExpDetectableInteractionContext &Context);
    virtual void OnDeactivated(const FVRExpDetectableInteractionContext &Context);
    virtual void OnDeinitialized(const FVRExpDetectableInteractionContext &FinalContext);
    virtual bool EvaluateSourceSwitches(
        const FVRExpDetectableInteractionContext &Context,
        EVRExpDetectableActivationMatchSource &OutMatchedSource,
        FString &OutFailureReason) const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger Logic")
    bool bEnabled;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Trigger Logic")
    bool bIsActive;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient,
              Category = "Trigger Logic|Debug")
    FVRExpDetectableTriggerRuntimeDebugState TriggerRuntimeDebugState;

    TWeakObjectPtr<UVRExpDetectableComponent> DetectableComponent;

private:
    bool bIsInitialized;
};
