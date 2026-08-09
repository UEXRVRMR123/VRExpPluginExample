#include "VRExpUIInfoComponentVisualizer.h"

#include "ActorEditorUtils.h"
#include "CanvasItem.h"
#include "Components/ChildActorComponent.h"
#include "Detection/VRExpDetectableComponent.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Materials/Material.h"
#include "Math/RotationMatrix.h"
#include "ScopedTransaction.h"
#include "SceneManagement.h"
#include "Settings/VRExpansionExtensionsEditorSettings.h"
#include "UI/VRExpDetectableUIInfoTriggerLogic.h"
#include "UnrealWidgetFwd.h"
#include "UObject/UnrealType.h"
#include "VRExpUIInfoEditorPreviewManager.h"

namespace
{
    struct HVRExpUIInfoWorldTransformProxy final : public HComponentVisProxy
    {
        DECLARE_HIT_PROXY();

        explicit HVRExpUIInfoWorldTransformProxy(const UActorComponent *Component)
            : HComponentVisProxy(Component, HPP_Wireframe)
        {
        }
    };

    IMPLEMENT_HIT_PROXY(HVRExpUIInfoWorldTransformProxy, HComponentVisProxy);

    FString GetPreviewSourceLabel(EVRExpUIInfoEditorPreviewSource PreviewSource)
    {
        const UEnum *PreviewSourceEnum = StaticEnum<EVRExpUIInfoEditorPreviewSource>();
        return PreviewSourceEnum
                   ? PreviewSourceEnum->GetDisplayNameTextByValue(static_cast<int64>(PreviewSource)).ToString()
                   : TEXT("Unknown");
    }

    bool IsEditorPreviewOwner(const AActor *Owner)
    {
        return IsValid(Owner) && FActorEditorUtils::IsAPreviewOrInactiveActor(Owner);
    }

    bool AreRotationsEqual(const FQuat &Left, const FQuat &Right)
    {
        return Left.GetNormalized().Equals(Right.GetNormalized(), KINDA_SMALL_NUMBER);
    }

    bool IsEditablePreviewMode(const UVRExpDetectableUIInfoTriggerLogic &Logic)
    {
        return Logic.EditorPreviewMode == EVRExpUIInfoEditorPreviewMode::TransformMarker ||
               (Logic.EditorPreviewMode == EVRExpUIInfoEditorPreviewMode::UIActor &&
                Logic.UIActorClass.Get() != nullptr);
    }

    FBox GetPreviewInteractionBounds(const UVRExpDetectableComponent *DetectableComponent,
                                     const FTransform &HandleTransform, EVRExpUIInfoEditorPreviewMode PreviewMode)
    {
        const UVRExpansionExtensionsEditorSettings *EditorSettings =
            GetDefault<UVRExpansionExtensionsEditorSettings>();
        FBox PreviewBounds(EForceInit::ForceInit);
        const UChildActorComponent *PreviewComponent =
            IsValid(DetectableComponent) && PreviewMode == EVRExpUIInfoEditorPreviewMode::UIActor
                ? DetectableComponent->GetEditorPreviewChildActorComponent()
                : nullptr;
        const AActor *PreviewActor = IsValid(PreviewComponent) ? PreviewComponent->GetChildActor() : nullptr;
        if (IsValid(PreviewActor))
        {
            PreviewBounds = PreviewActor->GetComponentsBoundingBox(true, true);
        }

        FVector BoundsCenter = HandleTransform.GetLocation();
        FVector BoundsExtent = PreviewMode == EVRExpUIInfoEditorPreviewMode::TransformMarker
                                   ? FVector(30.0f)
                                   : FVector(12.0f, 22.0f, 14.0f);
        const bool bHasValidPreviewBounds =
            PreviewBounds.IsValid && !PreviewBounds.Min.ContainsNaN() && !PreviewBounds.Max.ContainsNaN();

        const bool bUseFixedSize =
            EditorSettings != nullptr &&
            EditorSettings->InteractionBoundsMode == EVRExpUIInfoEditorPreviewBoundsMode::FixedSize;
        if (bUseFixedSize)
        {
            BoundsCenter = HandleTransform.GetLocation();
            const FVector FixedSize = EditorSettings->FixedBoundsSize.GetAbs();
            BoundsExtent = FVector(FMath::Max(FixedSize.X, 1.0f), FMath::Max(FixedSize.Y, 1.0f),
                                   FMath::Max(FixedSize.Z, 1.0f)) *
                           0.5f;
        }
        else
        {
            if (bHasValidPreviewBounds)
            {
                BoundsCenter = PreviewBounds.GetCenter();
                const FVector PreviewExtent = PreviewBounds.GetExtent();
                BoundsExtent.X = FMath::Max(PreviewExtent.X, BoundsExtent.X);
                BoundsExtent.Y = FMath::Max(PreviewExtent.Y, BoundsExtent.Y);
                BoundsExtent.Z = FMath::Max(PreviewExtent.Z, BoundsExtent.Z);
            }

            const FVector ConfiguredScale =
                EditorSettings != nullptr ? EditorSettings->AutoBoundsScale.GetAbs() : FVector::OneVector;
            const FVector SafeScale(FMath::Max(ConfiguredScale.X, 0.01f),
                                    FMath::Max(ConfiguredScale.Y, 0.01f),
                                    FMath::Max(ConfiguredScale.Z, 0.01f));
            BoundsExtent *= SafeScale;
        }

        const FVector ConfiguredPadding =
            EditorSettings != nullptr ? EditorSettings->InteractionBoundsPadding : FVector(2.0f);
        const FVector SafePadding(FMath::Max(ConfiguredPadding.X, 0.0f),
                                  FMath::Max(ConfiguredPadding.Y, 0.0f),
                                  FMath::Max(ConfiguredPadding.Z, 0.0f));
        return FBox::BuildAABB(BoundsCenter, BoundsExtent + SafePadding);
    }
} // namespace

