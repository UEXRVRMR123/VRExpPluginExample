#include "Detection/VRExpHeadDetectionComponent.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Detection/VRExpDetectableComponent.h"

namespace
{
FString GetCollisionChannelDisplayName(const ECollisionChannel Channel)
{
    if (const UCollisionProfile *CollisionProfile =
            UCollisionProfile::Get())
    {
        const FName ConfiguredChannelName =
            CollisionProfile->ReturnChannelNameFromContainerIndex(
                static_cast<int32>(Channel));
        if (!ConfiguredChannelName.IsNone())
        {
            return ConfiguredChannelName.ToString();
        }
    }

    const UEnum *ChannelEnum = StaticEnum<ECollisionChannel>();
    return ChannelEnum
               ? ChannelEnum->GetDisplayNameTextByValue(
                                static_cast<int64>(Channel))
                     .ToString()
               : FString::FromInt(static_cast<int32>(Channel));
}
} // namespace

UVRExpHeadDetectionComponent::UVRExpHeadDetectionComponent()
    : bDetectionEnabled(true), DetectionMode(EVRExpHeadDetectionMode::ForwardRay),
      TargetMode(EVRExpHeadDetectionTargetMode::SingleClosest), bOnlyRunForLocallyControlledPawn(true),
      DetectionInterval(0.05f), SphereRadius(20.0f), RayDistance(100.0f),
      RayTraceChannel(ECC_Visibility), ConeDistance(150.0f), ConeHalfAngleDegrees(30.0f),
      bConeRequiresLineOfSight(true), ConeLineOfSightChannel(ECC_Visibility), bDrawDebug(false),
      bDrawDebugWorldLabels(false), DebugScreenTextScale(1.0f),
      LastDebugState(EVRExpHeadDetectionDebugState::NotRunning), LastQueryHitCount(0),
      LastValidTargetCount(0), LastSelectedTargetCount(0), LastQueryHitActor(nullptr),
      LastQueryHitPrimitive(nullptr),
      LastQueryHitRejectionReason(EVRExpHeadDetectionRejectionReason::None),
      LastRejectionSummary(TEXT("None")), LastSelectedActor(nullptr),
      LastSelectedPrimitive(nullptr),
      DetectionElapsedTime(0.0f), LastDetectionMode(DetectionMode), LastTargetMode(TargetMode),
      LastQueryHitDistanceSquared(TNumericLimits<float>::Max()), LastQueryHitPoint(FVector::ZeroVector)
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

    DetectionObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
    DetectionObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
}

void UVRExpHeadDetectionComponent::BeginPlay()
{
    Super::BeginPlay();

    LastDetectionMode = DetectionMode;
    LastTargetMode = TargetMode;
    DetectionElapsedTime = DetectionInterval;

    if (USceneComponent *ParentComponent = GetAttachParent())
    {
        AddTickPrerequisiteComponent(ParentComponent);
    }
}

void UVRExpHeadDetectionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearCurrentDetections();
    Super::EndPlay(EndPlayReason);
}

void UVRExpHeadDetectionComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    ClearCurrentDetections();
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UVRExpHeadDetectionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                 FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bDrawDebug)
    {
        ClearDebugScreenMessage();
    }

    if (LastDetectionMode != DetectionMode || LastTargetMode != TargetMode)
    {
        ClearCurrentDetections();
        LastDetectionMode = DetectionMode;
        LastTargetMode = TargetMode;
        DetectionElapsedTime = DetectionInterval;
    }

    if (!bDetectionEnabled || !ShouldRunDetection())
    {
        ClearCurrentDetections();
        return;
    }

    DetectionElapsedTime += DeltaTime;
    if (DetectionInterval > 0.0f && DetectionElapsedTime < DetectionInterval)
    {
        return;
    }

    DetectionElapsedTime = 0.0f;
    RefreshDetectionNow();
}

void UVRExpHeadDetectionComponent::SetDetectionEnabled(bool bEnabled)
{
    if (bDetectionEnabled == bEnabled)
    {
        return;
    }

    bDetectionEnabled = bEnabled;
    DetectionElapsedTime = DetectionInterval;

    if (!bDetectionEnabled)
    {
        ClearCurrentDetections();
    }
    else
    {
        RefreshDetectionNow();
    }
}

void UVRExpHeadDetectionComponent::SetDetectionMode(EVRExpHeadDetectionMode NewDetectionMode)
{
    if (DetectionMode == NewDetectionMode)
    {
        return;
    }

    ClearCurrentDetections();
    DetectionMode = NewDetectionMode;
    LastDetectionMode = NewDetectionMode;
    DetectionElapsedTime = DetectionInterval;

    if (bDetectionEnabled)
    {
        RefreshDetectionNow();
    }
}

void UVRExpHeadDetectionComponent::SetTargetMode(EVRExpHeadDetectionTargetMode NewTargetMode)
{
    if (TargetMode == NewTargetMode)
    {
        return;
    }

    ClearCurrentDetections();
    TargetMode = NewTargetMode;
    LastTargetMode = NewTargetMode;
    DetectionElapsedTime = DetectionInterval;

    if (bDetectionEnabled)
    {
        RefreshDetectionNow();
    }
}

