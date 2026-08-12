#include "VRExpUIInfoEditorPreviewManager.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Detection/VRExpDetectableComponent.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interaction/VRExpGrabbableMotionComponent.h"
#include "UI/VRExpDetectableUIInfoTriggerLogic.h"
#include "UI/VRExpUIInfoPlacementResolver.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(
    LogVRExpUIInfoEditorPreview,
    Log,
    All);

namespace
{
constexpr EObjectFlags PreviewObjectFlags =
    RF_Transient |
    RF_TextExportTransient |
    RF_DuplicateTransient |
    RF_NonPIEDuplicateTransient;
constexpr float FullScanIntervalSeconds = 0.5f;

bool IsEditorPreviewWorld(const UWorld *World)
{
    return IsValid(World) &&
           (World->WorldType == EWorldType::Editor ||
            World->WorldType == EWorldType::EditorPreview ||
            World->WorldType == EWorldType::Inactive);
}

const FVRExpUIInfoPresentationSettings &GetPreviewSettings(
    const UVRExpDetectableUIInfoTriggerLogic &Logic)
{
    switch (Logic.EditorPreviewSource)
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

FString GetEditorPreviewManagerSourceLabel(
    EVRExpUIInfoEditorPreviewSource Source)
{
    const UEnum *Enum =
        StaticEnum<EVRExpUIInfoEditorPreviewSource>();
    return Enum
               ? Enum->GetDisplayNameTextByValue(
                         static_cast<int64>(Source))
                     .ToString()
               : TEXT("Unknown");
}

bool SettingsRequireEditorView(
    const FVRExpUIInfoPresentationSettings &Settings)
{
    return Settings.AnchorType ==
               EVRExpUIInfoAnchorType::Camera ||
           Settings.PositionMode ==
               EVRExpUIInfoPositionMode::BetweenAnchorAndCamera ||
           Settings.OrientationMode ==
               EVRExpUIInfoOrientationMode::FaceCamera ||
           Settings.OrientationMode ==
               EVRExpUIInfoOrientationMode::FaceCameraYawOnly;
}

FTransform GetErrorMarkerTransform(
    const UVRExpDetectableComponent *DetectableComponent)
{
    const AActor *Owner =
        IsValid(DetectableComponent)
            ? DetectableComponent->GetOwner()
            : nullptr;
    return IsValid(Owner)
               ? Owner->GetActorTransform()
               : FTransform::Identity;
}

void UpdateLevelEditorSelectability(
    UPrimitiveComponent *PrimitiveComponent,
    const UWorld *World)
{
    if (!IsValid(PrimitiveComponent))
    {
        return;
    }

    const bool bShouldBeSelectable =
        IsValid(World) &&
        World->WorldType == EWorldType::Editor;
    if (PrimitiveComponent->bSelectable !=
        bShouldBeSelectable)
    {
        PrimitiveComponent->bSelectable =
            bShouldBeSelectable;
        PrimitiveComponent->MarkRenderStateDirty();
    }
}
}

FVRExpUIInfoEditorPreviewManager::
    FVRExpUIInfoEditorPreviewManager()
{
    DetectableUnregisterHandle =
        UVRExpDetectableComponent::OnEditorUnregister.
            AddRaw(
                this,
                &FVRExpUIInfoEditorPreviewManager::
                    HandleDetectableUnregistered);
}

FVRExpUIInfoEditorPreviewManager::
    ~FVRExpUIInfoEditorPreviewManager()
{
    UVRExpDetectableComponent::OnEditorUnregister.
        Remove(DetectableUnregisterHandle);
    CleanupAllPreviews();
}

void FVRExpUIInfoEditorPreviewManager::Tick(float DeltaTime)
{
    if (bSuspended ||
        GEditor == nullptr ||
        GEditor->PlayWorld != nullptr)
    {
        return;
    }

    const bool bRunFullScan =
        bRefreshRequested ||
        SecondsUntilFullScan <= 0.0f;
    if (bRunFullScan)
    {
        TSet<TWeakObjectPtr<UVRExpDetectableComponent>>
            SeenComponents;
        for (TObjectIterator<UVRExpDetectableComponent>
                 Iterator;
             Iterator;
             ++Iterator)
        {
            UVRExpDetectableComponent *
                DetectableComponent = *Iterator;
            if (!IsPreviewCandidate(
                    DetectableComponent) ||
                (WorldFilterForTests.IsValid() &&
                 DetectableComponent->GetWorld() !=
                     WorldFilterForTests.Get()))
            {
                continue;
            }

            UVRExpDetectableUIInfoTriggerLogic *Logic =
                FindPreviewLogic(
                    DetectableComponent);
            if (!IsValid(Logic))
            {
                continue;
            }

            const TWeakObjectPtr<
                UVRExpDetectableComponent>
                Key(DetectableComponent);
            SeenComponents.Add(Key);
            FPreviewEntry &Entry =
                PreviewEntries.FindOrAdd(Key);
            RefreshPreview(
                DetectableComponent,
                Logic,
                Entry);
        }

        for (auto Iterator =
                 PreviewEntries.CreateIterator();
             Iterator;
             ++Iterator)
        {
            if (!SeenComponents.Contains(
                    Iterator.Key()))
            {
                CleanupEntry(
                    Iterator.Key().Get(),
                    Iterator.Value(),
                    true);
                Iterator.RemoveCurrent();
            }
        }
        SecondsUntilFullScan =
            FullScanIntervalSeconds;
        bRefreshRequested = false;
        return;
    }

    SecondsUntilFullScan -=
        FMath::Max(DeltaTime, 0.0f);
    for (auto Iterator =
             PreviewEntries.CreateIterator();
         Iterator;
         ++Iterator)
    {
        UVRExpDetectableComponent *
            DetectableComponent =
                Iterator.Key().Get();
        UVRExpDetectableUIInfoTriggerLogic *Logic =
            IsPreviewCandidate(DetectableComponent)
                ? FindPreviewLogic(
                      DetectableComponent)
                : nullptr;
        if (!IsValid(Logic) ||
            (WorldFilterForTests.IsValid() &&
             DetectableComponent->GetWorld() !=
                 WorldFilterForTests.Get()))
        {
            CleanupEntry(
                DetectableComponent,
                Iterator.Value(),
                true);
            Iterator.RemoveCurrent();
            continue;
        }

        RefreshPreview(
            DetectableComponent,
            Logic,
            Iterator.Value());
    }
}

TStatId FVRExpUIInfoEditorPreviewManager::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(
        FVRExpUIInfoEditorPreviewManager,
        STATGROUP_Tickables);
}