#define LOCTEXT_NAMESPACE "VRExpUIInfoComponentVisualizer"

FVRExpUIInfoComponentVisualizer::FVRExpUIInfoComponentVisualizer() = default;

FVRExpUIInfoComponentVisualizer::~FVRExpUIInfoComponentVisualizer() { FinalizeEditing(bAnyValueChanged); }

void FVRExpUIInfoComponentVisualizer::DrawVisualization(const UActorComponent *Component, const FSceneView *View,
                                                        FPrimitiveDrawInterface *PDI)
{
    const UVRExpDetectableComponent *DetectableComponent = Cast<UVRExpDetectableComponent>(Component);
    if (!IsValid(DetectableComponent) || PDI == nullptr)
    {
        return;
    }

    int32 LogicIndex = INDEX_NONE;
    UVRExpDetectableUIInfoTriggerLogic *Logic = nullptr;
    if (!ResolveFirstPreviewLogic(DetectableComponent, LogicIndex, Logic) || !IsValid(Logic))
    {
        return;
    }

    FVRExpUIInfoPresentationSettings *Settings = nullptr;
    FString ReadOnlyReason;
    if (!ResolveEditableSettings(DetectableComponent, LogicIndex, Logic->EditorPreviewSource, Logic, Settings,
                                 &ReadOnlyReason) ||
        Settings == nullptr)
    {
        return;
    }

    const FTransform HandleTransform = GetHandleTransform(DetectableComponent, *Settings);
    const FVector HandleLocation = HandleTransform.GetLocation();
    const FQuat HandleRotation = HandleTransform.GetRotation().GetNormalized();
    const bool bIsEdited = EditedComponentPath.IsValid() && ResolveEditedComponent() == DetectableComponent &&
                           EditedLogicIndex == LogicIndex && EditedPreviewSource == Logic->EditorPreviewSource;
    const FColor HandleColor = bIsEdited ? FColor::Yellow : FColor(38, 204, 255);
    const FBox InteractionBounds =
        GetPreviewInteractionBounds(DetectableComponent, HandleTransform, Logic->EditorPreviewMode);
    const UVRExpansionExtensionsEditorSettings *EditorSettings =
        GetDefault<UVRExpansionExtensionsEditorSettings>();
    const bool bDrawInteractionBounds =
        EditorSettings == nullptr || EditorSettings->bDrawInteractionBounds;

    // The large bounds are only a selection target before editing starts. Keeping their hit proxy
    // alive while the transform widget is active can cover the widget's own hit targets in the
    // level editor, leaving a visible gizmo that cannot be dragged.
    if (PDI->IsHitTesting())
    {
        if (bIsEdited)
        {
            return;
        }

        PDI->SetHitProxy(new HVRExpUIInfoWorldTransformProxy(DetectableComponent));
        if (GEngine != nullptr && GEngine->DebugMeshMaterial != nullptr)
        {
            const FTransform BoundsTransform(FQuat::Identity, InteractionBounds.GetCenter());
            DrawBox(PDI, BoundsTransform.ToMatrixNoScale(), InteractionBounds.GetExtent(),
                    GEngine->DebugMeshMaterial->GetRenderProxy(), SDPG_Foreground);
        }
        PDI->DrawPoint(HandleLocation, HandleColor, 30.0f, SDPG_Foreground);
        DrawWireBox(PDI, InteractionBounds, FLinearColor(HandleColor), SDPG_Foreground, 6.0f, 0.0f, true);
        PDI->SetHitProxy(nullptr);
        return;
    }

    PDI->DrawPoint(HandleLocation, HandleColor, 22.0f, SDPG_Foreground);
    if (bDrawInteractionBounds)
    {
        DrawWireBox(PDI, InteractionBounds, FLinearColor(HandleColor), SDPG_Foreground,
                    bIsEdited ? 2.0f : 1.0f, 0.0f, true);
    }

    constexpr float AxisLength = 22.0f;
    PDI->DrawLine(HandleLocation, HandleLocation + HandleRotation.GetAxisX() * AxisLength, FColor::Red,
                  SDPG_Foreground, 1.5f);
    PDI->DrawLine(HandleLocation, HandleLocation + HandleRotation.GetAxisY() * AxisLength, FColor::Green,
                  SDPG_Foreground, 1.5f);
    PDI->DrawLine(HandleLocation, HandleLocation + HandleRotation.GetAxisZ() * AxisLength, FColor::Blue,
                  SDPG_Foreground, 1.5f);
}