void UVRExpHeadDetectionComponent::RefreshDetectionNow()
{
    if (!bDetectionEnabled || !ShouldRunDetection() || !GetWorld())
    {
        ClearCurrentDetections();
        return;
    }

    ResetDebugState();

    FDetectionMap NewDetections;
    switch (DetectionMode)
    {
    case EVRExpHeadDetectionMode::SphereOverlap:
        PerformSphereDetection(NewDetections);
        break;

    case EVRExpHeadDetectionMode::ForwardRay:
        PerformRayDetection(NewDetections);
        break;

    case EVRExpHeadDetectionMode::ForwardCone:
        PerformConeDetection(NewDetections);
        break;

    default:
        break;
    }

    LastValidTargetCount = NewDetections.Num();
    ApplyTargetMode(NewDetections);
    UpdateDebugSelectionState(NewDetections);
    DrawDetectionDebug(NewDetections);
    ApplyDetectionResults(MoveTemp(NewDetections));
}

TArray<UVRExpDetectableComponent *> UVRExpHeadDetectionComponent::GetCurrentDetectedComponents() const
{
    TArray<UVRExpDetectableComponent *> Result;
    Result.Reserve(CurrentDetections.Num());

    for (const TPair<TWeakObjectPtr<UVRExpDetectableComponent>, TWeakObjectPtr<UPrimitiveComponent>> &Pair :
         CurrentDetections)
    {
        if (Pair.Key.IsValid())
        {
            Result.Add(Pair.Key.Get());
        }
    }

    return Result;
}

TArray<AActor *> UVRExpHeadDetectionComponent::GetCurrentDetectedActors() const
{
    TArray<AActor *> Result;
    Result.Reserve(CurrentDetections.Num());

    for (const TPair<TWeakObjectPtr<UVRExpDetectableComponent>, TWeakObjectPtr<UPrimitiveComponent>> &Pair :
         CurrentDetections)
    {
        UVRExpDetectableComponent *DetectableComponent = Pair.Key.Get();
        AActor *DetectedActor = IsValid(DetectableComponent) ? DetectableComponent->GetOwner() : nullptr;
        if (IsValid(DetectedActor))
        {
            Result.AddUnique(DetectedActor);
        }
    }

    return Result;
}

bool UVRExpHeadDetectionComponent::ShouldRunDetection() const
{
    if (!bOnlyRunForLocallyControlledPawn)
    {
        return true;
    }

    const APawn *OwningPawn = Cast<APawn>(GetOwner());
    return OwningPawn && OwningPawn->IsLocallyControlled();
}

bool UVRExpHeadDetectionComponent::IsPrimitiveObjectTypeEnabled(const UPrimitiveComponent *PrimitiveComponent) const
{
    if (!IsValid(PrimitiveComponent))
    {
        return false;
    }

    const ECollisionChannel PrimitiveObjectType = PrimitiveComponent->GetCollisionObjectType();
    for (const TEnumAsByte<EObjectTypeQuery> ObjectType : DetectionObjectTypes)
    {
        if (UEngineTypes::ConvertToCollisionChannel(ObjectType) == PrimitiveObjectType)
        {
            return true;
        }
    }

    return false;
}

FCollisionObjectQueryParams UVRExpHeadDetectionComponent::BuildObjectQueryParams() const
{
    FCollisionObjectQueryParams ObjectQueryParams;
    for (const TEnumAsByte<EObjectTypeQuery> ObjectType : DetectionObjectTypes)
    {
        const ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(ObjectType);
        if (CollisionChannel != ECC_MAX)
        {
            ObjectQueryParams.AddObjectTypesToQuery(CollisionChannel);
        }
    }

    return ObjectQueryParams;
}

void UVRExpHeadDetectionComponent::PerformSphereDetection(FDetectionMap &OutDetections)
{
    UWorld *World = GetWorld();
    if (!World || SphereRadius <= 0.0f)
    {
        return;
    }

    const FVector Origin = GetComponentLocation();
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VRExpHeadSphereDetection), false, GetOwner());
    TArray<FOverlapResult> OverlapResults;
    World->OverlapMultiByObjectType(OverlapResults, Origin, FQuat::Identity, BuildObjectQueryParams(),
                                    FCollisionShape::MakeSphere(SphereRadius), QueryParams);

    LastQueryHitCount = OverlapResults.Num();
    for (const FOverlapResult &OverlapResult : OverlapResults)
    {
        UPrimitiveComponent *PrimitiveComponent = OverlapResult.Component.Get();
        FVector QueryPoint =
            IsValid(PrimitiveComponent) ? PrimitiveComponent->Bounds.Origin
                                        : Origin;
        FVector ClosestPoint = QueryPoint;
        if (IsValid(PrimitiveComponent) &&
            PrimitiveComponent->GetClosestPointOnCollision(
                Origin, ClosestPoint) >= 0.0f)
        {
            QueryPoint = ClosestPoint;
        }

        RecordRawQueryPrimitive(PrimitiveComponent, QueryPoint);
        AddDetectablesForPrimitive(PrimitiveComponent, OutDetections);
    }
}