bool FVRExpUIInfoEditorPreviewManager::IsTickable() const
{
    return !IsRunningCommandlet();
}

void FVRExpUIInfoEditorPreviewManager::RequestRefresh()
{
    bRefreshRequested = true;
}

void FVRExpUIInfoEditorPreviewManager::SetSuspended(
    bool bInSuspended)
{
    if (bSuspended == bInSuspended)
    {
        return;
    }

    bSuspended = bInSuspended;
    if (bSuspended)
    {
        CleanupAllPreviews();
    }
    else
    {
        RequestRefresh();
    }
}

void FVRExpUIInfoEditorPreviewManager::CleanupAllPreviews()
{
    for (auto &EntryPair : PreviewEntries)
    {
        CleanupEntry(
            EntryPair.Key.Get(),
            EntryPair.Value,
            true);
    }
    PreviewEntries.Reset();

    for (TObjectIterator<UVRExpDetectableComponent> Iterator;
         Iterator;
         ++Iterator)
    {
        if (IsValid(*Iterator))
        {
            Iterator->
                DestroyEditorPreviewChildActorComponent();
        }
    }
}

int32 FVRExpUIInfoEditorPreviewManager::
    GetActivePreviewCountForTests() const
{
    return PreviewEntries.Num();
}

bool FVRExpUIInfoEditorPreviewManager::
    GetPreviewTransformForTests(
        const UVRExpDetectableComponent *DetectableComponent,
        FTransform &OutTransform) const
{
    const FPreviewEntry *Entry =
        PreviewEntries.Find(
            TWeakObjectPtr<UVRExpDetectableComponent>(
                const_cast<UVRExpDetectableComponent *>(
                    DetectableComponent)));
    if (Entry == nullptr)
    {
        return false;
    }

    if (const USceneComponent *MarkerRoot =
            Entry->MarkerRoot.Get())
    {
        OutTransform =
            MarkerRoot->GetComponentTransform();
        return true;
    }
    if (IsValid(DetectableComponent))
    {
        if (const UChildActorComponent *PreviewComponent =
                DetectableComponent->
                    GetEditorPreviewChildActorComponent())
        {
            OutTransform =
                PreviewComponent->GetComponentTransform();
            return true;
        }
    }
    return false;
}