void FVRExpUIInfoComponentVisualizer::DrawVisualizationHUD(const UActorComponent *Component, const FViewport *Viewport,
                                                           const FSceneView *View, FCanvas *Canvas)
{
    const UVRExpDetectableComponent *DetectableComponent = Cast<UVRExpDetectableComponent>(Component);
    if (!IsValid(DetectableComponent) || Canvas == nullptr || GEngine == nullptr)
    {
        return;
    }

    int32 LogicIndex = INDEX_NONE;
    UVRExpDetectableUIInfoTriggerLogic *Logic = nullptr;
    if (!ResolveFirstPreviewLogic(DetectableComponent, LogicIndex, Logic) || !IsValid(Logic))
    {
        return;
    }

    const FVRExpUIInfoPresentationSettings *Settings = GetSettings(Logic, Logic->EditorPreviewSource);
    if (Settings == nullptr)
    {
        return;
    }

    FString Message;
    FLinearColor MessageColor(0.15f, 0.8f, 1.0f);
    if (Logic->EditorPreviewMode == EVRExpUIInfoEditorPreviewMode::UIActor && Logic->UIActorClass.Get() == nullptr)
    {
        Message = TEXT("UI Preview Transform is read-only: UI Actor Class is not configured.");
        MessageColor = FLinearColor(1.0f, 0.2f, 0.1f);
    }
    else if (Settings->AnchorType != EVRExpUIInfoAnchorType::World)
    {
        Message = TEXT("UI Preview Transform is read-only: Anchor Type must be World.");
        MessageColor = FLinearColor(1.0f, 0.45f, 0.1f);
    }
    else if (Settings->PositionMode != EVRExpUIInfoPositionMode::AnchorRelative)
    {
        Message = TEXT("UI Preview Transform is read-only: Position Mode must be Anchor Relative.");
        MessageColor = FLinearColor(1.0f, 0.45f, 0.1f);
    }
    else if (Settings->OrientationMode == EVRExpUIInfoOrientationMode::FaceCamera ||
             Settings->OrientationMode == EVRExpUIInfoOrientationMode::FaceCameraYawOnly)
    {
        Message = TEXT("UI Preview rotation is driven by Face Camera; translation and scale remain editable.");
        MessageColor = FLinearColor(1.0f, 0.8f, 0.1f);
    }
    else
    {
        Message = FString::Printf(TEXT("Click the cyan UI Preview or its outline to edit %s World Transform."),
                                  *GetPreviewSourceLabel(Logic->EditorPreviewSource));
    }

    FCanvasTextItem TextItem(FVector2D(20.0f, 45.0f), FText::FromString(Message), GEngine->GetSmallFont(),
                             MessageColor);
    TextItem.EnableShadow(FLinearColor::Black);
    Canvas->DrawItem(TextItem);
}

bool FVRExpUIInfoComponentVisualizer::VisProxyHandleClick(FEditorViewportClient *ViewportClient,
                                                          HComponentVisProxy *VisProxy, const FViewportClick &Click)
{
    if (VisProxy == nullptr || !VisProxy->IsA(HVRExpUIInfoWorldTransformProxy::StaticGetType()) ||
        !VisProxy->Component.IsValid())
    {
        return false;
    }

    UVRExpDetectableComponent *DetectableComponent =
        Cast<UVRExpDetectableComponent>(const_cast<UActorComponent *>(VisProxy->Component.Get()));
    const bool bSelected = SelectPreview(DetectableComponent);
    if (bSelected && ViewportClient != nullptr)
    {
        ViewportClient->Invalidate();
    }
    return bSelected;
}

bool FVRExpUIInfoComponentVisualizer::GetWidgetLocation(const FEditorViewportClient *ViewportClient,
                                                        FVector &OutLocation) const
{
    UVRExpDetectableComponent *DetectableComponent = ResolveEditedComponent();
    UVRExpDetectableUIInfoTriggerLogic *Logic = ResolveEditedLogic();
    if (!IsValid(DetectableComponent) || !IsValid(Logic))
    {
        return false;
    }

    FVRExpUIInfoPresentationSettings *Settings = nullptr;
    if (!ResolveEditableSettings(DetectableComponent, EditedLogicIndex, EditedPreviewSource, Logic, Settings) ||
        Settings == nullptr)
    {
        return false;
    }

    OutLocation = Settings->WorldTransform.GetLocation();
    return true;
}

bool FVRExpUIInfoComponentVisualizer::GetCustomInputCoordinateSystem(const FEditorViewportClient *ViewportClient,
                                                                     FMatrix &OutMatrix) const
{
    if (ViewportClient == nullptr || (ViewportClient->GetWidgetCoordSystemSpace() != COORD_Local &&
                                      ViewportClient->GetWidgetMode() != UE::Widget::WM_Rotate))
    {
        return false;
    }

    UVRExpDetectableComponent *DetectableComponent = ResolveEditedComponent();
    UVRExpDetectableUIInfoTriggerLogic *Logic = ResolveEditedLogic();
    if (!IsValid(DetectableComponent) || !IsValid(Logic))
    {
        return false;
    }

    FVRExpUIInfoPresentationSettings *Settings = nullptr;
    if (!ResolveEditableSettings(DetectableComponent, EditedLogicIndex, EditedPreviewSource, Logic, Settings) ||
        Settings == nullptr)
    {
        return false;
    }

    OutMatrix = FRotationMatrix::Make(GetHandleTransform(DetectableComponent, *Settings).Rotator());
    return true;
}