void UVRExpHeadDetectionComponent::PerformRayDetection(FDetectionMap &OutDetections)
{
    UWorld *World = GetWorld();
    if (!World || RayDistance <= 0.0f)
    {
        return;
    }

    const FVector Origin = GetComponentLocation();
    const FVector End = Origin + GetForwardVector() * RayDistance;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VRExpHeadRayDetection), true, GetOwner());
    TArray<FHitResult> HitResults;
    World->LineTraceMultiByChannel(HitResults, Origin, End, RayTraceChannel, QueryParams);

    HitResults.Sort([](const FHitResult &Left, const FHitResult &Right) { return Left.Distance < Right.Distance; });

    LastQueryHitCount = HitResults.Num();
    for (const FHitResult &HitResult : HitResults)
    {
        UPrimitiveComponent *HitPrimitive = HitResult.GetComponent();
        const FVector QueryPoint = HitResult.bBlockingHit || HitResult.bStartPenetrating
                                       ? HitResult.ImpactPoint
                                       : HitResult.Location;
        LastQueryImpactPoints.Add(QueryPoint);
        RecordRawQueryPrimitive(HitPrimitive, QueryPoint);
    }

    for (const FHitResult &HitResult : HitResults)
    {
        UPrimitiveComponent *HitPrimitive = HitResult.GetComponent();
        if (!IsPrimitiveObjectTypeEnabled(HitPrimitive))
        {
            RecordPrimitiveRejection(
                HitPrimitive,
                EVRExpHeadDetectionRejectionReason::ObjectTypeNotEnabled);
            continue;
        }

        AddDetectablesForPrimitive(HitPrimitive, OutDetections);
    }
}

void UVRExpHeadDetectionComponent::PerformConeDetection(FDetectionMap &OutDetections)
{
    UWorld *World = GetWorld();
    if (!World || ConeDistance <= 0.0f)
    {
        return;
    }

    const FVector Origin = GetComponentLocation();
    const FVector Forward = GetForwardVector().GetSafeNormal();
    const float MinDirectionDot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(ConeHalfAngleDegrees, 0.0f, 180.0f)));

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VRExpHeadConeBroadPhase), false, GetOwner());
    TArray<FOverlapResult> OverlapResults;
    World->OverlapMultiByObjectType(OverlapResults, Origin, FQuat::Identity, BuildObjectQueryParams(),
                                    FCollisionShape::MakeSphere(ConeDistance), QueryParams);

    LastQueryHitCount = OverlapResults.Num();
    for (const FOverlapResult &OverlapResult : OverlapResults)
    {
        UPrimitiveComponent *PrimitiveComponent = OverlapResult.Component.Get();
        if (!IsValid(PrimitiveComponent))
        {
            continue;
        }

        FVector TargetPoint = PrimitiveComponent->Bounds.Origin;
        FVector ClosestPoint = TargetPoint;
        if (PrimitiveComponent->GetClosestPointOnCollision(Origin, ClosestPoint) >= 0.0f)
        {
            TargetPoint = ClosestPoint;
        }
        RecordRawQueryPrimitive(PrimitiveComponent, TargetPoint);

        const FVector ToTarget = TargetPoint - Origin;
        if (ToTarget.SizeSquared() > FMath::Square(ConeDistance))
        {
            RecordPrimitiveRejection(
                PrimitiveComponent,
                EVRExpHeadDetectionRejectionReason::OutsideCone);
            continue;
        }

        const FVector TargetDirection = ToTarget.GetSafeNormal();
        if (!TargetDirection.IsNearlyZero() && FVector::DotProduct(Forward, TargetDirection) < MinDirectionDot)
        {
            RecordPrimitiveRejection(
                PrimitiveComponent,
                EVRExpHeadDetectionRejectionReason::OutsideCone);
            continue;
        }

        if (bConeRequiresLineOfSight && !HasConeLineOfSight(PrimitiveComponent, TargetPoint))
        {
            RecordPrimitiveRejection(
                PrimitiveComponent,
                EVRExpHeadDetectionRejectionReason::LineOfSightBlocked);
            continue;
        }

        AddDetectablesForPrimitive(PrimitiveComponent, OutDetections);
    }
}