void FVRExpUIInfoEditorPreviewManager::
    SetWorldFilterForTests(UWorld *World)
{
    CleanupAllPreviews();
    WorldFilterForTests = World;
    RequestRefresh();
}

bool FVRExpUIInfoEditorPreviewManager::IsPreviewCandidate(
    const UVRExpDetectableComponent *DetectableComponent) const
{
    if (!IsValid(DetectableComponent) ||
        !DetectableComponent->IsRegistered() ||
        DetectableComponent->IsTemplate() ||
        DetectableComponent->HasAnyFlags(
            RF_ClassDefaultObject |
            RF_ArchetypeObject))
    {
        return false;
    }

    const AActor *Owner =
        DetectableComponent->GetOwner();
    return IsValid(Owner) &&
           !Owner->IsTemplate() &&
           !Owner->HasAnyFlags(
               RF_ClassDefaultObject |
               RF_ArchetypeObject) &&
           IsEditorPreviewWorld(
               DetectableComponent->GetWorld());
}

UVRExpDetectableUIInfoTriggerLogic *
FVRExpUIInfoEditorPreviewManager::FindPreviewLogic(
    const UVRExpDetectableComponent *DetectableComponent) const
{
    if (!IsValid(DetectableComponent))
    {
        return nullptr;
    }

    for (UVRExpDetectableTriggerLogicBase *TriggerLogic :
         DetectableComponent->TriggerLogics)
    {
        UVRExpDetectableUIInfoTriggerLogic *UIInfoLogic =
            Cast<UVRExpDetectableUIInfoTriggerLogic>(
                TriggerLogic);
        if (IsValid(UIInfoLogic) &&
            UIInfoLogic->EditorPreviewMode !=
                EVRExpUIInfoEditorPreviewMode::Disabled)
        {
            return UIInfoLogic;
        }
    }
    return nullptr;
}

void FVRExpUIInfoEditorPreviewManager::RefreshPreview(
    UVRExpDetectableComponent *DetectableComponent,
    UVRExpDetectableUIInfoTriggerLogic *Logic,
    FPreviewEntry &Entry)
{
    if (Entry.Logic.Get() != Logic)
    {
        CleanupEntry(
            DetectableComponent,
            Entry,
            true);
        Entry.Logic = Logic;
    }

    if (Logic->EditorPreviewMode ==
            EVRExpUIInfoEditorPreviewMode::UIActor &&
        Logic->UIActorClass.Get() == nullptr)
    {
        DetectableComponent->
            DestroyEditorPreviewChildActorComponent();
        const FString FailureReason =
            TEXT("UIActorClass is not configured.");
        RefreshMarker(
            DetectableComponent,
            Entry,
            GetErrorMarkerTransform(DetectableComponent),
            FailureReason,
            true);
        ReportStatus(
            Logic,
            EVRExpUIInfoEditorPreviewStatus::
                MissingUIActorClass,
            FailureReason,
            Entry);
        return;
    }

    FVRExpUIInfoPlacementResult PlacementResult;
    FString SourceLabel;
    FString FailureReason;
    EVRExpUIInfoEditorPreviewStatus FailureStatus =
        EVRExpUIInfoEditorPreviewStatus::PlacementFailed;
    if (!ResolvePreview(
            DetectableComponent,
            Logic,
            PlacementResult,
            SourceLabel,
            FailureReason,
            FailureStatus))
    {
        DetectableComponent->
            DestroyEditorPreviewChildActorComponent();
        RefreshMarker(
            DetectableComponent,
            Entry,
            GetErrorMarkerTransform(DetectableComponent),
            FailureReason,
            true);
        ReportStatus(
            Logic,
            FailureStatus,
            FailureReason,
            Entry);
        return;
    }

    if (Logic->EditorPreviewMode ==
        EVRExpUIInfoEditorPreviewMode::UIActor)
    {
        DestroyMarker(Entry);
        if (!RefreshUIActor(
                DetectableComponent,
                Logic,
                PlacementResult.TargetWorldTransform,
                FailureReason))
        {
            RefreshMarker(
                DetectableComponent,
                Entry,
                GetErrorMarkerTransform(
                    DetectableComponent),
                FailureReason,
                true);
            ReportStatus(
                Logic,
                EVRExpUIInfoEditorPreviewStatus::
                    PlacementFailed,
                FailureReason,
                Entry);
            return;
        }
    }
    else
    {
        DetectableComponent->
            DestroyEditorPreviewChildActorComponent();
        RefreshMarker(
            DetectableComponent,
            Entry,
            PlacementResult.TargetWorldTransform,
            FString::Printf(
                TEXT("UI Preview: %s"),
                *SourceLabel),
            false);
    }

    ReportStatus(
        Logic,
        EVRExpUIInfoEditorPreviewStatus::Active,
        FString(),
        Entry);
}