bool FVRExpUIInfoComponentVisualizer::HandleInputDelta(FEditorViewportClient *ViewportClient, FViewport *Viewport,
                                                       FVector &DeltaTranslate, FRotator &DeltaRotate,
                                                       FVector &DeltaScale)
{
    UVRExpDetectableComponent *DetectableComponent = ResolveEditedComponent();
    if (!IsValid(DetectableComponent))
    {
        return false;
    }

    if (!ActiveTransaction.IsValid())
    {
        TrackingStarted(ViewportClient);
    }

    return ApplyDeltaAndPropagate(DetectableComponent, EditedLogicIndex, EditedPreviewSource, DeltaTranslate,
                                  DeltaRotate, DeltaScale);
}

void FVRExpUIInfoComponentVisualizer::TrackingStarted(FEditorViewportClient *ViewportClient)
{
    if (ActiveTransaction.IsValid() || !IsValid(ResolveEditedLogic()))
    {
        return;
    }

    ModifiedLogics.Reset();
    bAnyValueChanged = false;
    ActiveTransaction =
        MakeUnique<FScopedTransaction>(LOCTEXT("EditUIInfoWorldTransform", "Edit UI Info World Transform"));
}

void FVRExpUIInfoComponentVisualizer::TrackingStopped(FEditorViewportClient * /*ViewportClient*/, bool /*bDidMove*/)
{
    FinalizeEditing(bAnyValueChanged);
}

void FVRExpUIInfoComponentVisualizer::EndEditing()
{
    FinalizeEditing(bAnyValueChanged);
    EditedComponentPath.Reset();
    EditedLogicIndex = INDEX_NONE;
    EditedPreviewSource = EVRExpUIInfoEditorPreviewSource::Head;
}

UActorComponent *FVRExpUIInfoComponentVisualizer::GetEditedComponent() const { return ResolveEditedComponent(); }

bool FVRExpUIInfoComponentVisualizer::IsVisualizingArchetype() const
{
    const UVRExpDetectableComponent *DetectableComponent = ResolveEditedComponent();
    return IsValid(DetectableComponent) && IsEditorPreviewOwner(DetectableComponent->GetOwner());
}

bool FVRExpUIInfoComponentVisualizer::BeginEditingPreview(UVRExpDetectableComponent *DetectableComponent)
{
    return SelectPreview(DetectableComponent);
}

#if WITH_DEV_AUTOMATION_TESTS
bool FVRExpUIInfoComponentVisualizer::SelectPreviewForTests(UVRExpDetectableComponent *DetectableComponent)
{
    return SelectPreview(DetectableComponent);
}

bool FVRExpUIInfoComponentVisualizer::ApplyInputDeltaForTests(const FVector &DeltaTranslate,
                                                              const FRotator &DeltaRotate, const FVector &DeltaScale,
                                                              bool bReportedDidMove)
{
    FVector MutableTranslate = DeltaTranslate;
    FRotator MutableRotate = DeltaRotate;
    FVector MutableScale = DeltaScale;
    TrackingStarted(nullptr);
    const bool bHandled = HandleInputDelta(nullptr, nullptr, MutableTranslate, MutableRotate, MutableScale);
    TrackingStopped(nullptr, bReportedDidMove);
    return bHandled;
}

FBox FVRExpUIInfoComponentVisualizer::GetPreviewInteractionBoundsForTests(
    const UVRExpDetectableComponent *DetectableComponent, const FTransform &HandleTransform,
    EVRExpUIInfoEditorPreviewMode PreviewMode)
{
    return GetPreviewInteractionBounds(DetectableComponent, HandleTransform, PreviewMode);
}
#endif

bool FVRExpUIInfoComponentVisualizer::SelectPreview(UVRExpDetectableComponent *DetectableComponent)
{
    int32 LogicIndex = INDEX_NONE;
    UVRExpDetectableUIInfoTriggerLogic *Logic = nullptr;
    if (!ResolveFirstPreviewLogic(DetectableComponent, LogicIndex, Logic) || !IsValid(Logic))
    {
        return false;
    }

    FVRExpUIInfoPresentationSettings *Settings = nullptr;
    if (!ResolveEditableSettings(DetectableComponent, LogicIndex, Logic->EditorPreviewSource, Logic, Settings) ||
        Settings == nullptr)
    {
        return false;
    }

    FinalizeEditing(bAnyValueChanged);
    EditedComponentPath = FComponentPropertyPath(DetectableComponent);
    if (!EditedComponentPath.IsValid())
    {
        EditedComponentPath.Reset();
        return false;
    }

    EditedLogicIndex = LogicIndex;
    EditedPreviewSource = Logic->EditorPreviewSource;
    return true;
}

