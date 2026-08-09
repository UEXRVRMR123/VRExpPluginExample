#pragma once

#include "TickableEditorObject.h"
#include "UI/VRExpUIInfoTypes.h"

class UArrowComponent;
class UBoxComponent;
class UChildActorComponent;
class USceneComponent;
class UTextRenderComponent;
class UWorld;
class UVRExpDetectableComponent;
class UVRExpDetectableUIInfoTriggerLogic;
struct FVRExpUIInfoPlacementResult;

class FVRExpUIInfoEditorPreviewManager final
    : public FTickableEditorObject
{
public:
    FVRExpUIInfoEditorPreviewManager();
    virtual ~FVRExpUIInfoEditorPreviewManager() override;

    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;

    void RequestRefresh();
    void SetSuspended(bool bInSuspended);
    void CleanupAllPreviews();

    int32 GetActivePreviewCountForTests() const;
    bool GetPreviewTransformForTests(
        const UVRExpDetectableComponent *DetectableComponent,
        FTransform &OutTransform) const;
    void SetWorldFilterForTests(UWorld *World);

private:
    struct FPreviewEntry
    {
        TWeakObjectPtr<UVRExpDetectableUIInfoTriggerLogic> Logic;
        TWeakObjectPtr<USceneComponent> MarkerRoot;
        TWeakObjectPtr<UArrowComponent> XArrow;
        TWeakObjectPtr<UArrowComponent> YArrow;
        TWeakObjectPtr<UArrowComponent> ZArrow;
        TWeakObjectPtr<UBoxComponent> Bounds;
        TWeakObjectPtr<UTextRenderComponent> Label;
        FString LastReportedFailure;
    };

    bool IsPreviewCandidate(
        const UVRExpDetectableComponent *DetectableComponent) const;
    UVRExpDetectableUIInfoTriggerLogic *FindPreviewLogic(
        const UVRExpDetectableComponent *DetectableComponent) const;
    void RefreshPreview(
        UVRExpDetectableComponent *DetectableComponent,
        UVRExpDetectableUIInfoTriggerLogic *Logic,
        FPreviewEntry &Entry);

    bool ResolvePreview(
        const UVRExpDetectableComponent *DetectableComponent,
        const UVRExpDetectableUIInfoTriggerLogic *Logic,
        FVRExpUIInfoPlacementResult &OutResult,
        FString &OutSourceLabel,
        FString &OutFailureReason,
        EVRExpUIInfoEditorPreviewStatus &OutFailureStatus) const;
    bool ResolveEditorViewTransform(
        UWorld *World,
        FTransform &OutViewTransform) const;

    void RefreshMarker(
        UVRExpDetectableComponent *DetectableComponent,
        FPreviewEntry &Entry,
        const FTransform &WorldTransform,
        const FString &Label,
        bool bIsError);
    bool RefreshUIActor(
        UVRExpDetectableComponent *DetectableComponent,
        UVRExpDetectableUIInfoTriggerLogic *Logic,
        const FTransform &TargetWorldTransform,
        FString &OutFailureReason);
    void DestroyMarker(FPreviewEntry &Entry);
    void CleanupEntry(
        UVRExpDetectableComponent *DetectableComponent,
        FPreviewEntry &Entry,
        bool bResetStatus);
    void ReportStatus(
        UVRExpDetectableUIInfoTriggerLogic *Logic,
        EVRExpUIInfoEditorPreviewStatus Status,
        const FString &FailureReason,
        FPreviewEntry &Entry);
    void HandleDetectableUnregistered(
        UVRExpDetectableComponent *DetectableComponent);

    TMap<TWeakObjectPtr<UVRExpDetectableComponent>, FPreviewEntry>
        PreviewEntries;
    bool bSuspended = false;
    bool bRefreshRequested = true;
    float SecondsUntilFullScan = 0.0f;
    FDelegateHandle DetectableUnregisterHandle;
    TWeakObjectPtr<UWorld> WorldFilterForTests;
};

FVRExpUIInfoEditorPreviewManager *
GetVRExpUIInfoEditorPreviewManager();