bool FVRExpUIInfoEditorPreviewManager::ResolvePreview(
    const UVRExpDetectableComponent *DetectableComponent,
    const UVRExpDetectableUIInfoTriggerLogic *Logic,
    FVRExpUIInfoPlacementResult &OutResult,
    FString &OutSourceLabel,
    FString &OutFailureReason,
    EVRExpUIInfoEditorPreviewStatus &OutFailureStatus) const
{
    if (!IsValid(DetectableComponent) ||
        !IsValid(Logic))
    {
        OutFailureReason =
            TEXT("Preview component or logic is unavailable.");
        OutFailureStatus =
            EVRExpUIInfoEditorPreviewStatus::
                PlacementFailed;
        return false;
    }

    FVRExpUIInfoPresentationSettings PreviewSettings =
        GetPreviewSettings(*Logic);
    FVRExpUIInfoPlacementResolver::NormalizeSettings(
        PreviewSettings);
    if (PreviewSettings.AnchorType ==
            EVRExpUIInfoAnchorType::Camera &&
        PreviewSettings.BindingMode ==
            EVRExpUIInfoBindingMode::Attach)
    {
        PreviewSettings.BindingMode =
            EVRExpUIInfoBindingMode::Follow;
    }

    AActor *Owner = DetectableComponent->GetOwner();
    USceneComponent *MotionUpdatedComponent = nullptr;
    if (IsValid(Owner))
    {
        UVRExpGrabbableMotionComponent *
            ConfiguredMotionComponent = nullptr;
        if (DetectableComponent->GripSourceMode ==
            EVRExpDetectableGripSourceMode::
                GrabbableMotionComponent)
        {
            ConfiguredMotionComponent =
                Cast<UVRExpGrabbableMotionComponent>(
                    DetectableComponent->
                        GrabbableMotionSource.GetComponent(
                            Owner));
        }

        TInlineComponentArray<
            UVRExpGrabbableMotionComponent *>
            MotionComponents(Owner);
        MotionComponents.RemoveAll(
            [](const UVRExpGrabbableMotionComponent *Component)
            {
                return !IsValid(Component);
            });
        UVRExpGrabbableMotionComponent *
            ResolvedMotionComponent =
                IsValid(ConfiguredMotionComponent)
                    ? ConfiguredMotionComponent
                    : MotionComponents.Num() == 1
                          ? MotionComponents[0]
                          : nullptr;
        if (IsValid(ResolvedMotionComponent))
        {
            MotionUpdatedComponent =
                ResolvedMotionComponent->
                    GetResolvedUpdatedComponent();
            if (!IsValid(MotionUpdatedComponent))
            {
                MotionUpdatedComponent =
                    Owner->GetRootComponent();
            }
        }
        else if (
            MotionComponents.Num() > 1 &&
            !IsValid(ConfiguredMotionComponent) &&
            PreviewSettings.AnchorType ==
                EVRExpUIInfoAnchorType::
                    MotionUpdatedComponent)
        {
            OutFailureReason =
                TEXT("Multiple Grabbable Motion Components make the Motion Updated Component anchor ambiguous.");
            OutFailureStatus =
                EVRExpUIInfoEditorPreviewStatus::
                    AmbiguousMotionSource;
            return false;
        }
    }

    FTransform ViewTransform;
    const bool bHasViewTransform =
        ResolveEditorViewTransform(
            DetectableComponent->GetWorld(),
            ViewTransform);
    if (!bHasViewTransform &&
        SettingsRequireEditorView(PreviewSettings))
    {
        OutFailureReason =
            TEXT("No editor viewport matches the preview object's world.");
        OutFailureStatus =
            EVRExpUIInfoEditorPreviewStatus::
                NoMatchingViewport;
        return false;
    }

    FVRExpUIInfoPlacementRequest Request;
    Request.Settings = &PreviewSettings;
    Request.DetectableOwner = Owner;
    Request.MotionUpdatedComponent =
        MotionUpdatedComponent;
    Request.bHasViewTransform = bHasViewTransform;
    Request.ViewTransform = ViewTransform;
    OutSourceLabel =
        GetEditorPreviewManagerSourceLabel(
            Logic->EditorPreviewSource);
    OutFailureStatus =
        EVRExpUIInfoEditorPreviewStatus::
            PlacementFailed;
    const bool bResolved =
        FVRExpUIInfoPlacementResolver::Resolve(
        Request,
        OutResult,
        OutFailureReason);
    if (!bResolved && OutFailureReason.IsEmpty())
    {
        OutFailureReason =
            TEXT("The shared placement resolver could not produce a preview transform.");
    }
    return bResolved;
}