bool FVRExpUIInfoComponentVisualizer::ResolveFirstPreviewLogic(const UVRExpDetectableComponent *DetectableComponent,
                                                               int32 &OutLogicIndex,
                                                               UVRExpDetectableUIInfoTriggerLogic *&OutLogic) const
{
    OutLogicIndex = INDEX_NONE;
    OutLogic = nullptr;
    if (!IsValid(DetectableComponent))
    {
        return false;
    }

    for (int32 LogicIndex = 0; LogicIndex < DetectableComponent->TriggerLogics.Num(); ++LogicIndex)
    {
        UVRExpDetectableUIInfoTriggerLogic *Logic =
            Cast<UVRExpDetectableUIInfoTriggerLogic>(DetectableComponent->TriggerLogics[LogicIndex]);
        if (IsValid(Logic) && Logic->EditorPreviewMode != EVRExpUIInfoEditorPreviewMode::Disabled)
        {
            OutLogicIndex = LogicIndex;
            OutLogic = Logic;
            return true;
        }
    }

    return false;
}

UVRExpDetectableComponent *FVRExpUIInfoComponentVisualizer::ResolveEditedComponent() const
{
    return Cast<UVRExpDetectableComponent>(EditedComponentPath.GetComponent());
}

UVRExpDetectableUIInfoTriggerLogic *FVRExpUIInfoComponentVisualizer::ResolveEditedLogic() const
{
    UVRExpDetectableComponent *DetectableComponent = ResolveEditedComponent();
    if (!IsValid(DetectableComponent) || !DetectableComponent->TriggerLogics.IsValidIndex(EditedLogicIndex))
    {
        return nullptr;
    }

    UVRExpDetectableUIInfoTriggerLogic *Logic =
        Cast<UVRExpDetectableUIInfoTriggerLogic>(DetectableComponent->TriggerLogics[EditedLogicIndex]);
    if (!IsValid(Logic) || Logic->EditorPreviewSource != EditedPreviewSource)
    {
        return nullptr;
    }
    return Logic;
}

bool FVRExpUIInfoComponentVisualizer::ResolveEditableSettings(const UVRExpDetectableComponent *DetectableComponent,
                                                              int32 LogicIndex,
                                                              EVRExpUIInfoEditorPreviewSource PreviewSource,
                                                              UVRExpDetectableUIInfoTriggerLogic *&OutLogic,
                                                              FVRExpUIInfoPresentationSettings *&OutSettings,
                                                              FString *OutReadOnlyReason) const
{
    OutLogic = nullptr;
    OutSettings = nullptr;
    if (OutReadOnlyReason)
    {
        OutReadOnlyReason->Reset();
    }

    if (!IsValid(DetectableComponent) || !DetectableComponent->TriggerLogics.IsValidIndex(LogicIndex))
    {
        return false;
    }

    OutLogic = Cast<UVRExpDetectableUIInfoTriggerLogic>(DetectableComponent->TriggerLogics[LogicIndex]);
    if (!IsValid(OutLogic) || !IsEditablePreviewMode(*OutLogic) || OutLogic->EditorPreviewSource != PreviewSource)
    {
        return false;
    }

    OutSettings = GetMutableSettings(OutLogic, PreviewSource);
    if (OutSettings == nullptr)
    {
        return false;
    }
    if (OutSettings->AnchorType != EVRExpUIInfoAnchorType::World)
    {
        if (OutReadOnlyReason)
        {
            *OutReadOnlyReason = TEXT("Anchor Type must be World.");
        }
        return false;
    }
    if (OutSettings->PositionMode != EVRExpUIInfoPositionMode::AnchorRelative)
    {
        if (OutReadOnlyReason)
        {
            *OutReadOnlyReason = TEXT("Position Mode must be Anchor Relative.");
        }
        return false;
    }
    return true;
}

