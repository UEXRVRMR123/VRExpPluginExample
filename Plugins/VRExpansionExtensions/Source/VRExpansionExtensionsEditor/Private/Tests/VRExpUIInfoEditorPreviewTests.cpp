#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Detection/VRExpDetectableComponent.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/TextRenderActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Info.h"
#include "Interaction/VRExpGrabbableMotionComponent.h"
#include "PreviewScene.h"
#include "Settings/VRExpansionExtensionsEditorSettings.h"
#include "UI/VRExpDetectableUIInfoTriggerLogic.h"
#include "UnrealEdGlobals.h"
#include "VRExpUIInfoComponentVisualizer.h"
#include "VRExpUIInfoEditorPreviewManager.h"

namespace
{
UVRExpDetectableComponent *AddDetectableWithPreview(
    UWorld *World,
    EVRExpUIInfoEditorPreviewMode PreviewMode,
    const FTransform &PreviewTransform,
    bool bSetUIActorClass,
    UVRExpDetectableUIInfoTriggerLogic *&OutLogic)
{
    AActor *Owner =
        World->SpawnActor<AActor>();
    UVRExpDetectableComponent *DetectableComponent =
        NewObject<UVRExpDetectableComponent>(
            Owner,
            NAME_None,
            RF_Transient);
    Owner->AddInstanceComponent(DetectableComponent);
    DetectableComponent->RegisterComponent();

    OutLogic =
        NewObject<UVRExpDetectableUIInfoTriggerLogic>(
            DetectableComponent,
            NAME_None,
            RF_Transient);
    OutLogic->EditorPreviewMode = PreviewMode;
    OutLogic->EditorPreviewSource =
        EVRExpUIInfoEditorPreviewSource::Head;
    OutLogic->HeadPresentationSettings.AnchorType =
        EVRExpUIInfoAnchorType::World;
    OutLogic->HeadPresentationSettings.WorldTransform =
        PreviewTransform;
    if (bSetUIActorClass)
    {
        OutLogic->UIActorClass =
            AInfo::StaticClass();
    }
    DetectableComponent->TriggerLogics.Add(OutLogic);
    return DetectableComponent;
}

FVRExpUIInfoPresentationSettings &GetPreviewSettingsForTest(
    UVRExpDetectableUIInfoTriggerLogic &Logic,
    EVRExpUIInfoEditorPreviewSource PreviewSource)
{
    switch (PreviewSource)
    {
    case EVRExpUIInfoEditorPreviewSource::Grip:
        return Logic.GripPresentationSettings;
    case EVRExpUIInfoEditorPreviewSource::Release:
        return Logic.ReleasePresentationSettings;
    case EVRExpUIInfoEditorPreviewSource::Motion:
        return Logic.MotionPresentationSettings;
    case EVRExpUIInfoEditorPreviewSource::Head:
    default:
        return Logic.HeadPresentationSettings;
    }
}

UVRExpDetectableComponent *AddEditablePreviewInstance(
    UWorld *World,
    UVRExpDetectableComponent *ComponentArchetype,
    UVRExpDetectableUIInfoTriggerLogic *LogicArchetype,
    UVRExpDetectableUIInfoTriggerLogic *&OutLogic)
{
    AActor *Owner = World->SpawnActor<AActor>();
    UVRExpDetectableComponent *DetectableComponent =
        NewObject<UVRExpDetectableComponent>(
            Owner,
            NAME_None,
            RF_Transactional,
            ComponentArchetype);
    DetectableComponent->TriggerLogics.Reset();
    OutLogic =
        NewObject<UVRExpDetectableUIInfoTriggerLogic>(
            DetectableComponent,
            NAME_None,
            RF_Transactional,
            LogicArchetype);
    DetectableComponent->TriggerLogics.Add(OutLogic);
    Owner->AddInstanceComponent(DetectableComponent);
    DetectableComponent->RegisterComponent();
    return DetectableComponent;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVRExpUIInfoEditorPreviewManagerTest,
    "VRExpansionExtensions.Editor.UIInfo.PreviewManager",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FVRExpUIInfoEditorPreviewManagerTest::RunTest(
    const FString &Parameters)
{
    FVRExpUIInfoEditorPreviewManager *PreviewManager =
        GetVRExpUIInfoEditorPreviewManager();
    if (!TestNotNull(
            TEXT("Editor Preview Manager"),
            PreviewManager))
    {
        return false;
    }

    PreviewManager->CleanupAllPreviews();
    FPreviewScene PreviewScene(
        FPreviewScene::ConstructionValues()
            .SetEditor(true)
            .SetTransactional(false)
            .AllowAudioPlayback(false)
            .SetCreatePhysicsScene(false)
            .ShouldSimulatePhysics(false));
    UWorld *World = PreviewScene.GetWorld();
    PreviewManager->SetWorldFilterForTests(World);

    const FTransform InitialMarkerTransform(
        FRotator(0.0f, 20.0f, 0.0f),
        FVector(100.0f, 200.0f, 300.0f));
    UVRExpDetectableUIInfoTriggerLogic *MarkerLogic =
        nullptr;
    UVRExpDetectableComponent *MarkerDetectable =
        AddDetectableWithPreview(
            World,
            EVRExpUIInfoEditorPreviewMode::
                TransformMarker,
            InitialMarkerTransform,
            false,
            MarkerLogic);

    UVRExpDetectableUIInfoTriggerLogic *IgnoredSecondLogic =
        NewObject<UVRExpDetectableUIInfoTriggerLogic>(
            MarkerDetectable,
            NAME_None,
            RF_Transient);
    IgnoredSecondLogic->EditorPreviewMode =
        EVRExpUIInfoEditorPreviewMode::TransformMarker;
    MarkerDetectable->TriggerLogics.Add(
        IgnoredSecondLogic);

    UVRExpDetectableUIInfoTriggerLogic *UIActorLogic =
        nullptr;
    UVRExpDetectableComponent *UIActorDetectable =
        AddDetectableWithPreview(
            World,
            EVRExpUIInfoEditorPreviewMode::UIActor,
            FTransform(
                FRotator::ZeroRotator,
                FVector(400.0f, 500.0f, 600.0f)),
            true,
            UIActorLogic);
    UIActorLogic->UIActorClass =
        ATextRenderActor::StaticClass();

    UVRExpDetectableUIInfoTriggerLogic *MissingClassLogic =
        nullptr;
    UVRExpDetectableComponent *MissingClassDetectable =
        AddDetectableWithPreview(
            World,
            EVRExpUIInfoEditorPreviewMode::UIActor,
            FTransform::Identity,
            false,
            MissingClassLogic);

    UVRExpDetectableUIInfoTriggerLogic *NoViewportLogic =
        nullptr;
    AddDetectableWithPreview(
        World,
        EVRExpUIInfoEditorPreviewMode::TransformMarker,
        FTransform::Identity,
        false,
        NoViewportLogic);
    NoViewportLogic->HeadPresentationSettings.AnchorType =
        EVRExpUIInfoAnchorType::Camera;

    UVRExpDetectableUIInfoTriggerLogic *AmbiguousMotionLogic =
        nullptr;
    UVRExpDetectableComponent *AmbiguousMotionDetectable =
        AddDetectableWithPreview(
            World,
            EVRExpUIInfoEditorPreviewMode::TransformMarker,
            FTransform::Identity,
            false,
            AmbiguousMotionLogic);
    AmbiguousMotionLogic->EditorPreviewSource =
        EVRExpUIInfoEditorPreviewSource::Motion;
    AmbiguousMotionLogic->
        MotionPresentationSettings.AnchorType =
        EVRExpUIInfoAnchorType::MotionUpdatedComponent;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        UVRExpGrabbableMotionComponent *MotionComponent =
            NewObject<UVRExpGrabbableMotionComponent>(
                AmbiguousMotionDetectable->GetOwner(),
                NAME_None,
                RF_Transient);
        AmbiguousMotionDetectable->GetOwner()->
            AddInstanceComponent(MotionComponent);
        MotionComponent->RegisterComponent();
    }

    AddExpectedError(
        TEXT("editor preview failed: UIActorClass is not configured"),
        EAutomationExpectedErrorFlags::Contains,
        2);
    AddExpectedError(
        TEXT("editor preview failed: No editor viewport matches"),
        EAutomationExpectedErrorFlags::Contains,
        2);
    AddExpectedError(
        TEXT("editor preview failed: Multiple Grabbable Motion Components"),
        EAutomationExpectedErrorFlags::Contains,
        2);
    {
        TGuardValue<TEnumAsByte<EWorldType::Type>, EWorldType::Type>
            WorldTypeGuard(
                World->WorldType,
                EWorldType::Editor);
        PreviewManager->Tick(0.0f);
    }

    TestEqual(
        TEXT("Multiple unselected objects are previewed together"),
        PreviewManager->
            GetActivePreviewCountForTests(),
        5);
    TestEqual(
        TEXT("Transform Marker is active"),
        MarkerLogic->EditorPreviewStatus,
        EVRExpUIInfoEditorPreviewStatus::Active);
    TestEqual(
        TEXT("UI Actor preview is active"),
        UIActorLogic->EditorPreviewStatus,
        EVRExpUIInfoEditorPreviewStatus::Active);
    TestNotNull(
        TEXT("UI Actor preview creates a Child Actor Component"),
        UIActorDetectable->
            GetEditorPreviewChildActorComponent());
    UChildActorComponent *UIActorPreviewComponent =
        UIActorDetectable->
            GetEditorPreviewChildActorComponent();
    AActor *UIActorPreview =
        IsValid(UIActorPreviewComponent)
            ? UIActorPreviewComponent->GetChildActor()
            : nullptr;
    UPrimitiveComponent *UIActorPreviewPrimitive =
        IsValid(UIActorPreview)
            ? Cast<UPrimitiveComponent>(
                  UIActorPreview->GetRootComponent())
            : nullptr;
    if (TestNotNull(
            TEXT("UI Actor preview has a primitive component"),
            UIActorPreviewPrimitive))
    {
        TestTrue(
            TEXT("Temporary UI Actor primitive participates in level hit testing"),
            UIActorPreviewPrimitive->bSelectable);
    }
    TestEqual(
        TEXT("Only the first enabled UI Trigger Logic is used"),
        IgnoredSecondLogic->EditorPreviewStatus,
        EVRExpUIInfoEditorPreviewStatus::Disabled);
    TestEqual(
        TEXT("Missing UIActorClass reports an explicit status"),
        MissingClassLogic->EditorPreviewStatus,
        EVRExpUIInfoEditorPreviewStatus::
            MissingUIActorClass);
    TestTrue(
        TEXT("Missing UIActorClass reports a reason"),
        !MissingClassLogic->
            EditorPreviewFailureReason.IsEmpty());
    TestNull(
        TEXT("Missing UIActorClass does not create a Child Actor"),
        MissingClassDetectable->
            GetEditorPreviewChildActorComponent());
    TestEqual(
        TEXT("Camera preview without a matching viewport reports its status"),
        NoViewportLogic->EditorPreviewStatus,
        EVRExpUIInfoEditorPreviewStatus::
            NoMatchingViewport);
    TestEqual(
        TEXT("Multiple Motion anchors report ambiguity"),
        AmbiguousMotionLogic->EditorPreviewStatus,
        EVRExpUIInfoEditorPreviewStatus::
            AmbiguousMotionSource);

    FTransform ResolvedMarkerTransform;
    TestTrue(
        TEXT("Marker transform can be inspected"),
        PreviewManager->GetPreviewTransformForTests(
            MarkerDetectable,
            ResolvedMarkerTransform));
    TestTrue(
        TEXT("Marker uses the shared world placement result"),
        ResolvedMarkerTransform.Equals(
            InitialMarkerTransform));

    const FTransform UpdatedMarkerTransform(
        FRotator(0.0f, 65.0f, 0.0f),
        FVector(-100.0f, 50.0f, 150.0f));
    MarkerLogic->HeadPresentationSettings.WorldTransform =
        UpdatedMarkerTransform;
    PreviewManager->RequestRefresh();
    PreviewManager->Tick(0.0f);
    TestTrue(
        TEXT("Property changes update the existing preview in place"),
        PreviewManager->GetPreviewTransformForTests(
            MarkerDetectable,
            ResolvedMarkerTransform) &&
            ResolvedMarkerTransform.Equals(
                UpdatedMarkerTransform));

    MarkerDetectable->UnregisterComponent();
    TestEqual(
        TEXT("Component unregister immediately resets preview status"),
        MarkerLogic->EditorPreviewStatus,
        EVRExpUIInfoEditorPreviewStatus::Disabled);
    TestEqual(
        TEXT("Component unregister immediately removes its preview entry"),
        PreviewManager->
            GetActivePreviewCountForTests(),
        4);

    PreviewManager->SetSuspended(true);
    TestEqual(
        TEXT("PIE suspension clears all preview entries"),
        PreviewManager->
            GetActivePreviewCountForTests(),
        0);
    TestNull(
        TEXT("PIE suspension destroys the Child Actor Component"),
        UIActorDetectable->
            GetEditorPreviewChildActorComponent());
    PreviewManager->SetSuspended(false);
    PreviewManager->Tick(0.0f);
    TestEqual(
        TEXT("Ending PIE rebuilds enabled previews"),
        PreviewManager->
            GetActivePreviewCountForTests(),
        4);
    TestNotNull(
        TEXT("Ending PIE rebuilds the UI Actor preview"),
        UIActorDetectable->
            GetEditorPreviewChildActorComponent());

    PreviewManager->CleanupAllPreviews();
    TestEqual(
        TEXT("Cleanup resets preview status"),
        MarkerLogic->EditorPreviewStatus,
        EVRExpUIInfoEditorPreviewStatus::Disabled);
    TestNull(
        TEXT("Cleanup destroys the Child Actor Component"),
        UIActorDetectable->
            GetEditorPreviewChildActorComponent());
    PreviewManager->SetWorldFilterForTests(nullptr);
    PreviewManager->RequestRefresh();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVRExpUIInfoEditorTransformEditingTest,
    "VRExpansionExtensions.Editor.UIInfo.WorldTransformEditing",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FVRExpUIInfoEditorTransformEditingTest::RunTest(
    const FString &Parameters)
{
    const bool bVisualizerRegistered =
        GUnrealEd != nullptr &&
        GUnrealEd->FindComponentVisualizer(
            UVRExpDetectableComponent::StaticClass()).IsValid();
    TestTrue(
        TEXT("UI Info component visualizer is registered"),
        bVisualizerRegistered);

    UVRExpansionExtensionsEditorSettings *EditorSettings =
        GetMutableDefault<UVRExpansionExtensionsEditorSettings>();
    if (TestNotNull(TEXT("UI Info editor preview settings"), EditorSettings))
    {
        TGuardValue<EVRExpUIInfoEditorPreviewBoundsMode> BoundsModeGuard(
            EditorSettings->InteractionBoundsMode,
            EVRExpUIInfoEditorPreviewBoundsMode::FixedSize);
        TGuardValue<FVector> BoundsSizeGuard(
            EditorSettings->FixedBoundsSize,
            FVector(80.0f, 60.0f, 40.0f));
        TGuardValue<FVector> BoundsPaddingGuard(
            EditorSettings->InteractionBoundsPadding,
            FVector(5.0f, 4.0f, 3.0f));
        const FTransform HandleTransform(
            FRotator::ZeroRotator,
            FVector(100.0f, 200.0f, 300.0f));
        const FBox InteractionBounds =
            FVRExpUIInfoComponentVisualizer::GetPreviewInteractionBoundsForTests(
                nullptr,
                HandleTransform,
                EVRExpUIInfoEditorPreviewMode::UIActor);
        TestTrue(
            TEXT("Fixed interaction bounds keep the preview transform center"),
            InteractionBounds.GetCenter().Equals(
                HandleTransform.GetLocation()));
        TestTrue(
            TEXT("Fixed interaction bounds use configured size and padding"),
            InteractionBounds.GetExtent().Equals(
                FVector(45.0f, 34.0f, 23.0f)));
    }

    FPreviewScene PreviewScene(
        FPreviewScene::ConstructionValues()
            .SetEditor(true)
            .SetTransactional(true)
            .AllowAudioPlayback(false)
            .SetCreatePhysicsScene(false)
            .ShouldSimulatePhysics(false));
    UWorld *World = PreviewScene.GetWorld();
    if (!TestNotNull(
            TEXT("Editor preview world"),
            World))
    {
        return false;
    }

    UVRExpDetectableComponent *ComponentArchetype =
        NewObject<UVRExpDetectableComponent>(
            GetTransientPackage(),
            NAME_None,
            RF_ArchetypeObject |
                RF_Transactional);
    UVRExpDetectableUIInfoTriggerLogic *LogicArchetype =
        NewObject<UVRExpDetectableUIInfoTriggerLogic>(
            ComponentArchetype,
            NAME_None,
            RF_ArchetypeObject |
                RF_Transactional);
    LogicArchetype->EditorPreviewMode =
        EVRExpUIInfoEditorPreviewMode::UIActor;
    LogicArchetype->EditorPreviewSource =
        EVRExpUIInfoEditorPreviewSource::Head;
    LogicArchetype->UIActorClass =
        AInfo::StaticClass();

    const EVRExpUIInfoEditorPreviewSource
        PreviewSources[] = {
            EVRExpUIInfoEditorPreviewSource::Head,
            EVRExpUIInfoEditorPreviewSource::Grip,
            EVRExpUIInfoEditorPreviewSource::Release,
            EVRExpUIInfoEditorPreviewSource::Motion};
    for (EVRExpUIInfoEditorPreviewSource PreviewSource :
         PreviewSources)
    {
        FVRExpUIInfoPresentationSettings &Settings =
            GetPreviewSettingsForTest(
                *LogicArchetype,
                PreviewSource);
        Settings.AnchorType =
            EVRExpUIInfoAnchorType::World;
        Settings.PositionMode =
            EVRExpUIInfoPositionMode::AnchorRelative;
        Settings.OrientationMode =
            EVRExpUIInfoOrientationMode::InheritAnchor;
        Settings.WorldTransform =
            FTransform::Identity;
    }
    ComponentArchetype->TriggerLogics.Add(
        LogicArchetype);

    UVRExpDetectableUIInfoTriggerLogic *PreviewLogic =
        nullptr;
    UVRExpDetectableComponent *PreviewComponent =
        AddEditablePreviewInstance(
            World,
            ComponentArchetype,
            LogicArchetype,
            PreviewLogic);
    UVRExpDetectableUIInfoTriggerLogic *DefaultInstanceLogic =
        nullptr;
    AddEditablePreviewInstance(
        World,
        ComponentArchetype,
        LogicArchetype,
        DefaultInstanceLogic);
    UVRExpDetectableUIInfoTriggerLogic *OverriddenInstanceLogic =
        nullptr;
    AddEditablePreviewInstance(
        World,
        ComponentArchetype,
        LogicArchetype,
        OverriddenInstanceLogic);
    OverriddenInstanceLogic->
        HeadPresentationSettings.
        WorldTransform.SetLocation(
            FVector(900.0f, 0.0f, 0.0f));

    FVRExpUIInfoComponentVisualizer Visualizer;
    UVRExpDetectableUIInfoTriggerLogic *LevelInstanceLogic =
        nullptr;
    UVRExpDetectableComponent *LevelInstanceComponent =
        AddDetectableWithPreview(
            World,
            EVRExpUIInfoEditorPreviewMode::UIActor,
            FTransform::Identity,
            true,
            LevelInstanceLogic);
    const FVector LevelInstanceLocationBeforeEdit =
        LevelInstanceLogic->
            HeadPresentationSettings.
            WorldTransform.GetLocation();
    {
        TGuardValue<TEnumAsByte<EWorldType::Type>, EWorldType::Type>
            WorldTypeGuard(
            World->WorldType,
            EWorldType::Editor);
        TestTrue(
            TEXT("Level instance preview can be selected"),
            Visualizer.SelectPreviewForTests(
                LevelInstanceComponent));
        TestTrue(
            TEXT("Level instance edit is handled when the viewport reports no movement"),
            Visualizer.ApplyInputDeltaForTests(
                FVector(15.0f, 0.0f, 0.0f),
                FRotator::ZeroRotator,
                FVector::ZeroVector,
                false));
    }
    TestTrue(
        TEXT("Level instance receives the edited transform"),
        LevelInstanceLogic->
            HeadPresentationSettings.
            WorldTransform.GetLocation().Equals(
                LevelInstanceLocationBeforeEdit +
                FVector(15.0f, 0.0f, 0.0f)));
    if (GEditor)
    {
        TestTrue(
            TEXT("Level instance edit can be undone"),
            GEditor->UndoTransaction());
        TestTrue(
            TEXT("Undo restores the level instance transform"),
            LevelInstanceLogic->
                HeadPresentationSettings.
                WorldTransform.GetLocation().Equals(
                    LevelInstanceLocationBeforeEdit));
        TestTrue(
            TEXT("Level instance edit can be redone"),
            GEditor->RedoTransaction());
        TestTrue(
            TEXT("Redo restores the level instance edit"),
            LevelInstanceLogic->
                HeadPresentationSettings.
                WorldTransform.GetLocation().Equals(
                    LevelInstanceLocationBeforeEdit +
                    FVector(15.0f, 0.0f, 0.0f)));
    }
    LevelInstanceLogic->EditorPreviewMode =
        EVRExpUIInfoEditorPreviewMode::Disabled;

    PreviewLogic->EditorPreviewMode =
        EVRExpUIInfoEditorPreviewMode::TransformMarker;
    TestTrue(
        TEXT("Transform-marker preview can be selected"),
        Visualizer.SelectPreviewForTests(
            PreviewComponent));
    TestTrue(
        TEXT("Transform-marker translation delta is handled"),
        Visualizer.ApplyInputDeltaForTests(
            FVector(1.0f, 0.0f, 0.0f),
            FRotator::ZeroRotator,
            FVector::ZeroVector));
    TestTrue(
        TEXT("Transform-marker edit writes the archetype settings"),
        LogicArchetype->
            HeadPresentationSettings.
            WorldTransform.GetLocation().Equals(
                FVector(1.0f, 0.0f, 0.0f)));
    PreviewLogic->EditorPreviewMode =
        EVRExpUIInfoEditorPreviewMode::UIActor;
    PreviewLogic->
        HeadPresentationSettings.
        WorldTransform.SetLocation(
            FVector::ZeroVector);
    LogicArchetype->
        HeadPresentationSettings.
        WorldTransform.SetLocation(
            FVector::ZeroVector);
    DefaultInstanceLogic->
        HeadPresentationSettings.
        WorldTransform.SetLocation(
            FVector::ZeroVector);

    for (int32 SourceIndex = 0;
         SourceIndex < UE_ARRAY_COUNT(PreviewSources);
         ++SourceIndex)
    {
        const EVRExpUIInfoEditorPreviewSource
            PreviewSource =
                PreviewSources[SourceIndex];
        PreviewLogic->EditorPreviewSource =
            PreviewSource;
        const FVector Delta(
            10.0f * (SourceIndex + 1),
            5.0f,
            0.0f);
        TestTrue(
            TEXT("World preview source can be selected"),
            Visualizer.SelectPreviewForTests(
                PreviewComponent));
        TestTrue(
            TEXT("Translation delta is handled"),
            Visualizer.ApplyInputDeltaForTests(
                Delta,
                FRotator::ZeroRotator,
                FVector::ZeroVector));
        TestTrue(
            TEXT("Selected preview source receives the translation"),
            GetPreviewSettingsForTest(
                *LogicArchetype,
                PreviewSource)
                .WorldTransform.GetLocation()
                .Equals(Delta));
    }

    PreviewLogic->EditorPreviewSource =
        EVRExpUIInfoEditorPreviewSource::Head;
    const FRotator FixedRotationBeforeInheritEdit =
        PreviewLogic->
            HeadPresentationSettings.
            FixedWorldRotation;
    TestTrue(
        TEXT("Inherited preview can be selected"),
        Visualizer.SelectPreviewForTests(
            PreviewComponent));
    TestTrue(
        TEXT("Inherited rotation delta is handled"),
        Visualizer.ApplyInputDeltaForTests(
            FVector::ZeroVector,
            FRotator(0.0f, 15.0f, 0.0f),
            FVector::ZeroVector));
    TestTrue(
        TEXT("Inherited rotation writes WorldTransform rotation"),
        LogicArchetype->
            HeadPresentationSettings.
            WorldTransform.GetRotation().Equals(
                FRotator(0.0f, 15.0f, 0.0f).
                    Quaternion(),
                KINDA_SMALL_NUMBER));
    TestTrue(
        TEXT("Inherited rotation leaves FixedWorldRotation unchanged"),
        LogicArchetype->
            HeadPresentationSettings.
            FixedWorldRotation.Equals(
                FixedRotationBeforeInheritEdit));

    LogicArchetype->
        HeadPresentationSettings.
        OrientationMode =
            EVRExpUIInfoOrientationMode::FixedWorld;
    PreviewLogic->
        HeadPresentationSettings.
        OrientationMode =
            EVRExpUIInfoOrientationMode::FixedWorld;
    LogicArchetype->
        HeadPresentationSettings.
        FixedWorldRotation =
            FRotator::ZeroRotator;
    PreviewLogic->
        HeadPresentationSettings.
        FixedWorldRotation =
            FRotator::ZeroRotator;
    const FQuat WorldRotationBeforeFixedEdit =
        PreviewLogic->
            HeadPresentationSettings.
            WorldTransform.GetRotation();
    TestTrue(
        TEXT("Fixed-world preview can be selected"),
        Visualizer.SelectPreviewForTests(
            PreviewComponent));
    TestTrue(
        TEXT("Fixed-world rotation delta is handled"),
        Visualizer.ApplyInputDeltaForTests(
            FVector::ZeroVector,
            FRotator(0.0f, 30.0f, 0.0f),
            FVector::ZeroVector));
    TestTrue(
        TEXT("Fixed-world rotation writes FixedWorldRotation"),
        LogicArchetype->
            HeadPresentationSettings.
            FixedWorldRotation.Equals(
                FRotator(0.0f, 30.0f, 0.0f),
                0.01f));
    TestTrue(
        TEXT("Fixed-world rotation leaves WorldTransform rotation unchanged"),
        LogicArchetype->
            HeadPresentationSettings.
            WorldTransform.GetRotation().Equals(
                WorldRotationBeforeFixedEdit,
                KINDA_SMALL_NUMBER));

    LogicArchetype->
        HeadPresentationSettings.
        OrientationMode =
            EVRExpUIInfoOrientationMode::FaceCamera;
    PreviewLogic->
        HeadPresentationSettings.
        OrientationMode =
            EVRExpUIInfoOrientationMode::FaceCamera;
    const FRotator FixedRotationBeforeFaceEdit =
        PreviewLogic->
            HeadPresentationSettings.
            FixedWorldRotation;
    const FQuat WorldRotationBeforeFaceEdit =
        PreviewLogic->
            HeadPresentationSettings.
            WorldTransform.GetRotation();
    const FVector LocationBeforeFaceEdit =
        PreviewLogic->
            HeadPresentationSettings.
            WorldTransform.GetLocation();
    const FVector ScaleBeforeFaceEdit =
        PreviewLogic->
            HeadPresentationSettings.
            WorldTransform.GetScale3D();
    TestTrue(
        TEXT("Face-camera preview can be selected"),
        Visualizer.SelectPreviewForTests(
            PreviewComponent));
    TestTrue(
        TEXT("Face-camera translation and scale are handled"),
        Visualizer.ApplyInputDeltaForTests(
            FVector(5.0f, 0.0f, 0.0f),
            FRotator(0.0f, 45.0f, 0.0f),
            FVector(0.25f)));
    TestTrue(
        TEXT("Face-camera translation remains editable"),
        LogicArchetype->
            HeadPresentationSettings.
            WorldTransform.GetLocation().Equals(
                LocationBeforeFaceEdit +
                FVector(5.0f, 0.0f, 0.0f)));
    TestTrue(
        TEXT("Face-camera scale remains editable"),
        LogicArchetype->
            HeadPresentationSettings.
            WorldTransform.GetScale3D().Equals(
                ScaleBeforeFaceEdit +
                FVector(0.25f)));
    TestTrue(
        TEXT("Face-camera rotation does not write WorldTransform rotation"),
        LogicArchetype->
            HeadPresentationSettings.
            WorldTransform.GetRotation().Equals(
                WorldRotationBeforeFaceEdit,
                KINDA_SMALL_NUMBER));
    TestTrue(
        TEXT("Face-camera rotation does not write FixedWorldRotation"),
        LogicArchetype->
            HeadPresentationSettings.
            FixedWorldRotation.Equals(
                FixedRotationBeforeFaceEdit));

    PreviewLogic->
        HeadPresentationSettings.
        AnchorType =
            EVRExpUIInfoAnchorType::Camera;
    TestFalse(
        TEXT("Non-world preview is read-only"),
        Visualizer.SelectPreviewForTests(
            PreviewComponent));
    PreviewLogic->
        HeadPresentationSettings.
        AnchorType =
            EVRExpUIInfoAnchorType::World;
    PreviewLogic->
        HeadPresentationSettings.
        PositionMode =
            EVRExpUIInfoPositionMode::
                BetweenAnchorAndCamera;
    TestFalse(
        TEXT("Between preview is read-only"),
        Visualizer.SelectPreviewForTests(
            PreviewComponent));
    PreviewLogic->
        HeadPresentationSettings.
        PositionMode =
            EVRExpUIInfoPositionMode::AnchorRelative;

    LogicArchetype->
        HeadPresentationSettings.
        OrientationMode =
            EVRExpUIInfoOrientationMode::InheritAnchor;
    PreviewLogic->
        HeadPresentationSettings.
        OrientationMode =
            EVRExpUIInfoOrientationMode::InheritAnchor;
    DefaultInstanceLogic->
        HeadPresentationSettings =
            LogicArchetype->
                HeadPresentationSettings;
    const FVector OverriddenLocationBeforePropagation =
        OverriddenInstanceLogic->
            HeadPresentationSettings.
            WorldTransform.GetLocation();
    const FVector ArchetypeLocationBeforePropagation =
        LogicArchetype->
            HeadPresentationSettings.
            WorldTransform.GetLocation();
    PreviewLogic->
        HeadPresentationSettings.
        WorldTransform.SetLocation(
            ArchetypeLocationBeforePropagation);
    TestTrue(
        TEXT("Archetype preview can be selected"),
        Visualizer.SelectPreviewForTests(
            PreviewComponent));
    TestTrue(
        TEXT("Archetype translation is handled"),
        Visualizer.ApplyInputDeltaForTests(
            FVector(12.0f, 0.0f, 0.0f),
            FRotator::ZeroRotator,
            FVector::ZeroVector));
    TestTrue(
        TEXT("Archetype receives the edited value"),
        LogicArchetype->
            HeadPresentationSettings.
            WorldTransform.GetLocation().Equals(
                ArchetypeLocationBeforePropagation +
                FVector(12.0f, 0.0f, 0.0f)));
    TestTrue(
        TEXT("Default-valued archetype instance is propagated"),
        DefaultInstanceLogic->
            HeadPresentationSettings.
            WorldTransform.GetLocation().Equals(
                LogicArchetype->
                    HeadPresentationSettings.
                    WorldTransform.GetLocation()));
    TestTrue(
        TEXT("Overridden archetype instance is preserved"),
        OverriddenInstanceLogic->
            HeadPresentationSettings.
            WorldTransform.GetLocation().Equals(
                OverriddenLocationBeforePropagation));

    if (GEditor)
    {
        const FVector BeforeUndoEdit =
            LogicArchetype->
                HeadPresentationSettings.
                WorldTransform.GetLocation();
        TestTrue(
            TEXT("Undo test preview can be selected"),
            Visualizer.SelectPreviewForTests(
                PreviewComponent));
        TestTrue(
            TEXT("Undo test delta is handled"),
            Visualizer.ApplyInputDeltaForTests(
                FVector(25.0f, 0.0f, 0.0f),
                FRotator::ZeroRotator,
                FVector::ZeroVector));
        TestTrue(
            TEXT("Editor transaction can be undone"),
            GEditor->UndoTransaction());
        TestTrue(
            TEXT("Undo restores the archetype transform"),
            LogicArchetype->
                HeadPresentationSettings.
                WorldTransform.GetLocation().Equals(
                    BeforeUndoEdit));
        TestTrue(
            TEXT("Editor transaction can be redone"),
            GEditor->RedoTransaction());
        TestTrue(
            TEXT("Redo restores the edited archetype transform"),
            LogicArchetype->
                HeadPresentationSettings.
                WorldTransform.GetLocation().Equals(
                    BeforeUndoEdit +
                    FVector(25.0f, 0.0f, 0.0f)));
    }

    FVRExpUIInfoEditorPreviewManager *PreviewManager =
        GetVRExpUIInfoEditorPreviewManager();
    if (TestNotNull(
            TEXT("Preview manager for refresh test"),
            PreviewManager))
    {
        PreviewManager->CleanupAllPreviews();
        PreviewManager->SetWorldFilterForTests(World);
        PreviewManager->RequestRefresh();
        PreviewManager->Tick(0.0f);

        FTransform RefreshedPreviewTransform;
        const FTransform ExpectedPreviewTransform =
            PreviewLogic->
                HeadPresentationSettings.
                WorldTransform;
        TestTrue(
            TEXT("Preview manager resolves the edited transform"),
            PreviewManager->GetPreviewTransformForTests(
                PreviewComponent,
                RefreshedPreviewTransform));
        TestTrue(
            TEXT("Preview manager refresh does not revert viewport edits"),
            RefreshedPreviewTransform.Equals(
                ExpectedPreviewTransform));

        PreviewManager->CleanupAllPreviews();
        PreviewManager->SetWorldFilterForTests(nullptr);
        PreviewManager->RequestRefresh();
    }

    return true;
}

#endif
