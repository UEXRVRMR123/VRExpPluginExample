#include "Modules/ModuleManager.h"

#include "Components/ChildActorComponent.h"
#include "CoreGlobals.h"
#include "Detection/VRExpDetectableComponent.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "LevelEditorViewport.h"
#include "Selection.h"
#include "TimerManager.h"
#include "UI/VRExpDetectableUIInfoTriggerLogic.h"
#include "UnrealEdGlobals.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "VRExpUIInfoComponentVisualizer.h"
#include "VRExpUIInfoEditorPreviewManager.h"

DEFINE_LOG_CATEGORY_STATIC(
    LogVRExpansionExtensionsEditor,
    Log,
    All);

namespace
{
FVRExpUIInfoEditorPreviewManager *GPreviewManager =
    nullptr;
}

FVRExpUIInfoEditorPreviewManager *
GetVRExpUIInfoEditorPreviewManager()
{
    return GPreviewManager;
}

class FVRExpansionExtensionsEditorModule final
    : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        if (IsRunningCommandlet())
        {
            return;
        }

        PreviewManager =
            MakeUnique<FVRExpUIInfoEditorPreviewManager>();
        GPreviewManager = PreviewManager.Get();
        RegisterUIInfoComponentVisualizer();
        USelection::SelectionChangedEvent.AddRaw(
            this,
            &FVRExpansionExtensionsEditorModule::
                OnEditorSelectionChanged);
        USelection::SelectObjectEvent.AddRaw(
            this,
            &FVRExpansionExtensionsEditorModule::
                OnEditorSelectionChanged);

        FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(
            this,
            &FVRExpansionExtensionsEditorModule::OnObjectPropertyChanged);
        FEditorDelegates::PreBeginPIE.AddRaw(
            this,
            &FVRExpansionExtensionsEditorModule::OnPreBeginPIE);
        FEditorDelegates::EndPIE.AddRaw(
            this,
            &FVRExpansionExtensionsEditorModule::OnEndPIE);
        FEditorDelegates::MapChange.AddRaw(
            this,
            &FVRExpansionExtensionsEditorModule::OnMapChange);
        FWorldDelegates::OnWorldCleanup.AddRaw(
            this,
            &FVRExpansionExtensionsEditorModule::OnWorldCleanup);
        if (GEditor)
        {
            GEditor->OnBlueprintPreCompile().AddRaw(
                this,
                &FVRExpansionExtensionsEditorModule::OnBlueprintPreCompile);
            GEditor->OnBlueprintCompiled().AddRaw(
                this,
                &FVRExpansionExtensionsEditorModule::OnBlueprintCompiled);
        }
    }

    virtual void ShutdownModule() override
    {
        USelection::SelectionChangedEvent.RemoveAll(this);
        USelection::SelectObjectEvent.RemoveAll(this);
        if (GEditor)
        {
            GEditor->GetTimerManager()->
                ClearTimer(LevelPreviewActivationTimer);
        }

        if (GUnrealEd &&
            bUIInfoComponentVisualizerRegistered)
        {
            GUnrealEd->
                UnregisterComponentVisualizer(
                    UVRExpDetectableComponent::
                        StaticClass()->GetFName());
        }
        bUIInfoComponentVisualizerRegistered = false;
        UIInfoComponentVisualizer.Reset();

        CleanupAllPreviews();
        FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
        FEditorDelegates::PreBeginPIE.RemoveAll(this);
        FEditorDelegates::EndPIE.RemoveAll(this);
        FEditorDelegates::MapChange.RemoveAll(this);
        FWorldDelegates::OnWorldCleanup.RemoveAll(this);
        if (GEditor)
        {
            GEditor->OnBlueprintPreCompile().RemoveAll(this);
            GEditor->OnBlueprintCompiled().RemoveAll(this);
        }
        GPreviewManager = nullptr;
        PreviewManager.Reset();
    }