bool FVRExpUIInfoComponentVisualizer::ApplyDeltaAndPropagate(UVRExpDetectableComponent *DetectableComponent,
                                                             int32 LogicIndex,
                                                             EVRExpUIInfoEditorPreviewSource PreviewSource,
                                                             const FVector &DeltaTranslate, const FRotator &DeltaRotate,
                                                             const FVector &DeltaScale)
{
    UVRExpDetectableUIInfoTriggerLogic *Logic = nullptr;
    FVRExpUIInfoPresentationSettings *Settings = nullptr;
    if (!ResolveEditableSettings(DetectableComponent, LogicIndex, PreviewSource, Logic, Settings) || !IsValid(Logic) ||
        Settings == nullptr)
    {
        return false;
    }

    UVRExpDetectableComponent *ArchetypeComponent = nullptr;
    UVRExpDetectableUIInfoTriggerLogic *ArchetypeLogic = nullptr;
    const bool bPropagateFromPreview = IsEditorPreviewOwner(DetectableComponent->GetOwner());
    if (bPropagateFromPreview)
    {
        ArchetypeComponent = Cast<UVRExpDetectableComponent>(DetectableComponent->GetArchetype());
        if (!IsValid(ArchetypeComponent) || ArchetypeComponent->HasAnyFlags(RF_ClassDefaultObject) ||
            !ArchetypeComponent->TriggerLogics.IsValidIndex(LogicIndex))
        {
            return false;
        }

        ArchetypeLogic = Cast<UVRExpDetectableUIInfoTriggerLogic>(ArchetypeComponent->TriggerLogics[LogicIndex]);
        if (!IsValid(ArchetypeLogic) || ArchetypeLogic->HasAnyFlags(RF_ClassDefaultObject))
        {
            return false;
        }
    }

    struct FPropagationTarget
    {
        TWeakObjectPtr<UVRExpDetectableUIInfoTriggerLogic> Logic;
        bool bUpdateLocation = false;
        bool bUpdateScale = false;
        bool bUpdateWorldRotation = false;
        bool bUpdateFixedRotation = false;
    };

    TArray<FPropagationTarget> PropagationTargets;
    FVRExpUIInfoPresentationSettings OldArchetypeSettings;
    if (bPropagateFromPreview)
    {
        const FVRExpUIInfoPresentationSettings *ArchetypeSettings = GetSettings(ArchetypeLogic, PreviewSource);
        if (ArchetypeSettings == nullptr)
        {
            return false;
        }
        OldArchetypeSettings = *ArchetypeSettings;

        TArray<UObject *> ArchetypeInstances;
        ArchetypeComponent->GetArchetypeInstances(ArchetypeInstances);
        for (UObject *ArchetypeInstance : ArchetypeInstances)
        {
            UVRExpDetectableComponent *InstanceComponent = Cast<UVRExpDetectableComponent>(ArchetypeInstance);
            if (!IsValid(InstanceComponent) || InstanceComponent == DetectableComponent ||
                !InstanceComponent->TriggerLogics.IsValidIndex(LogicIndex))
            {
                continue;
            }

            UVRExpDetectableUIInfoTriggerLogic *InstanceLogic =
                Cast<UVRExpDetectableUIInfoTriggerLogic>(InstanceComponent->TriggerLogics[LogicIndex]);
            const FVRExpUIInfoPresentationSettings *InstanceSettings = GetSettings(InstanceLogic, PreviewSource);
            if (!IsValid(InstanceLogic) || InstanceSettings == nullptr)
            {
                continue;
            }

            FPropagationTarget &Target = PropagationTargets.AddDefaulted_GetRef();
            Target.Logic = InstanceLogic;
            Target.bUpdateLocation = InstanceSettings->WorldTransform.GetLocation().Equals(
                OldArchetypeSettings.WorldTransform.GetLocation());
            Target.bUpdateScale =
                InstanceSettings->WorldTransform.GetScale3D().Equals(OldArchetypeSettings.WorldTransform.GetScale3D());
            Target.bUpdateWorldRotation = AreRotationsEqual(InstanceSettings->WorldTransform.GetRotation(),
                                                            OldArchetypeSettings.WorldTransform.GetRotation());
            Target.bUpdateFixedRotation =
                InstanceSettings->FixedWorldRotation.Equals(OldArchetypeSettings.FixedWorldRotation);
        }
    }

    PrepareForTransaction(Logic);
    const FSettingsDeltaResult DeltaResult = ApplyDeltaToSettings(*Settings, DeltaTranslate, DeltaRotate, DeltaScale);
    if (!DeltaResult.HasAnyChange())
    {
        return true;
    }

    const FVRExpUIInfoPresentationSettings NewSettings = *Settings;
    TArray<UVRExpDetectableUIInfoTriggerLogic *> ChangedLogics;
    ChangedLogics.Add(Logic);

    if (bPropagateFromPreview)
    {
        PrepareForTransaction(ArchetypeLogic);
        FVRExpUIInfoPresentationSettings *MutableArchetypeSettings = GetMutableSettings(ArchetypeLogic, PreviewSource);
        if (MutableArchetypeSettings == nullptr)
        {
            return false;
        }

        if (DeltaResult.bLocationChanged)
        {
            MutableArchetypeSettings->WorldTransform.SetLocation(NewSettings.WorldTransform.GetLocation());
        }
        if (DeltaResult.bScaleChanged)
        {
            MutableArchetypeSettings->WorldTransform.SetScale3D(NewSettings.WorldTransform.GetScale3D());
        }
        if (DeltaResult.bWorldRotationChanged)
        {
            MutableArchetypeSettings->WorldTransform.SetRotation(NewSettings.WorldTransform.GetRotation());
        }
        if (DeltaResult.bFixedRotationChanged)
        {
            MutableArchetypeSettings->FixedWorldRotation = NewSettings.FixedWorldRotation;
        }
        ChangedLogics.AddUnique(ArchetypeLogic);

        for (const FPropagationTarget &Target : PropagationTargets)
        {
            UVRExpDetectableUIInfoTriggerLogic *InstanceLogic = Target.Logic.Get();
            FVRExpUIInfoPresentationSettings *InstanceSettings = GetMutableSettings(InstanceLogic, PreviewSource);
            if (!IsValid(InstanceLogic) || InstanceSettings == nullptr)
            {
                continue;
            }

            bool bInstanceChanged = false;
            const bool bWillChangeInstance = (DeltaResult.bLocationChanged && Target.bUpdateLocation) ||
                                             (DeltaResult.bScaleChanged && Target.bUpdateScale) ||
                                             (DeltaResult.bWorldRotationChanged && Target.bUpdateWorldRotation) ||
                                             (DeltaResult.bFixedRotationChanged && Target.bUpdateFixedRotation);
            if (!bWillChangeInstance)
            {
                continue;
            }

            PrepareForTransaction(InstanceLogic);
            if (DeltaResult.bLocationChanged && Target.bUpdateLocation)
            {
                InstanceSettings->WorldTransform.SetLocation(NewSettings.WorldTransform.GetLocation());
                bInstanceChanged = true;
            }
            if (DeltaResult.bScaleChanged && Target.bUpdateScale)
            {
                InstanceSettings->WorldTransform.SetScale3D(NewSettings.WorldTransform.GetScale3D());
                bInstanceChanged = true;
            }
            if (DeltaResult.bWorldRotationChanged && Target.bUpdateWorldRotation)
            {
                InstanceSettings->WorldTransform.SetRotation(NewSettings.WorldTransform.GetRotation());
                bInstanceChanged = true;
            }
            if (DeltaResult.bFixedRotationChanged && Target.bUpdateFixedRotation)
            {
                InstanceSettings->FixedWorldRotation = NewSettings.FixedWorldRotation;
                bInstanceChanged = true;
            }
            if (bInstanceChanged)
            {
                ChangedLogics.AddUnique(InstanceLogic);
            }
        }
    }

    for (UVRExpDetectableUIInfoTriggerLogic *ChangedLogic : ChangedLogics)
    {
        NotifyLogicModified(ChangedLogic, PreviewSource, EPropertyChangeType::Interactive);
        ModifiedLogics.Add(ChangedLogic);
    }

    bAnyValueChanged = true;
    if (FVRExpUIInfoEditorPreviewManager *PreviewManager = GetVRExpUIInfoEditorPreviewManager())
    {
        PreviewManager->RequestRefresh();
    }
    if (GEditor)
    {
        GEditor->RedrawAllViewports();
    }
    return true;
}

