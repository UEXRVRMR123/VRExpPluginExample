#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Detection/VRExpDetectableTypes.h"
#include "VRExpHeadDetectionComponent.generated.h"

class AActor;
class UPrimitiveComponent;
class UVRExpDetectableComponent;
struct FCollisionObjectQueryParams;

UCLASS(Blueprintable, ClassGroup = (VRExpansionExtensions), HideCategories = (ComponentTick),
       meta = (BlueprintSpawnableComponent))
class VREXPANSIONEXTENSIONS_API UVRExpHeadDetectionComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UVRExpHeadDetectionComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction *ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "VRExpansionExtensions|Head Detection")
    void SetDetectionEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "VRExpansionExtensions|Head Detection")
    void SetDetectionMode(EVRExpHeadDetectionMode NewDetectionMode);

    UFUNCTION(BlueprintCallable, Category = "VRExpansionExtensions|Head Detection")
    void SetTargetMode(EVRExpHeadDetectionTargetMode NewTargetMode);

    UFUNCTION(BlueprintCallable, Category = "VRExpansionExtensions|Head Detection")
    void RefreshDetectionNow();

    UFUNCTION(BlueprintPure, Category = "VRExpansionExtensions|Head Detection")
    TArray<UVRExpDetectableComponent *> GetCurrentDetectedComponents() const;

    UFUNCTION(BlueprintPure, Category = "VRExpansionExtensions|Head Detection")
    TArray<AActor *> GetCurrentDetectedActors() const;

    UFUNCTION(BlueprintPure, Category = "VRExpansionExtensions|Head Detection")
    bool IsDetectionEnabled() const { return bDetectionEnabled; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
    bool bDetectionEnabled;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
    EVRExpHeadDetectionMode DetectionMode;

    /** SingleClosest returns one nearest valid target. Multiple returns every valid target found by the query. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection",
              meta = (ToolTip = "Single Closest returns one nearest valid target. Multiple returns every valid target found by the selected query. A ray still stops at its first blocking hit."))
    EVRExpHeadDetectionTargetMode TargetMode;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
    bool bOnlyRunForLocallyControlledPawn;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection", meta = (ClampMin = "0.0", Units = "s"))
    float DetectionInterval;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sphere",
              meta = (ClampMin = "0.0", Units = "cm",
                      EditCondition = "DetectionMode == EVRExpHeadDetectionMode::SphereOverlap",
                      EditConditionHides))
    float SphereRadius;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ray",
              meta = (ClampMin = "0.0", Units = "cm",
                      EditCondition = "DetectionMode == EVRExpHeadDetectionMode::ForwardRay",
                      EditConditionHides))
    float RayDistance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ray",
              meta = (EditCondition = "DetectionMode == EVRExpHeadDetectionMode::ForwardRay",
                      EditConditionHides))
    TEnumAsByte<ECollisionChannel> RayTraceChannel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cone",
              meta = (ClampMin = "0.0", Units = "cm",
                      EditCondition = "DetectionMode == EVRExpHeadDetectionMode::ForwardCone",
                      EditConditionHides))
    float ConeDistance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cone",
              meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg",
                      EditCondition = "DetectionMode == EVRExpHeadDetectionMode::ForwardCone",
                      EditConditionHides))
    float ConeHalfAngleDegrees;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cone",
              meta = (EditCondition = "DetectionMode == EVRExpHeadDetectionMode::ForwardCone",
                      EditConditionHides))
    bool bConeRequiresLineOfSight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cone",
              meta = (EditCondition = "DetectionMode == EVRExpHeadDetectionMode::ForwardCone && bConeRequiresLineOfSight",
                      EditConditionHides))
    TEnumAsByte<ECollisionChannel> ConeLineOfSightChannel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
    TArray<TEnumAsByte<EObjectTypeQuery>> DetectionObjectTypes;

    /**
     * Draws the active query shape and a fixed screen summary in PIE/Game.
     * Red: no physics query hits. Orange: physics hits were rejected.
     * Green: one valid target selected. Cyan: multiple valid targets selected.
     * Text includes counts, raw/selected objects and concrete rejection reasons.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug",
              meta = (DisplayName = "Draw Runtime Debug",
                      ToolTip = "Draw the current query in PIE/Game and show a fixed, non-stacking screen message. Red = no query hits; Orange = hits rejected; Green = one selected target; Cyan = multiple selected targets."))
    bool bDrawDebug;

    /** Optional world labels. Disabled by default because labels close to the ray impact can overlap. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug",
              meta = (DisplayName = "Draw World Target Labels",
                      EditCondition = "bDrawDebug", EditConditionHides,
                      ToolTip = "Draw short Raw/Selected labels beside world targets. The fixed screen summary remains available when this is disabled."))
    bool bDrawDebugWorldLabels;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug",
              meta = (DisplayName = "Debug Screen Text Scale",
                      EditCondition = "bDrawDebug", EditConditionHides,
                      ClampMin = "0.5", ClampMax = "3.0",
                      ToolTip = "Scale of the fixed on-screen debug summary."))
    float DebugScreenTextScale;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug|Runtime")
    EVRExpHeadDetectionDebugState LastDebugState;

    /** Number of raw trace hits or broad-phase overlaps before detectable filtering. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug|Runtime")
    int32 LastQueryHitCount;

    /** Number of valid UVRExpDetectableComponent targets before SingleClosest/Multiple selection. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug|Runtime")
    int32 LastValidTargetCount;

    /** Number of targets actually applied after SingleClosest/Multiple selection. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug|Runtime")
    int32 LastSelectedTargetCount;

    /** Closest raw physics-query Actor, even if it was rejected by detectable filtering. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug|Runtime")
    TObjectPtr<AActor> LastQueryHitActor;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug|Runtime")
    TObjectPtr<UPrimitiveComponent> LastQueryHitPrimitive;

    /** Why the closest raw hit was rejected. None means it was accepted or no raw primitive exists. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug|Runtime")
    EVRExpHeadDetectionRejectionReason LastQueryHitRejectionReason;

    /** Human-readable nearest-hit reason plus aggregate rejection counts for the last query. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug|Runtime")
    FString LastRejectionSummary;

    /** Primary selected valid Actor. In Multiple mode use GetCurrentDetectedActors for the full list. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug|Runtime")
    TObjectPtr<AActor> LastSelectedActor;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Debug|Runtime")
    TObjectPtr<UPrimitiveComponent> LastSelectedPrimitive;

    UPROPERTY(BlueprintAssignable, Category = "VRExpansionExtensions|Head Detection")
    FVRExpHeadDetectionEvent OnDetectionStarted;

    UPROPERTY(BlueprintAssignable, Category = "VRExpansionExtensions|Head Detection")
    FVRExpHeadDetectionEvent OnDetectionEnded;

private:
    using FDetectionMap = TMap<TWeakObjectPtr<UVRExpDetectableComponent>, TWeakObjectPtr<UPrimitiveComponent>>;

    bool ShouldRunDetection() const;
    bool IsPrimitiveObjectTypeEnabled(const UPrimitiveComponent *PrimitiveComponent) const;
    FCollisionObjectQueryParams BuildObjectQueryParams() const;
    void PerformSphereDetection(FDetectionMap &OutDetections);
    void PerformRayDetection(FDetectionMap &OutDetections);
    void PerformConeDetection(FDetectionMap &OutDetections);
    int32 AddDetectablesForPrimitive(UPrimitiveComponent *PrimitiveComponent, FDetectionMap &OutDetections);
    bool HasConeLineOfSight(UPrimitiveComponent *PrimitiveComponent, const FVector &TargetPoint) const;
    void ApplyTargetMode(FDetectionMap &InOutDetections) const;
    void UpdateDebugSelectionState(const FDetectionMap &Detections);
    void RecordRawQueryPrimitive(UPrimitiveComponent *PrimitiveComponent, const FVector &QueryPoint);
    void RecordPrimitiveRejection(UPrimitiveComponent *PrimitiveComponent,
                                  EVRExpHeadDetectionRejectionReason Reason);
    FString BuildRejectionSummary() const;
    void DrawDetectionDebug(const FDetectionMap &Detections) const;
    FColor GetDebugColor() const;
    FString GetDebugColorName() const;
    uint64 GetDebugScreenMessageKey() const;
    void ClearDebugScreenMessage() const;
    void ResetDebugState();
    void ApplyDetectionResults(FDetectionMap &&NewDetections);
    void ClearCurrentDetections();

    FDetectionMap CurrentDetections;
    TArray<FVector> LastQueryImpactPoints;
    TMap<TWeakObjectPtr<UPrimitiveComponent>, float> LastQueryPrimitiveDistances;
    TMap<TWeakObjectPtr<UPrimitiveComponent>, EVRExpHeadDetectionRejectionReason>
        LastPrimitiveRejectionReasons;
    float DetectionElapsedTime;
    EVRExpHeadDetectionMode LastDetectionMode;
    EVRExpHeadDetectionTargetMode LastTargetMode;
    float LastQueryHitDistanceSquared;
    FVector LastQueryHitPoint;
};