bool FVRExpUIInfoEditorPreviewManager::
    ResolveEditorViewTransform(
        UWorld *World,
        FTransform &OutViewTransform) const
{
    if (!IsValid(World) || GEditor == nullptr)
    {
        return false;
    }

    FEditorViewportClient *FallbackClient = nullptr;
    for (FEditorViewportClient *ViewportClient :
         GEditor->GetAllViewportClients())
    {
        if (ViewportClient == nullptr ||
            ViewportClient->Viewport == nullptr ||
            ViewportClient->GetWorld() != World)
        {
            continue;
        }

        if (ViewportClient->IsPerspective())
        {
            OutViewTransform = FTransform(
                ViewportClient->GetViewRotation(),
                ViewportClient->GetViewLocation(),
                FVector::OneVector);
            return true;
        }
        if (FallbackClient == nullptr)
        {
            FallbackClient = ViewportClient;
        }
    }

    if (FallbackClient == nullptr)
    {
        return false;
    }
    OutViewTransform = FTransform(
        FallbackClient->GetViewRotation(),
        FallbackClient->GetViewLocation(),
        FVector::OneVector);
    return true;
}

void FVRExpUIInfoEditorPreviewManager::RefreshMarker(
    UVRExpDetectableComponent *DetectableComponent,
    FPreviewEntry &Entry,
    const FTransform &WorldTransform,
    const FString &Label,
    bool bIsError)
{
    AActor *Owner =
        IsValid(DetectableComponent)
            ? DetectableComponent->GetOwner()
            : nullptr;
    UWorld *World =
        IsValid(DetectableComponent)
            ? DetectableComponent->GetWorld()
            : nullptr;
    if (!IsValid(Owner) ||
        !IsEditorPreviewWorld(World))
    {
        DestroyMarker(Entry);
        return;
    }

    USceneComponent *MarkerRoot =
        Entry.MarkerRoot.Get();
    if (!IsValid(MarkerRoot))
    {
        DestroyMarker(Entry);

        MarkerRoot = NewObject<USceneComponent>(
            Owner,
            NAME_None,
            PreviewObjectFlags);
        MarkerRoot->SetIsVisualizationComponent(true);
        MarkerRoot->SetMobility(
            EComponentMobility::Movable);
        MarkerRoot->SetAbsolute(true, true, true);
        MarkerRoot->SetComponentTickEnabled(false);
        MarkerRoot->RegisterComponentWithWorld(World);
        Entry.MarkerRoot = MarkerRoot;

        const auto CreateArrow =
            [Owner, World, MarkerRoot](
                const FColor &Color,
                const FRotator &RelativeRotation)
        {
            UArrowComponent *Arrow =
                NewObject<UArrowComponent>(
                    Owner,
                    NAME_None,
                    PreviewObjectFlags);
            Arrow->SetIsVisualizationComponent(true);
            Arrow->SetMobility(
                EComponentMobility::Movable);
            Arrow->SetComponentTickEnabled(false);
            Arrow->SetCollisionEnabled(
                ECollisionEnabled::NoCollision);
            Arrow->SetArrowFColor(Color);
            Arrow->SetArrowLength(25.0f);
            Arrow->SetArrowSize(1.0f);
            Arrow->SetIsScreenSizeScaled(false);
            Arrow->SetTreatAsASprite(false);
            Arrow->SetupAttachment(MarkerRoot);
            Arrow->SetRelativeRotation(RelativeRotation);
            Arrow->RegisterComponentWithWorld(World);
            return Arrow;
        };

        Entry.XArrow = CreateArrow(
            FColor::Red,
            FRotator::ZeroRotator);
        Entry.YArrow = CreateArrow(
            FColor::Green,
            FRotator(0.0f, 90.0f, 0.0f));
        Entry.ZArrow = CreateArrow(
            FColor::Blue,
            FRotator(90.0f, 0.0f, 0.0f));

        UBoxComponent *Bounds =
            NewObject<UBoxComponent>(
                Owner,
                NAME_None,
                PreviewObjectFlags);
        Bounds->SetIsVisualizationComponent(true);
        Bounds->SetMobility(
            EComponentMobility::Movable);
        Bounds->SetComponentTickEnabled(false);
        Bounds->SetCollisionEnabled(
            ECollisionEnabled::NoCollision);
        Bounds->InitBoxExtent(
            FVector(10.0f, 20.0f, 12.0f));
        Bounds->bDrawOnlyIfSelected = false;
        Bounds->SetLineThickness(1.5f);
        Bounds->SetupAttachment(MarkerRoot);
        Bounds->RegisterComponentWithWorld(World);
        Entry.Bounds = Bounds;

        UTextRenderComponent *Text =
            NewObject<UTextRenderComponent>(
                Owner,
                NAME_None,
                PreviewObjectFlags);
        Text->SetIsVisualizationComponent(true);
        Text->SetMobility(
            EComponentMobility::Movable);
        Text->SetComponentTickEnabled(false);
        Text->SetCollisionEnabled(
            ECollisionEnabled::NoCollision);
        Text->SetHorizontalAlignment(
            EHorizTextAligment::EHTA_Center);
        Text->SetWorldSize(10.0f);
        Text->SetupAttachment(MarkerRoot);
        Text->SetRelativeLocation(
            FVector(0.0f, 0.0f, 30.0f));
        Text->RegisterComponentWithWorld(World);
        Entry.Label = Text;
    }

    MarkerRoot->SetWorldTransform(WorldTransform);
    const FColor MarkerColor =
        bIsError
            ? FColor::Red
            : FColor(38, 204, 255);
    if (UArrowComponent *XArrow =
            Entry.XArrow.Get())
    {
        XArrow->SetArrowFColor(
            bIsError ? MarkerColor : FColor::Red);
    }
    if (UArrowComponent *YArrow =
            Entry.YArrow.Get())
    {
        YArrow->SetArrowFColor(
            bIsError ? MarkerColor : FColor::Green);
    }
    if (UArrowComponent *ZArrow =
            Entry.ZArrow.Get())
    {
        ZArrow->SetArrowFColor(
            bIsError ? MarkerColor : FColor::Blue);
    }
    if (UBoxComponent *Bounds = Entry.Bounds.Get())
    {
        Bounds->ShapeColor = MarkerColor;
        Bounds->MarkRenderStateDirty();
    }
    if (UTextRenderComponent *Text = Entry.Label.Get())
    {
        Text->SetText(
            FText::FromString(
                bIsError
                    ? FString::Printf(
                          TEXT("UI Preview Error: %s"),
                          *Label)
                    : Label));
        Text->SetTextRenderColor(MarkerColor);
    }

    UpdateLevelEditorSelectability(Entry.XArrow.Get(), World);
    UpdateLevelEditorSelectability(Entry.YArrow.Get(), World);
    UpdateLevelEditorSelectability(Entry.ZArrow.Get(), World);
    UpdateLevelEditorSelectability(Entry.Bounds.Get(), World);
    UpdateLevelEditorSelectability(Entry.Label.Get(), World);
}