void FVRExpUIInfoComponentVisualizer::FinalizeEditing(bool bNotifyFinalValue)
{
    if (!ActiveTransaction.IsValid())
    {
        ModifiedLogics.Reset();
        bAnyValueChanged = false;
        return;
    }

    if (bNotifyFinalValue)
    {
        TSet<TWeakObjectPtr<AActor>> ModifiedOwners;
        for (const TWeakObjectPtr<UVRExpDetectableUIInfoTriggerLogic> &LogicPointer : ModifiedLogics)
        {
            UVRExpDetectableUIInfoTriggerLogic *Logic = LogicPointer.Get();
            if (!IsValid(Logic))
            {
                continue;
            }

            NotifyLogicModified(Logic, EditedPreviewSource, EPropertyChangeType::ValueSet);
            if (UVRExpDetectableComponent *OuterComponent = Logic->GetTypedOuter<UVRExpDetectableComponent>())
            {
                if (AActor *Owner = OuterComponent->GetOwner())
                {
                    ModifiedOwners.Add(Owner);
                }
            }
        }

        for (const TWeakObjectPtr<AActor> &OwnerPointer : ModifiedOwners)
        {
            if (AActor *Owner = OwnerPointer.Get())
            {
                Owner->PostEditMove(true);
                Owner->MarkPackageDirty();
            }
        }
    }
    else
    {
        ActiveTransaction->Cancel();
    }

    ActiveTransaction.Reset();
    ModifiedLogics.Reset();
    bAnyValueChanged = false;

    if (FVRExpUIInfoEditorPreviewManager *PreviewManager = GetVRExpUIInfoEditorPreviewManager())
    {
        PreviewManager->RequestRefresh();
    }
}

void FVRExpUIInfoComponentVisualizer::NotifyLogicModified(UVRExpDetectableUIInfoTriggerLogic *Logic,
                                                          EVRExpUIInfoEditorPreviewSource PreviewSource,
                                                          EPropertyChangeType::Type ChangeType)
{
    if (!IsValid(Logic))
    {
        return;
    }

    FProperty *SettingsProperty = FindFProperty<FProperty>(UVRExpDetectableUIInfoTriggerLogic::StaticClass(),
                                                           GetSettingsPropertyName(PreviewSource));
    FPropertyChangedEvent PropertyChangedEvent(SettingsProperty, ChangeType);
    static_cast<UObject *>(Logic)->PostEditChangeProperty(PropertyChangedEvent);
    Logic->MarkPackageDirty();

    if (UVRExpDetectableComponent *OuterComponent = Logic->GetTypedOuter<UVRExpDetectableComponent>())
    {
        OuterComponent->MarkPackageDirty();
        if (AActor *Owner = OuterComponent->GetOwner())
        {
            Owner->MarkPackageDirty();
        }
    }
}

FVRExpUIInfoPresentationSettings *
FVRExpUIInfoComponentVisualizer::GetMutableSettings(UVRExpDetectableUIInfoTriggerLogic *Logic,
                                                    EVRExpUIInfoEditorPreviewSource PreviewSource)
{
    if (!IsValid(Logic))
    {
        return nullptr;
    }

    switch (PreviewSource)
    {
    case EVRExpUIInfoEditorPreviewSource::Grip:
        return &Logic->GripPresentationSettings;
    case EVRExpUIInfoEditorPreviewSource::Release:
        return &Logic->ReleasePresentationSettings;
    case EVRExpUIInfoEditorPreviewSource::Motion:
        return &Logic->MotionPresentationSettings;
    case EVRExpUIInfoEditorPreviewSource::Head:
    default:
        return &Logic->HeadPresentationSettings;
    }
}

