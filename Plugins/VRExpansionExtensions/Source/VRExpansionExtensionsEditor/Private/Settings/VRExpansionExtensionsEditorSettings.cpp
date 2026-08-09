#include "Settings/VRExpansionExtensionsEditorSettings.h"

#include "Editor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VRExpansionExtensionsEditorSettings)

#if WITH_EDITOR
void UVRExpansionExtensionsEditorSettings::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (GEditor != nullptr)
    {
        GEditor->RedrawAllViewports();
    }
}
#endif