int32 UVRExpHeadDetectionComponent::AddDetectablesForPrimitive(UPrimitiveComponent *PrimitiveComponent,
                                                               FDetectionMap &OutDetections)
{
    if (!IsValid(PrimitiveComponent))
    {
        return 0;
    }

    AActor *PrimitiveOwner = PrimitiveComponent->GetOwner();
    if (!IsValid(PrimitiveOwner) || PrimitiveOwner == GetOwner())
    {
        RecordPrimitiveRejection(
            PrimitiveComponent,
            EVRExpHeadDetectionRejectionReason::OwnerRejected);
        return 0;
    }

    TArray<UVRExpDetectableComponent *> DetectableComponents;
    PrimitiveOwner->GetComponents<UVRExpDetectableComponent>(DetectableComponents);

    int32 AddedCount = 0;
    bool bHasValidDetectableComponent = false;
    bool bAcceptedPrimitive = false;
    for (UVRExpDetectableComponent *DetectableComponent : DetectableComponents)
    {
        if (!IsValid(DetectableComponent))
        {
            continue;
        }

        bHasValidDetectableComponent = true;
        if (DetectableComponent->AcceptsDetectedPrimitive(PrimitiveComponent))
        {
            bAcceptedPrimitive = true;
            const TWeakObjectPtr<UVRExpDetectableComponent> DetectableKey(
                DetectableComponent);
            const TWeakObjectPtr<UPrimitiveComponent> PrimitiveKey(
                PrimitiveComponent);
            if (TWeakObjectPtr<UPrimitiveComponent> *ExistingPrimitive =
                    OutDetections.Find(DetectableKey))
            {
                const float *ExistingDistance =
                    LastQueryPrimitiveDistances.Find(*ExistingPrimitive);
                const float *NewDistance =
                    LastQueryPrimitiveDistances.Find(PrimitiveKey);
                if (NewDistance &&
                    (!ExistingDistance || *NewDistance < *ExistingDistance))
                {
                    *ExistingPrimitive = PrimitiveKey;
                }
            }
            else
            {
                OutDetections.Add(DetectableKey, PrimitiveKey);
                ++AddedCount;
            }
        }
    }

    if (bAcceptedPrimitive)
    {
        LastPrimitiveRejectionReasons.Remove(
            TWeakObjectPtr<UPrimitiveComponent>(PrimitiveComponent));
    }
    else
    {
        RecordPrimitiveRejection(
            PrimitiveComponent,
            bHasValidDetectableComponent
                ? EVRExpHeadDetectionRejectionReason::PrimitiveModeRejected
                : EVRExpHeadDetectionRejectionReason::NoDetectableComponent);
    }

    return AddedCount;
}

bool UVRExpHeadDetectionComponent::HasConeLineOfSight(UPrimitiveComponent *PrimitiveComponent,
                                                      const FVector &TargetPoint) const
{
    UWorld *World = GetWorld();
    if (!World || !IsValid(PrimitiveComponent))
    {
        return false;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VRExpHeadConeLineOfSight), true, GetOwner());
    FHitResult HitResult;
    const bool bHit = World->LineTraceSingleByChannel(HitResult, GetComponentLocation(), TargetPoint,
                                                      ConeLineOfSightChannel, QueryParams);

    return !bHit || HitResult.GetComponent() == PrimitiveComponent ||
           HitResult.GetActor() == PrimitiveComponent->GetOwner();
}

void UVRExpHeadDetectionComponent::ApplyTargetMode(FDetectionMap &InOutDetections) const
{
    if (TargetMode != EVRExpHeadDetectionTargetMode::SingleClosest || InOutDetections.Num() <= 1)
    {
        return;
    }

    const FVector Origin = GetComponentLocation();
    float ClosestDistanceSquared = TNumericLimits<float>::Max();
    TWeakObjectPtr<UVRExpDetectableComponent> ClosestDetectable;
    TWeakObjectPtr<UPrimitiveComponent> ClosestPrimitive;

    for (const TPair<TWeakObjectPtr<UVRExpDetectableComponent>, TWeakObjectPtr<UPrimitiveComponent>> &Pair :
         InOutDetections)
    {
        UPrimitiveComponent *PrimitiveComponent = Pair.Value.Get();
        if (!Pair.Key.IsValid() || !IsValid(PrimitiveComponent))
        {
            continue;
        }

        FVector TargetPoint = PrimitiveComponent->Bounds.Origin;
        FVector ClosestPoint = TargetPoint;
        if (PrimitiveComponent->GetClosestPointOnCollision(Origin, ClosestPoint) >= 0.0f)
        {
            TargetPoint = ClosestPoint;
        }

        float DistanceSquared = FVector::DistSquared(Origin, TargetPoint);
        if (const float *RecordedDistanceSquared =
                LastQueryPrimitiveDistances.Find(Pair.Value))
        {
            DistanceSquared = *RecordedDistanceSquared;
        }

        if (DistanceSquared < ClosestDistanceSquared)
        {
            ClosestDistanceSquared = DistanceSquared;
            ClosestDetectable = Pair.Key;
            ClosestPrimitive = Pair.Value;
        }
    }

    InOutDetections.Reset();
    if (ClosestDetectable.IsValid() && ClosestPrimitive.IsValid())
    {
        InOutDetections.Add(ClosestDetectable, ClosestPrimitive);
    }
}