bool FVRExpUIInfoEditorPreviewManager::RefreshUIActor(
    UVRExpDetectableComponent *DetectableComponent,
    UVRExpDetectableUIInfoTriggerLogic *Logic,
    const FTransform &TargetWorldTransform,
    FString &OutFailureReason)
{
    if (!IsValid(DetectableComponent) ||
        !IsValid(Logic) ||
        Logic->UIActorClass.Get() == nullptr)
    {
        OutFailureReason =
            TEXT("UI Actor preview prerequisites are unavailable.");
        return false;
    }

    UChildActorComponent *PreviewComponent =
        DetectableComponent->
            GetEditorPreviewChildActorComponent();
    if (!IsValid(PreviewComponent) ||
        PreviewComponent->GetChildActorClass() !=
            Logic->UIActorClass)
    {
        DetectableComponent->
            DestroyEditorPreviewChildActorComponent();
        AActor *Owner = DetectableComponent->GetOwner();
        UWorld *World = DetectableComponent->GetWorld();
        if (!IsValid(Owner) ||
            !IsEditorPreviewWorld(World))
        {
            OutFailureReason =
                TEXT("The preview owner or editor world is unavailable.");
            return false;
        }

        PreviewComponent =
            NewObject<UChildActorComponent>(
                Owner,
                NAME_None,
                PreviewObjectFlags);
        PreviewComponent->SetIsVisualizationComponent(true);
        PreviewComponent->SetMobility(
            EComponentMobility::Movable);
        PreviewComponent->SetAbsolute(true, true, true);
        PreviewComponent->SetComponentTickEnabled(false);
        PreviewComponent->SetIsReplicated(false);
        PreviewComponent->SetHiddenInGame(true);
        PreviewComponent->SetVisibility(true, true);
        PreviewComponent->SetWorldTransform(
            TargetWorldTransform);
        PreviewComponent->SetChildActorClass(
            Logic->UIActorClass);
        PreviewComponent->RegisterComponentWithWorld(
            World);
        DetectableComponent->
            SetEditorPreviewChildActorComponent(
                PreviewComponent);
    }

    PreviewComponent->SetWorldTransform(
        TargetWorldTransform);
    AActor *PreviewActor =
        PreviewComponent->GetChildActor();
    if (!IsValid(PreviewActor))
    {
        OutFailureReason =
            TEXT("The editor-only Child Actor could not be created.");
        DetectableComponent->
            DestroyEditorPreviewChildActorComponent();
        return false;
    }

    PreviewComponent->SetIsReplicated(false);
    PreviewActor->SetFlags(
        RF_Transient | RF_TextExportTransient);
    PreviewActor->bIsEditorOnlyActor = true;
    PreviewActor->SetReplicates(false);
    PreviewActor->SetReplicateMovement(false);
    PreviewActor->SetActorEnableCollision(false);
    PreviewActor->SetActorTickEnabled(false);
    PreviewActor->SetActorHiddenInGame(true);
    PreviewActor->SetIsTemporarilyHiddenInEditor(false);

    TInlineComponentArray<UActorComponent *>
        PreviewActorComponents(PreviewActor);
    for (UActorComponent *ActorComponent :
         PreviewActorComponents)
    {
        if (!IsValid(ActorComponent))
        {
            continue;
        }
        ActorComponent->SetComponentTickEnabled(false);
        ActorComponent->SetIsReplicated(false);
        if (UPrimitiveComponent *PrimitiveComponent =
                Cast<UPrimitiveComponent>(ActorComponent))
        {
            PrimitiveComponent->SetCollisionEnabled(
                ECollisionEnabled::NoCollision);
            UpdateLevelEditorSelectability(
                PrimitiveComponent,
                DetectableComponent->GetWorld());
        }
    }
    return true;
}