const FVRExpUIInfoPresentationSettings *
FVRExpUIInfoComponentVisualizer::GetSettings(const UVRExpDetectableUIInfoTriggerLogic *Logic,
                                             EVRExpUIInfoEditorPreviewSource PreviewSource)
{
    return GetMutableSettings(const_cast<UVRExpDetectableUIInfoTriggerLogic *>(Logic), PreviewSource);
}

FName FVRExpUIInfoComponentVisualizer::GetSettingsPropertyName(EVRExpUIInfoEditorPreviewSource PreviewSource)
{
    switch (PreviewSource)
    {
    case EVRExpUIInfoEditorPreviewSource::Grip:
        return GET_MEMBER_NAME_CHECKED(UVRExpDetectableUIInfoTriggerLogic, GripPresentationSettings);
    case EVRExpUIInfoEditorPreviewSource::Release:
        return GET_MEMBER_NAME_CHECKED(UVRExpDetectableUIInfoTriggerLogic, ReleasePresentationSettings);
    case EVRExpUIInfoEditorPreviewSource::Motion:
        return GET_MEMBER_NAME_CHECKED(UVRExpDetectableUIInfoTriggerLogic, MotionPresentationSettings);
    case EVRExpUIInfoEditorPreviewSource::Head:
    default:
        return GET_MEMBER_NAME_CHECKED(UVRExpDetectableUIInfoTriggerLogic, HeadPresentationSettings);
    }
}

FVRExpUIInfoComponentVisualizer::FSettingsDeltaResult
FVRExpUIInfoComponentVisualizer::ApplyDeltaToSettings(FVRExpUIInfoPresentationSettings &Settings,
                                                      const FVector &DeltaTranslate, const FRotator &DeltaRotate,
                                                      const FVector &DeltaScale)
{
    FSettingsDeltaResult Result;
    if (!DeltaTranslate.IsNearlyZero())
    {
        Settings.WorldTransform.SetLocation(Settings.WorldTransform.GetLocation() + DeltaTranslate);
        Result.bLocationChanged = true;
    }

    if (!DeltaScale.IsNearlyZero())
    {
        Settings.WorldTransform.SetScale3D(Settings.WorldTransform.GetScale3D() + DeltaScale);
        Result.bScaleChanged = true;
    }

    if (!DeltaRotate.IsNearlyZero())
    {
        const FQuat DeltaRotation = DeltaRotate.Quaternion();
        if (Settings.OrientationMode == EVRExpUIInfoOrientationMode::InheritAnchor)
        {
            Settings.WorldTransform.SetRotation(
                (DeltaRotation * Settings.WorldTransform.GetRotation()).GetNormalized());
            Result.bWorldRotationChanged = true;
        }
        else if (Settings.OrientationMode == EVRExpUIInfoOrientationMode::FixedWorld)
        {
            Settings.FixedWorldRotation = (DeltaRotation * Settings.FixedWorldRotation.Quaternion()).Rotator();
            Result.bFixedRotationChanged = true;
        }
        else
        {
            Result.bRotationRejected = true;
        }
    }

    return Result;
}

FTransform FVRExpUIInfoComponentVisualizer::GetHandleTransform(const UVRExpDetectableComponent *DetectableComponent,
                                                               const FVRExpUIInfoPresentationSettings &Settings)
{
    FTransform HandleTransform = Settings.WorldTransform;
    if (Settings.OrientationMode == EVRExpUIInfoOrientationMode::FixedWorld)
    {
        HandleTransform.SetRotation(Settings.FixedWorldRotation.Quaternion());
    }
    else if (Settings.OrientationMode == EVRExpUIInfoOrientationMode::FaceCamera ||
             Settings.OrientationMode == EVRExpUIInfoOrientationMode::FaceCameraYawOnly)
    {
        const UChildActorComponent *PreviewComponent =
            IsValid(DetectableComponent) ? DetectableComponent->GetEditorPreviewChildActorComponent() : nullptr;
        if (IsValid(PreviewComponent))
        {
            HandleTransform.SetRotation(PreviewComponent->GetComponentQuat());
        }
    }
    return HandleTransform;
}

void FVRExpUIInfoComponentVisualizer::PrepareForTransaction(UVRExpDetectableUIInfoTriggerLogic *Logic)
{
    if (!IsValid(Logic))
    {
        return;
    }

    Logic->SetFlags(RF_Transactional);
    Logic->Modify();
    if (UVRExpDetectableComponent *OuterComponent = Logic->GetTypedOuter<UVRExpDetectableComponent>())
    {
        OuterComponent->SetFlags(RF_Transactional);
        OuterComponent->Modify();
        if (AActor *Owner = OuterComponent->GetOwner())
        {
            Owner->SetFlags(RF_Transactional);
            Owner->Modify();
        }
    }
}

#undef LOCTEXT_NAMESPACE