void UVRExpHeadDetectionComponent::UpdateDebugSelectionState(const FDetectionMap &Detections)
{
    LastSelectedTargetCount = Detections.Num();
    LastSelectedActor = nullptr;
    LastSelectedPrimitive = nullptr;

    const FVector Origin = GetComponentLocation();
    float ClosestDistanceSquared = TNumericLimits<float>::Max();
    for (const TPair<TWeakObjectPtr<UVRExpDetectableComponent>, TWeakObjectPtr<UPrimitiveComponent>> &Pair :
         Detections)
    {
        UPrimitiveComponent *PrimitiveComponent = Pair.Value.Get();
        UVRExpDetectableComponent *DetectableComponent = Pair.Key.Get();
        if (!IsValid(PrimitiveComponent) || !IsValid(DetectableComponent))
        {
            continue;
        }

        FVector TargetPoint = PrimitiveComponent->Bounds.Origin;
        FVector ClosestPoint = TargetPoint;
        if (PrimitiveComponent->GetClosestPointOnCollision(Origin, ClosestPoint) >= 0.0f)
        {
            TargetPoint = ClosestPoint;
        }

        const float DistanceSquared = FVector::DistSquared(Origin, TargetPoint);
        if (DistanceSquared < ClosestDistanceSquared)
        {
            ClosestDistanceSquared = DistanceSquared;
            LastSelectedActor = DetectableComponent->GetOwner();
            LastSelectedPrimitive = PrimitiveComponent;
        }
    }

    if (LastQueryHitCount <= 0)
    {
        LastDebugState = EVRExpHeadDetectionDebugState::NoQueryHits;
    }
    else if (LastSelectedTargetCount <= 0)
    {
        LastDebugState = EVRExpHeadDetectionDebugState::QueryHitsRejected;
    }
    else if (LastSelectedTargetCount == 1)
    {
        LastDebugState = EVRExpHeadDetectionDebugState::SingleTargetSelected;
    }
    else
    {
        LastDebugState = EVRExpHeadDetectionDebugState::MultipleTargetsSelected;
    }

    LastQueryHitRejectionReason =
        EVRExpHeadDetectionRejectionReason::None;
    if (LastQueryHitPrimitive.Get())
    {
        const TWeakObjectPtr<UPrimitiveComponent> PrimitiveKey(
            LastQueryHitPrimitive.Get());
        if (const EVRExpHeadDetectionRejectionReason *Reason =
                LastPrimitiveRejectionReasons.Find(PrimitiveKey))
        {
            LastQueryHitRejectionReason = *Reason;
        }
    }

    LastRejectionSummary = BuildRejectionSummary();
}

void UVRExpHeadDetectionComponent::RecordRawQueryPrimitive(UPrimitiveComponent *PrimitiveComponent,
                                                            const FVector &QueryPoint)
{
    if (!IsValid(PrimitiveComponent))
    {
        return;
    }

    const float DistanceSquared = FVector::DistSquared(GetComponentLocation(), QueryPoint);
    const TWeakObjectPtr<UPrimitiveComponent> PrimitiveKey(PrimitiveComponent);
    if (float *RecordedDistanceSquared =
            LastQueryPrimitiveDistances.Find(PrimitiveKey))
    {
        *RecordedDistanceSquared =
            FMath::Min(*RecordedDistanceSquared, DistanceSquared);
    }
    else
    {
        LastQueryPrimitiveDistances.Add(PrimitiveKey, DistanceSquared);
    }

    if (DistanceSquared < LastQueryHitDistanceSquared)
    {
        LastQueryHitDistanceSquared = DistanceSquared;
        LastQueryHitActor = PrimitiveComponent->GetOwner();
        LastQueryHitPrimitive = PrimitiveComponent;
        LastQueryHitPoint = QueryPoint;
    }
}

void UVRExpHeadDetectionComponent::RecordPrimitiveRejection(
    UPrimitiveComponent *PrimitiveComponent,
    EVRExpHeadDetectionRejectionReason Reason)
{
    if (!IsValid(PrimitiveComponent) ||
        Reason == EVRExpHeadDetectionRejectionReason::None)
    {
        return;
    }

    LastPrimitiveRejectionReasons.Add(
        TWeakObjectPtr<UPrimitiveComponent>(PrimitiveComponent), Reason);
}