void FVRExpUIInfoEditorPreviewManager::DestroyMarker(
    FPreviewEntry &Entry)
{
    const auto DestroyComponent =
        [](TWeakObjectPtr<UActorComponent> &Component)
    {
        if (UActorComponent *Resolved = Component.Get())
        {
            Resolved->DestroyComponent();
        }
        Component.Reset();
    };

    TWeakObjectPtr<UActorComponent> Label =
        Entry.Label;
    TWeakObjectPtr<UActorComponent> Bounds =
        Entry.Bounds;
    TWeakObjectPtr<UActorComponent> ZArrow =
        Entry.ZArrow;
    TWeakObjectPtr<UActorComponent> YArrow =
        Entry.YArrow;
    TWeakObjectPtr<UActorComponent> XArrow =
        Entry.XArrow;
    TWeakObjectPtr<UActorComponent> MarkerRoot =
        Entry.MarkerRoot;
    DestroyComponent(Label);
    DestroyComponent(Bounds);
    DestroyComponent(ZArrow);
    DestroyComponent(YArrow);
    DestroyComponent(XArrow);
    DestroyComponent(MarkerRoot);
    Entry.Label.Reset();
    Entry.Bounds.Reset();
    Entry.ZArrow.Reset();
    Entry.YArrow.Reset();
    Entry.XArrow.Reset();
    Entry.MarkerRoot.Reset();
}