private:
    void RegisterUIInfoComponentVisualizer()
    {
        if (GUnrealEd == nullptr)
        {
            UE_LOG(
                LogVRExpansionExtensionsEditor,
                Error,
                TEXT("Cannot register the UI Info component visualizer because GUnrealEd is unavailable."));
            return;
        }

        UIInfoComponentVisualizer =
            MakeShared<
                FVRExpUIInfoComponentVisualizer>();
        const FName ComponentClassName =
            UVRExpDetectableComponent::
                StaticClass()->GetFName();
        GUnrealEd->RegisterComponentVisualizer(
            ComponentClassName,
            UIInfoComponentVisualizer);
        UIInfoComponentVisualizer->OnRegister();
        bUIInfoComponentVisualizerRegistered =
            GUnrealEd->FindComponentVisualizer(
                ComponentClassName) ==
            UIInfoComponentVisualizer;

        if (bUIInfoComponentVisualizerRegistered)
        {
            UE_LOG(
                LogVRExpansionExtensionsEditor,
                Display,
                TEXT("UI Info component visualizer registration: Succeeded"));
        }
        else
        {
            UE_LOG(
                LogVRExpansionExtensionsEditor,
                Error,
                TEXT("UI Info component visualizer registration: Failed"));
        }
    }

    void OnObjectPropertyChanged(
        UObject *Object,
        FPropertyChangedEvent &PropertyChangedEvent)
    {
        if (Object &&
            (Object->IsA<UVRExpDetectableUIInfoTriggerLogic>() ||
             Object->IsA<UVRExpDetectableComponent>()))
        {
            RequestRefresh();
            if (GEditor)
            {
                GEditor->RedrawAllViewports();
            }
        }
    }

    void OnEditorSelectionChanged(UObject *SelectionObject)
    {
        if (GEditor == nullptr ||
            LevelPreviewActivationTimer.IsValid() ||
            bNormalizingLevelPreviewSelection)
        {
            return;
        }

        const bool bIsLevelSelectionSet =
            SelectionObject ==
                GEditor->GetSelectedActors() ||
            SelectionObject ==
                GEditor->GetSelectedComponents();
        UWorld *SelectedObjectWorld = nullptr;
        if (AActor *SelectedActor =
                Cast<AActor>(SelectionObject))
        {
            SelectedObjectWorld =
                SelectedActor->GetWorld();
        }
        else if (UActorComponent *SelectedComponent =
                     Cast<UActorComponent>(
                         SelectionObject))
        {
            SelectedObjectWorld =
                SelectedComponent->GetWorld();
        }
        if (!bIsLevelSelectionSet &&
            (!IsValid(SelectedObjectWorld) ||
             SelectedObjectWorld->WorldType !=
                 EWorldType::Editor))
        {
            return;
        }

        LevelPreviewActivationTimer =
            GEditor->GetTimerManager()->
                SetTimerForNextTick(
                    FTimerDelegate::CreateRaw(
                        this,
                        &FVRExpansionExtensionsEditorModule::
                            ActivateSelectedLevelPreview));
    }

    void ActivateSelectedLevelPreview()
    {
        LevelPreviewActivationTimer.Invalidate();
        if (GEditor == nullptr ||
            GUnrealEd == nullptr ||
            GEditor->PlayWorld != nullptr ||
            !UIInfoComponentVisualizer.IsValid())
        {
            return;
        }

        TSet<AActor *> SelectedOwners;
        for (FSelectionIterator Iterator(
                 GEditor->GetSelectedComponentIterator());
             Iterator;
             ++Iterator)
        {
            UActorComponent *SelectedComponent =
                Cast<UActorComponent>(*Iterator);
            AActor *Owner =
                IsValid(SelectedComponent)
                    ? SelectedComponent->GetOwner()
                    : nullptr;
            while (IsValid(Owner) &&
                   Owner->IsChildActor())
            {
                Owner = Owner->GetParentActor();
            }
            if (IsValid(Owner))
            {
                SelectedOwners.Add(Owner);
            }
        }

        for (FSelectionIterator Iterator(
                 GEditor->GetSelectedActorIterator());
             Iterator;
             ++Iterator)
        {
            AActor *Owner = Cast<AActor>(*Iterator);
            while (IsValid(Owner) &&
                   Owner->IsChildActor())
            {
                Owner = Owner->GetParentActor();
            }
            if (IsValid(Owner))
            {
                SelectedOwners.Add(Owner);
            }
        }

        if (SelectedOwners.Num() != 1)
        {
            return;
        }

        AActor *SelectedOwner =
            *SelectedOwners.CreateConstIterator();
        UWorld *SelectedWorld =
            IsValid(SelectedOwner)
                ? SelectedOwner->GetWorld()
                : nullptr;
        if (!IsValid(SelectedWorld) ||
            SelectedWorld->WorldType !=
                EWorldType::Editor)
        {
            return;
        }

        FEditorViewportClient *ViewportClient =
            GCurrentLevelEditingViewportClient;
        if (ViewportClient == nullptr ||
            ViewportClient->GetWorld() != SelectedWorld)
        {
            ViewportClient = nullptr;
            for (FEditorViewportClient *Candidate :
                 GEditor->GetAllViewportClients())
            {
                if (Candidate != nullptr &&
                    Candidate->IsLevelEditorClient() &&
                    Candidate->GetWorld() == SelectedWorld)
                {
                    ViewportClient = Candidate;
                    break;
                }
            }
        }
        if (ViewportClient == nullptr)
        {
            return;
        }

        TInlineComponentArray<UVRExpDetectableComponent *>
            DetectableComponents(SelectedOwner);
        for (UVRExpDetectableComponent *DetectableComponent :
                 DetectableComponents)
        {
            if (!IsValid(DetectableComponent) ||
                !DetectableComponent->IsRegistered())
            {
                continue;
            }

            NormalizeLevelPreviewSelection(
                DetectableComponent,
                SelectedOwner);

            TSharedPtr<FComponentVisualizer> Visualizer =
                UIInfoComponentVisualizer;
            const bool bAlreadyEditing =
                GUnrealEd->ComponentVisManager.
                    GetActiveComponentVis() == Visualizer &&
                UIInfoComponentVisualizer->
                        GetEditedComponent() ==
                    DetectableComponent;
            if (bAlreadyEditing ||
                UIInfoComponentVisualizer->
                    BeginEditingPreview(DetectableComponent))
            {
                if (!bAlreadyEditing)
                {
                    GUnrealEd->ComponentVisManager.
                        SetActiveComponentVis(
                            ViewportClient,
                            Visualizer);
                }
                ViewportClient->Invalidate();
                GEditor->RedrawLevelEditingViewports();
                return;
            }
        }
    }

    void NormalizeLevelPreviewSelection(
        UVRExpDetectableComponent *DetectableComponent,
        AActor *SelectedOwner)
    {
        if (GEditor == nullptr ||
            !IsValid(DetectableComponent) ||
            !IsValid(SelectedOwner))
        {
            return;
        }

        const UChildActorComponent *PreviewComponent =
            DetectableComponent->GetEditorPreviewChildActorComponent();
        const AActor *PreviewActor =
            IsValid(PreviewComponent)
                ? PreviewComponent->GetChildActor()
                : nullptr;
        if (!IsValid(PreviewActor))
        {
            return;
        }

        bool bPreviewObjectSelected = false;
        for (FSelectionIterator Iterator(
                 GEditor->GetSelectedActorIterator());
             Iterator;
             ++Iterator)
        {
            if (Cast<AActor>(*Iterator) == PreviewActor)
            {
                bPreviewObjectSelected = true;
                break;
            }
        }

        if (!bPreviewObjectSelected)
        {
            for (FSelectionIterator Iterator(
                     GEditor->GetSelectedComponentIterator());
                 Iterator;
                 ++Iterator)
            {
                const UActorComponent *SelectedComponent =
                    Cast<UActorComponent>(*Iterator);
                if (IsValid(SelectedComponent) &&
                    SelectedComponent->GetOwner() == PreviewActor)
                {
                    bPreviewObjectSelected = true;
                    break;
                }
            }
        }

        if (!bPreviewObjectSelected)
        {
            return;
        }

        // A ChildActorComponent-owned preview actor is not the persistent edit target. Selecting it
        // leaves the standard transform gizmo bound to a transient child actor, which can make the
        // gizmo visible but non-manipulable. Keep a real level actor selected and let the active
        // component visualizer consume the transform delta instead.
        bNormalizingLevelPreviewSelection = true;
        GEditor->SelectNone(false, true, false);
        GEditor->SelectActor(SelectedOwner, true, true);
        bNormalizingLevelPreviewSelection = false;
    }

    void OnPreBeginPIE(bool bIsSimulating)
    {
        if (PreviewManager)
        {
            PreviewManager->SetSuspended(true);
        }
    }

    void OnEndPIE(bool bIsSimulating)
    {
        if (PreviewManager)
        {
            PreviewManager->SetSuspended(false);
        }
    }

    void OnMapChange(uint32 MapChangeFlags)
    {
        CleanupAllPreviews();
        RequestRefresh();
    }

    void OnBlueprintPreCompile(UBlueprint *Blueprint)
    {
        CleanupAllPreviews();
    }

    void OnBlueprintCompiled()
    {
        RequestRefresh();
    }

    void OnWorldCleanup(
        UWorld *World,
        bool bSessionEnded,
        bool bCleanupResources)
    {
        CleanupAllPreviews();
        RequestRefresh();
    }

    void RequestRefresh()
    {
        if (PreviewManager)
        {
            PreviewManager->RequestRefresh();
        }
    }

    void CleanupAllPreviews()
    {
        if (PreviewManager)
        {
            PreviewManager->CleanupAllPreviews();
        }
    }

    TUniquePtr<FVRExpUIInfoEditorPreviewManager> PreviewManager;
    TSharedPtr<FVRExpUIInfoComponentVisualizer>
        UIInfoComponentVisualizer;
    FTimerHandle LevelPreviewActivationTimer;
    bool bUIInfoComponentVisualizerRegistered = false;
    bool bNormalizingLevelPreviewSelection = false;
};

IMPLEMENT_MODULE(
    FVRExpansionExtensionsEditorModule,
    VRExpansionExtensionsEditor);