FString UVRExpHeadDetectionComponent::BuildRejectionSummary() const
{
    TMap<EVRExpHeadDetectionRejectionReason, int32> ReasonCounts;
    for (const TPair<TWeakObjectPtr<UPrimitiveComponent>,
                     EVRExpHeadDetectionRejectionReason> &Pair :
         LastPrimitiveRejectionReasons)
    {
        if (Pair.Key.IsValid())
        {
            ReasonCounts.FindOrAdd(Pair.Value)++;
        }
    }

    TArray<FString> CountParts;
    const auto AddReasonCount =
        [&ReasonCounts, &CountParts](
            const EVRExpHeadDetectionRejectionReason Reason,
            const TCHAR *Label)
    {
        if (const int32 *Count = ReasonCounts.Find(Reason))
        {
            CountParts.Add(FString::Printf(TEXT("%s=%d"), Label, *Count));
        }
    };

    AddReasonCount(
        EVRExpHeadDetectionRejectionReason::ObjectTypeNotEnabled,
        TEXT("ObjectType"));
    AddReasonCount(EVRExpHeadDetectionRejectionReason::OutsideCone,
                   TEXT("OutsideCone"));
    AddReasonCount(
        EVRExpHeadDetectionRejectionReason::LineOfSightBlocked,
        TEXT("LineOfSight"));
    AddReasonCount(
        EVRExpHeadDetectionRejectionReason::NoDetectableComponent,
        TEXT("NoDetectable"));
    AddReasonCount(
        EVRExpHeadDetectionRejectionReason::PrimitiveModeRejected,
        TEXT("HeadPrimitiveMode"));
    AddReasonCount(EVRExpHeadDetectionRejectionReason::OwnerRejected,
                   TEXT("Owner"));

    FString NearestReason;
    switch (LastQueryHitRejectionReason)
    {
    case EVRExpHeadDetectionRejectionReason::ObjectTypeNotEnabled:
    {
        TArray<FString> EnabledObjectTypes;
        for (const TEnumAsByte<EObjectTypeQuery> ObjectType :
             DetectionObjectTypes)
        {
            const ECollisionChannel CollisionChannel =
                UEngineTypes::ConvertToCollisionChannel(ObjectType);
            if (CollisionChannel != ECC_MAX)
            {
                EnabledObjectTypes.AddUnique(
                    GetCollisionChannelDisplayName(CollisionChannel));
            }
        }

        const UPrimitiveComponent *HitPrimitive =
            LastQueryHitPrimitive.Get();
        const FString HitObjectType = IsValid(HitPrimitive)
                                          ? GetCollisionChannelDisplayName(
                                                HitPrimitive
                                                    ->GetCollisionObjectType())
                                          : TEXT("Unknown");
        const FString EnabledObjectTypesText =
            EnabledObjectTypes.Num() > 0
                ? FString::Join(EnabledObjectTypes, TEXT(", "))
                : TEXT("None");
        NearestReason = FString::Printf(
            TEXT("Nearest rejection: ObjectType %s is not enabled "
                 "(enabled: %s)"),
            *HitObjectType, *EnabledObjectTypesText);
        break;
    }

    case EVRExpHeadDetectionRejectionReason::OutsideCone:
        NearestReason =
            TEXT("Nearest rejection: outside the configured forward cone");
        break;

    case EVRExpHeadDetectionRejectionReason::LineOfSightBlocked:
        NearestReason =
            TEXT("Nearest rejection: cone line of sight is blocked");
        break;

    case EVRExpHeadDetectionRejectionReason::NoDetectableComponent:
        NearestReason = FString::Printf(
            TEXT("Nearest rejection: %s has no "
                 "UVRExpDetectableComponent"),
            *GetNameSafe(LastQueryHitActor.Get()));
        break;

    case EVRExpHeadDetectionRejectionReason::PrimitiveModeRejected:
        NearestReason = FString::Printf(
            TEXT("Nearest rejection: %s is rejected by HeadPrimitiveMode"),
            *GetNameSafe(LastQueryHitPrimitive.Get()));
        break;

    case EVRExpHeadDetectionRejectionReason::OwnerRejected:
        NearestReason =
            TEXT("Nearest rejection: invalid owner or detector owner");
        break;

    case EVRExpHeadDetectionRejectionReason::None:
    default:
        NearestReason =
            LastSelectedTargetCount > 0
                ? TEXT("Nearest raw hit is a valid target")
                : TEXT("No per-primitive rejection reason");
        break;
    }

    const FString RejectedCounts =
        CountParts.Num() > 0
            ? FString::Join(CountParts, TEXT(", "))
            : TEXT("None");
    return FString::Printf(TEXT("%s\nRejected totals: %s"),
                           *NearestReason, *RejectedCounts);
}

