#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VRExpansionExtensionsEditorSettings.generated.h"

UENUM()
enum class EVRExpUIInfoEditorPreviewBoundsMode : uint8
{
    AutoFromUIActor UMETA(DisplayName = "Auto From UI Actor"),
    FixedSize UMETA(DisplayName = "Fixed Size")
};

UCLASS(Config = Editor, DefaultConfig, meta = (DisplayName = "VRExpansion Extensions - Editor Preview"))
class UVRExpansionExtensionsEditorSettings final : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual FName GetCategoryName() const override
    {
        return FName(TEXT("Plugins"));
    }

    UPROPERTY(Config, EditAnywhere, Category = "UI Info Preview|Interaction Bounds",
              meta = (DisplayName = "Draw Interaction Bounds",
                      ToolTip = "Draw the cyan/yellow interaction bounds. Disabling this only hides the wire box; the configured hit area remains active."))
    bool bDrawInteractionBounds = true;

    UPROPERTY(Config, EditAnywhere, Category = "UI Info Preview|Interaction Bounds",
              meta = (DisplayName = "Bounds Mode",
                      ToolTip = "Auto uses the preview UI Actor bounds. Fixed Size uses an editor-only size that does not scale or otherwise modify the UI Actor."))
    EVRExpUIInfoEditorPreviewBoundsMode InteractionBoundsMode =
        EVRExpUIInfoEditorPreviewBoundsMode::AutoFromUIActor;

    UPROPERTY(Config, EditAnywhere, Category = "UI Info Preview|Interaction Bounds",
              meta = (DisplayName = "Auto Bounds Scale",
                      ClampMin = "0.01", UIMin = "0.01",
                      EditCondition = "InteractionBoundsMode == EVRExpUIInfoEditorPreviewBoundsMode::AutoFromUIActor"))
    FVector AutoBoundsScale = FVector::OneVector;

    UPROPERTY(Config, EditAnywhere, Category = "UI Info Preview|Interaction Bounds",
              meta = (DisplayName = "Fixed Bounds Size (cm)",
                      ClampMin = "1.0", UIMin = "1.0",
                      EditCondition = "InteractionBoundsMode == EVRExpUIInfoEditorPreviewBoundsMode::FixedSize"))
    FVector FixedBoundsSize = FVector(60.0f, 60.0f, 60.0f);

    UPROPERTY(Config, EditAnywhere, Category = "UI Info Preview|Interaction Bounds",
              meta = (DisplayName = "Bounds Padding (cm)",
                      ClampMin = "0.0", UIMin = "0.0",
                      ToolTip = "Extra editor-only padding added to each side of the interaction bounds."))
    FVector InteractionBoundsPadding = FVector(2.0f);

#if WITH_EDITOR
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent &PropertyChangedEvent) override;
#endif
};
