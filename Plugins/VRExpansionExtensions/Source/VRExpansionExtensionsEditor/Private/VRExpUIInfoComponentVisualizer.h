#pragma once

#include "ComponentVisualizer.h"
#include "UI/VRExpUIInfoTypes.h"

class FScopedTransaction;
class UVRExpDetectableComponent;
class UVRExpDetectableUIInfoTriggerLogic;

class FVRExpUIInfoComponentVisualizer final : public FComponentVisualizer
{
public:
    FVRExpUIInfoComponentVisualizer();
    virtual ~FVRExpUIInfoComponentVisualizer() override;

    virtual void DrawVisualization(const UActorComponent *Component, const FSceneView *View,
                                   FPrimitiveDrawInterface *PDI) override;
    virtual void DrawVisualizationHUD(const UActorComponent *Component, const FViewport *Viewport,
                                      const FSceneView *View, FCanvas *Canvas) override;
    virtual bool VisProxyHandleClick(FEditorViewportClient *ViewportClient, HComponentVisProxy *VisProxy,
                                     const FViewportClick &Click) override;
    virtual bool GetWidgetLocation(const FEditorViewportClient *ViewportClient, FVector &OutLocation) const override;
    virtual bool GetCustomInputCoordinateSystem(const FEditorViewportClient *ViewportClient,
                                                FMatrix &OutMatrix) const override;
    virtual bool HandleInputDelta(FEditorViewportClient *ViewportClient, FViewport *Viewport, FVector &DeltaTranslate,
                                  FRotator &DeltaRotate, FVector &DeltaScale) override;
    virtual void TrackingStarted(FEditorViewportClient *ViewportClient) override;
    virtual void TrackingStopped(FEditorViewportClient *ViewportClient, bool bDidMove) override;
    virtual void EndEditing() override;
    virtual UActorComponent *GetEditedComponent() const override;
    virtual bool IsVisualizingArchetype() const override;

    bool BeginEditingPreview(UVRExpDetectableComponent *DetectableComponent);

#if WITH_DEV_AUTOMATION_TESTS
    bool SelectPreviewForTests(UVRExpDetectableComponent *DetectableComponent);
    bool ApplyInputDeltaForTests(const FVector &DeltaTranslate, const FRotator &DeltaRotate, const FVector &DeltaScale,
                                 bool bReportedDidMove = true);
    static FBox GetPreviewInteractionBoundsForTests(const UVRExpDetectableComponent *DetectableComponent,
                                                    const FTransform &HandleTransform,
                                                    EVRExpUIInfoEditorPreviewMode PreviewMode);
#endif

private:
    struct FSettingsDeltaResult
    {
        bool bLocationChanged = false;
        bool bScaleChanged = false;
        bool bWorldRotationChanged = false;
        bool bFixedRotationChanged = false;
        bool bRotationRejected = false;

        bool HasAnyChange() const
        {
            return bLocationChanged || bScaleChanged || bWorldRotationChanged || bFixedRotationChanged;
        }
    };

    bool SelectPreview(UVRExpDetectableComponent *DetectableComponent);
    bool ResolveFirstPreviewLogic(const UVRExpDetectableComponent *DetectableComponent, int32 &OutLogicIndex,
                                  UVRExpDetectableUIInfoTriggerLogic *&OutLogic) const;
    UVRExpDetectableComponent *ResolveEditedComponent() const;
    UVRExpDetectableUIInfoTriggerLogic *ResolveEditedLogic() const;
    bool ResolveEditableSettings(const UVRExpDetectableComponent *DetectableComponent, int32 LogicIndex,
                                 EVRExpUIInfoEditorPreviewSource PreviewSource,
                                 UVRExpDetectableUIInfoTriggerLogic *&OutLogic,
                                 FVRExpUIInfoPresentationSettings *&OutSettings,
                                 FString *OutReadOnlyReason = nullptr) const;
    bool ApplyDeltaAndPropagate(UVRExpDetectableComponent *DetectableComponent, int32 LogicIndex,
                                EVRExpUIInfoEditorPreviewSource PreviewSource, const FVector &DeltaTranslate,
                                const FRotator &DeltaRotate, const FVector &DeltaScale);
    void FinalizeEditing(bool bNotifyFinalValue);
    void NotifyLogicModified(UVRExpDetectableUIInfoTriggerLogic *Logic, EVRExpUIInfoEditorPreviewSource PreviewSource,
                             EPropertyChangeType::Type ChangeType);

    static FVRExpUIInfoPresentationSettings *GetMutableSettings(UVRExpDetectableUIInfoTriggerLogic *Logic,
                                                                EVRExpUIInfoEditorPreviewSource PreviewSource);
    static const FVRExpUIInfoPresentationSettings *GetSettings(const UVRExpDetectableUIInfoTriggerLogic *Logic,
                                                               EVRExpUIInfoEditorPreviewSource PreviewSource);
    static FName GetSettingsPropertyName(EVRExpUIInfoEditorPreviewSource PreviewSource);
    static FSettingsDeltaResult ApplyDeltaToSettings(FVRExpUIInfoPresentationSettings &Settings,
                                                     const FVector &DeltaTranslate, const FRotator &DeltaRotate,
                                                     const FVector &DeltaScale);
    static FTransform GetHandleTransform(const UVRExpDetectableComponent *DetectableComponent,
                                         const FVRExpUIInfoPresentationSettings &Settings);
    static void PrepareForTransaction(UVRExpDetectableUIInfoTriggerLogic *Logic);

    FComponentPropertyPath EditedComponentPath;
    int32 EditedLogicIndex = INDEX_NONE;
    EVRExpUIInfoEditorPreviewSource EditedPreviewSource = EVRExpUIInfoEditorPreviewSource::Head;
    TUniquePtr<FScopedTransaction> ActiveTransaction;
    TSet<TWeakObjectPtr<UVRExpDetectableUIInfoTriggerLogic>> ModifiedLogics;
    bool bAnyValueChanged = false;
};