void UVRExpHeadDetectionComponent::DrawDetectionDebug(const FDetectionMap &Detections) const
{
    UWorld *World = GetWorld();
    if (!bDrawDebug || !World)
    {
        return;
    }

    const FVector Origin = GetComponentLocation();
    const FVector Forward = GetForwardVector().GetSafeNormal();
    const float DrawDuration = DetectionInterval > 0.0f ? DetectionInterval : 0.0f;
    const FColor DebugColor = GetDebugColor();

    switch (DetectionMode)
    {
    case EVRExpHeadDetectionMode::SphereOverlap:
        DrawDebugSphere(World, Origin, SphereRadius, 24, DebugColor, false, DrawDuration, 0, 1.5f);
        break;

    case EVRExpHeadDetectionMode::ForwardRay:
    {
        const FVector End = Origin + Forward * RayDistance;
        DrawDebugLine(World, Origin, End, DebugColor, false, DrawDuration, 0, 2.0f);
        for (const FVector &ImpactPoint : LastQueryImpactPoints)
        {
            DrawDebugSphere(World, ImpactPoint, 1.5f, 8, FColor::Orange,
                            false, DrawDuration, 0, 0.75f);
        }
        break;
    }

    case EVRExpHeadDetectionMode::ForwardCone:
        DrawDebugCone(World, Origin, Forward, ConeDistance,
                      FMath::DegreesToRadians(ConeHalfAngleDegrees),
                      FMath::DegreesToRadians(ConeHalfAngleDegrees), 24, DebugColor, false,
                      DrawDuration, 0, 1.5f);
        break;

    default:
        break;
    }

    const UEnum *DetectionModeEnum = StaticEnum<EVRExpHeadDetectionMode>();
    const UEnum *TargetModeEnum = StaticEnum<EVRExpHeadDetectionTargetMode>();
    const UEnum *DebugStateEnum = StaticEnum<EVRExpHeadDetectionDebugState>();
    const FString DetectionModeName =
        DetectionModeEnum
            ? DetectionModeEnum->GetDisplayNameTextByValue(static_cast<int64>(DetectionMode)).ToString()
            : TEXT("Unknown");
    const FString TargetModeName =
        TargetModeEnum
            ? TargetModeEnum->GetDisplayNameTextByValue(static_cast<int64>(TargetMode)).ToString()
            : TEXT("Unknown");
    const FString DebugStateName =
        DebugStateEnum
            ? DebugStateEnum
                  ->GetDisplayNameTextByValue(
                      static_cast<int64>(LastDebugState))
                  .ToString()
            : TEXT("Unknown");

    const FString SummaryText = FString::Printf(
        TEXT("[VR Head Detection] %s | %s\n"
             "State: %s [%s]\n"
             "Counts: Raw=%d | Valid=%d | Selected=%d\n"
             "Raw: %s / %s\n"
             "Selected: %s / %s\n%s"),
        *DetectionModeName, *TargetModeName, *DebugStateName,
        *GetDebugColorName(), LastQueryHitCount, LastValidTargetCount,
        LastSelectedTargetCount, *GetNameSafe(LastQueryHitActor.Get()),
        *GetNameSafe(LastQueryHitPrimitive.Get()),
        *GetNameSafe(LastSelectedActor.Get()),
        *GetNameSafe(LastSelectedPrimitive.Get()),
        *LastRejectionSummary);

    if (GEngine)
    {
        const float ScreenDuration =
            FMath::Max(0.25f, DetectionInterval * 2.5f);
        const float TextScale = FMath::Max(0.5f, DebugScreenTextScale);
        GEngine->AddOnScreenDebugMessage(
            GetDebugScreenMessageKey(), ScreenDuration, FColor::White,
            SummaryText, false, FVector2D(TextScale, TextScale));
    }

    if (!bDrawDebugWorldLabels)
    {
        return;
    }

    bool bRawPrimitiveIsSelected = false;
    for (const TPair<TWeakObjectPtr<UVRExpDetectableComponent>,
                     TWeakObjectPtr<UPrimitiveComponent>> &Pair :
         Detections)
    {
        if (Pair.Value.Get() == LastQueryHitPrimitive.Get())
        {
            bRawPrimitiveIsSelected = true;
            break;
        }
    }

    TSet<TWeakObjectPtr<UPrimitiveComponent>> LabeledPrimitives;
    if (IsValid(LastQueryHitActor.Get()) &&
        IsValid(LastQueryHitPrimitive.Get()))
    {
        const TCHAR *LabelPrefix =
            bRawPrimitiveIsSelected ? TEXT("Raw + Selected")
                                    : TEXT("Raw");
        const FString RawHitLabel = FString::Printf(
            TEXT("%s: %s / %s"), LabelPrefix,
            *GetNameSafe(LastQueryHitActor.Get()),
            *GetNameSafe(LastQueryHitPrimitive.Get()));
        DrawDebugString(
            World, LastQueryHitPoint + FVector(0.0f, 0.0f, 5.0f),
            RawHitLabel, nullptr,
            bRawPrimitiveIsSelected ? DebugColor : FColor::Orange,
            DrawDuration, false, 0.8f);
        LabeledPrimitives.Add(
            TWeakObjectPtr<UPrimitiveComponent>(
                LastQueryHitPrimitive.Get()));
    }

    for (const TPair<TWeakObjectPtr<UVRExpDetectableComponent>, TWeakObjectPtr<UPrimitiveComponent>> &Pair :
         Detections)
    {
        UVRExpDetectableComponent *DetectableComponent = Pair.Key.Get();
        UPrimitiveComponent *PrimitiveComponent = Pair.Value.Get();
        if (!IsValid(DetectableComponent) || !IsValid(PrimitiveComponent))
        {
            continue;
        }

        const TWeakObjectPtr<UPrimitiveComponent> PrimitiveKey(
            PrimitiveComponent);
        if (LabeledPrimitives.Contains(PrimitiveKey))
        {
            continue;
        }
        LabeledPrimitives.Add(PrimitiveKey);

        const FString SelectedLabel = FString::Printf(
            TEXT("Selected: %s / %s"), *GetNameSafe(DetectableComponent->GetOwner()),
            *GetNameSafe(PrimitiveComponent));
        DrawDebugString(World, PrimitiveComponent->Bounds.Origin + FVector(0.0f, 0.0f, 10.0f),
                        SelectedLabel, nullptr, DebugColor, DrawDuration, false, 0.9f);
    }
}