void FVRExpUIInfoEditorPreviewManager::CleanupEntry(
    UVRExpDetectableComponent *DetectableComponent,
    FPreviewEntry &Entry,
    bool bResetStatus)
{
    DestroyMarker(Entry);
    if (IsValid(DetectableComponent))
    {
        DetectableComponent->
            DestroyEditorPreviewChildActorComponent();
    }
    if (bResetStatus)
    {
        if (UVRExpDetectableUIInfoTriggerLogic *Logic =
                Entry.Logic.Get())
        {
            Logic->SetEditorPreviewStatus(
                EVRExpUIInfoEditorPreviewStatus::Disabled,
                FString());
        }
    }
    Entry.Logic.Reset();
    Entry.LastReportedFailure.Reset();
}

void FVRExpUIInfoEditorPreviewManager::ReportStatus(
    UVRExpDetectableUIInfoTriggerLogic *Logic,
    EVRExpUIInfoEditorPreviewStatus Status,
    const FString &FailureReason,
    FPreviewEntry &Entry)
{
    if (!IsValid(Logic))
    {
        return;
    }

    Logic->SetEditorPreviewStatus(
        Status,
        FailureReason);

    if (Status ==
            EVRExpUIInfoEditorPreviewStatus::Active ||
        Status ==
            EVRExpUIInfoEditorPreviewStatus::Disabled ||
        Entry.LastReportedFailure == FailureReason)
    {
        if (Status ==
                EVRExpUIInfoEditorPreviewStatus::Active ||
            Status ==
                EVRExpUIInfoEditorPreviewStatus::Disabled)
        {
            Entry.LastReportedFailure.Reset();
        }
        return;
    }

    Entry.LastReportedFailure = FailureReason;
    UE_LOG(
        LogVRExpUIInfoEditorPreview,
        Warning,
        TEXT("%s editor preview failed: %s"),
        *Logic->GetPathName(),
        *FailureReason);
}

void FVRExpUIInfoEditorPreviewManager::
    HandleDetectableUnregistered(
        UVRExpDetectableComponent *DetectableComponent)
{
    const TWeakObjectPtr<UVRExpDetectableComponent> Key(
        DetectableComponent);
    if (FPreviewEntry *Entry =
            PreviewEntries.Find(Key))
    {
        CleanupEntry(
            DetectableComponent,
            *Entry,
            true);
        PreviewEntries.Remove(Key);
    }
}