FColor UVRExpHeadDetectionComponent::GetDebugColor() const
{
    switch (LastDebugState)
    {
    case EVRExpHeadDetectionDebugState::NoQueryHits:
        return FColor::Red;

    case EVRExpHeadDetectionDebugState::QueryHitsRejected:
        return FColor::Orange;

    case EVRExpHeadDetectionDebugState::SingleTargetSelected:
        return FColor::Green;

    case EVRExpHeadDetectionDebugState::MultipleTargetsSelected:
        return FColor::Cyan;

    case EVRExpHeadDetectionDebugState::NotRunning:
    default:
        return FColor::Silver;
    }
}

FString UVRExpHeadDetectionComponent::GetDebugColorName() const
{
    switch (LastDebugState)
    {
    case EVRExpHeadDetectionDebugState::NoQueryHits:
        return TEXT("RED");

    case EVRExpHeadDetectionDebugState::QueryHitsRejected:
        return TEXT("ORANGE");

    case EVRExpHeadDetectionDebugState::SingleTargetSelected:
        return TEXT("GREEN");

    case EVRExpHeadDetectionDebugState::MultipleTargetsSelected:
        return TEXT("CYAN");

    case EVRExpHeadDetectionDebugState::NotRunning:
    default:
        return TEXT("SILVER");
    }
}

uint64 UVRExpHeadDetectionComponent::GetDebugScreenMessageKey() const
{
    constexpr uint32 DebugMessageKeyPrefix = 0x56000000U;
    constexpr uint32 UniqueIdMask = 0x00FFFFFFU;
    return static_cast<uint64>(
        DebugMessageKeyPrefix |
        (static_cast<uint32>(GetUniqueID()) & UniqueIdMask));
}

void UVRExpHeadDetectionComponent::ClearDebugScreenMessage() const
{
    if (GEngine)
    {
        GEngine->RemoveOnScreenDebugMessage(GetDebugScreenMessageKey());
    }
}

void UVRExpHeadDetectionComponent::ResetDebugState()
{
    LastDebugState = EVRExpHeadDetectionDebugState::NotRunning;
    LastQueryHitCount = 0;
    LastValidTargetCount = 0;
    LastSelectedTargetCount = 0;
    LastQueryHitActor = nullptr;
    LastQueryHitPrimitive = nullptr;
    LastQueryHitRejectionReason =
        EVRExpHeadDetectionRejectionReason::None;
    LastRejectionSummary = TEXT("None");
    LastSelectedActor = nullptr;
    LastSelectedPrimitive = nullptr;
    LastQueryImpactPoints.Reset();
    LastQueryPrimitiveDistances.Reset();
    LastPrimitiveRejectionReasons.Reset();
    LastQueryHitDistanceSquared = TNumericLimits<float>::Max();
    LastQueryHitPoint = FVector::ZeroVector;
}

void UVRExpHeadDetectionComponent::ApplyDetectionResults(FDetectionMap &&NewDetections)
{
    for (const TPair<TWeakObjectPtr<UVRExpDetectableComponent>, TWeakObjectPtr<UPrimitiveComponent>> &ExistingPair :
         CurrentDetections)
    {
        UVRExpDetectableComponent *DetectableComponent = ExistingPair.Key.Get();
        if (DetectableComponent && !NewDetections.Contains(ExistingPair.Key))
        {
            UPrimitiveComponent *PrimitiveComponent = ExistingPair.Value.Get();
            DetectableComponent->NotifyHeadDetectionEnd(this, PrimitiveComponent);
            OnDetectionEnded.Broadcast(DetectableComponent, PrimitiveComponent);
        }
    }

    for (const TPair<TWeakObjectPtr<UVRExpDetectableComponent>, TWeakObjectPtr<UPrimitiveComponent>> &NewPair :
         NewDetections)
    {
        UVRExpDetectableComponent *DetectableComponent = NewPair.Key.Get();
        if (DetectableComponent && !CurrentDetections.Contains(NewPair.Key))
        {
            UPrimitiveComponent *PrimitiveComponent = NewPair.Value.Get();
            DetectableComponent->NotifyHeadDetectionBegin(this, PrimitiveComponent);
            OnDetectionStarted.Broadcast(DetectableComponent, PrimitiveComponent);
        }
    }

    CurrentDetections = MoveTemp(NewDetections);
}

void UVRExpHeadDetectionComponent::ClearCurrentDetections()
{
    for (const TPair<TWeakObjectPtr<UVRExpDetectableComponent>, TWeakObjectPtr<UPrimitiveComponent>> &Pair :
         CurrentDetections)
    {
        if (UVRExpDetectableComponent *DetectableComponent = Pair.Key.Get())
        {
            UPrimitiveComponent *PrimitiveComponent = Pair.Value.Get();
            DetectableComponent->NotifyHeadDetectionEnd(this, PrimitiveComponent);
            OnDetectionEnded.Broadcast(DetectableComponent, PrimitiveComponent);
        }
    }

    CurrentDetections.Reset();
    ResetDebugState();
    ClearDebugScreenMessage();
}
