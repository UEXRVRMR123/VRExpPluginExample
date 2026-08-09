#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interaction/VRExpGrabbableMotionComponent.h"
#include "PreviewScene.h"

struct FVRExpGrabbableMotionTestAccessor
{
    static void StartNormalMotion(UVRExpGrabbableMotionComponent &Component) { Component.StartNormalMotion(); }

    static void StartReleaseMotionFromDrop(UVRExpGrabbableMotionComponent &Component)
    {
        Component.StartReleaseMotionFromDrop();
    }

    static void CaptureInitialReleaseAttachmentState(UVRExpGrabbableMotionComponent &Component)
    {
        Component.CaptureInitialReleaseAttachmentState();
    }

    static void HandleReleaseAttachmentAfterDrop(UVRExpGrabbableMotionComponent &Component,
                                                 EVRExpGrabbableReleaseMotionMode ReleasedMode)
    {
        Component.HandleReleaseAttachmentAfterDrop(ReleasedMode);
    }

    static void PrepareReleaseAttachmentBeforeMotion(UVRExpGrabbableMotionComponent &Component,
                                                     EVRExpGrabbableReleaseMotionMode ReleasedMode)
    {
        Component.PrepareReleaseAttachmentBeforeMotion(ReleasedMode);
    }

    static void FinalizeReleaseAttachmentAfterMotionStart(UVRExpGrabbableMotionComponent &Component)
    {
        Component.FinalizeReleaseAttachmentAfterMotionStart();
    }

    static void CompletePendingReleaseAttachment(UVRExpGrabbableMotionComponent &Component)
    {
        Component.CompletePendingReleaseAttachment();
    }

    static bool HasPendingReleaseAttachment(const UVRExpGrabbableMotionComponent &Component)
    {
        return Component.bHasPendingReleaseAttachment;
    }

    static void CancelPendingReleaseAttachment(UVRExpGrabbableMotionComponent &Component)
    {
        Component.CancelPendingReleaseAttachment();
    }

    static bool ShouldStartReleaseMotionFromGripEnd(bool bIsFinalGrip,
                                                    bool bWasSocketed,
                                                    bool bHadMovementAuthority)
    {
        return UVRExpGrabbableMotionComponent::ShouldStartReleaseMotionFromGripEnd(
            bIsFinalGrip, bWasSocketed, bHadMovementAuthority);
    }

    static bool IsMovingToSpline(const UVRExpGrabbableMotionComponent &Component)
    {
        return Component.bIsMovingToSpline;
    }

    static bool IsSplineApproachActive(const UVRExpGrabbableMotionComponent &Component)
    {
        return Component.SplineApproachPhase != UVRExpGrabbableMotionComponent::ESplineApproachPhase::None;
    }

    static float GetSplineApproachFallSpeed(const UVRExpGrabbableMotionComponent &Component)
    {
        return Component.SplineApproachFallSpeed;
    }

    static float GetCurrentSplineProgress(const UVRExpGrabbableMotionComponent &Component)
    {
        return Component.CurrentSplineProgress;
    }

    static float ResolveKinematicGravityAcceleration(const UVRExpGrabbableMotionComponent &Component)
    {
        return Component.ResolveKinematicGravityAcceleration();
    }

    static bool IsUsingParentRelativeMotionSpace(const UVRExpGrabbableMotionComponent &Component)
    {
        return Component.IsUsingParentRelativeMotionSpace();
    }
};

namespace
{
    struct FVRExpGrabbableMotionTestScene
    {
        explicit FVRExpGrabbableMotionTestScene(bool bCreatePhysicsScene = false)
            : PreviewScene(FPreviewScene::ConstructionValues()
                               .SetEditor(true)
                               .SetTransactional(false)
                               .AllowAudioPlayback(false)
                               .SetCreatePhysicsScene(bCreatePhysicsScene)
                               .ShouldSimulatePhysics(bCreatePhysicsScene))
        {
            World = PreviewScene.GetWorld();
            if (!World)
            {
                return;
            }

            MotionOwner = World->SpawnActor<AActor>();
            UpdatedComponent = NewObject<UBoxComponent>(MotionOwner, NAME_None, RF_Transient);
            UpdatedComponent->SetMobility(EComponentMobility::Movable);
            UpdatedComponent->InitBoxExtent(FVector(5.0f));
            if (bCreatePhysicsScene)
            {
                UpdatedComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }
            MotionOwner->SetRootComponent(UpdatedComponent);
            MotionOwner->AddInstanceComponent(UpdatedComponent);
            UpdatedComponent->RegisterComponentWithWorld(World);

            MotionComponent = NewObject<UVRExpGrabbableMotionComponent>(MotionOwner, NAME_None, RF_Transient);
            MotionOwner->AddInstanceComponent(MotionComponent);
            MotionComponent->RegisterComponentWithWorld(World);
            MotionComponent->SetUpdatedComponent(UpdatedComponent);

            SplineOwner = World->SpawnActor<AActor>();
            SplineRoot = NewObject<USceneComponent>(SplineOwner, NAME_None, RF_Transient);
            SplineRoot->SetMobility(EComponentMobility::Movable);
            SplineOwner->SetRootComponent(SplineRoot);
            SplineOwner->AddInstanceComponent(SplineRoot);
            SplineRoot->RegisterComponentWithWorld(World);

            SplineComponent = NewObject<USplineComponent>(SplineOwner, NAME_None, RF_Transient);
            SplineComponent->SetMobility(EComponentMobility::Movable);
            SplineComponent->SetupAttachment(SplineRoot);
            SplineOwner->AddInstanceComponent(SplineComponent);
            SplineComponent->RegisterComponentWithWorld(World);

            MotionComponent->bAutoStartNormalMotion = false;
            MotionComponent->bUseManualSplineReference = true;
            MotionComponent->bUseSplineActor = true;
            MotionComponent->SplineActor = SplineOwner;
            MotionComponent->SplineToFollow = SplineComponent;
            MotionComponent->SplineArrivalThreshold = 0.1f;
            MotionComponent->bSweepMovement = false;
            MotionComponent->bDisablePhysicsDuringKinematicMotion = true;
            SetSplinePoints(FVector(100.0f, 0.0f, 0.0f), FVector(200.0f, 0.0f, 0.0f));
        }

        bool IsReady() const
        {
            return World && MotionOwner && UpdatedComponent && MotionComponent && SplineOwner && SplineComponent;
        }

        void SetSplinePoints(const FVector &FirstPoint, const FVector &SecondPoint)
        {
            SplineComponent->ClearSplinePoints(false);
            SplineComponent->AddSplinePoint(FirstPoint, ESplineCoordinateSpace::World, false);
            SplineComponent->AddSplinePoint(SecondPoint, ESplineCoordinateSpace::World, false);
            SplineComponent->UpdateSpline();
        }

        void SetObjectTransform(const FVector &Location, const FQuat &Rotation = FQuat::Identity)
        {
            UpdatedComponent->SetWorldLocationAndRotation(Location, Rotation, false, nullptr,
                                                          ETeleportType::TeleportPhysics);
        }

        USceneComponent *CreateExternalParent()
        {
            InitialParentOwner = World ? World->SpawnActor<AActor>() : nullptr;
            if (!InitialParentOwner)
            {
                return nullptr;
            }

            InitialParentComponent = NewObject<USceneComponent>(InitialParentOwner, NAME_None, RF_Transient);
            InitialParentComponent->SetMobility(EComponentMobility::Movable);
            InitialParentOwner->SetRootComponent(InitialParentComponent);
            InitialParentOwner->AddInstanceComponent(InitialParentComponent);
            InitialParentComponent->RegisterComponentWithWorld(World);
            return InitialParentComponent;
        }

        bool CacheInitialParent(USceneComponent *ParentComponent,
                                const FTransform &InitialRelativeTransform,
                                FName SocketName = NAME_None)
        {
            if (!ParentComponent || !UpdatedComponent || !MotionComponent)
            {
                return false;
            }

            UpdatedComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
            if (!UpdatedComponent->AttachToComponent(ParentComponent,
                                                     FAttachmentTransformRules::KeepWorldTransform,
                                                     SocketName))
            {
                return false;
            }

            UpdatedComponent->SetRelativeTransform(InitialRelativeTransform, false, nullptr,
                                                   ETeleportType::TeleportPhysics);
            FVRExpGrabbableMotionTestAccessor::CaptureInitialReleaseAttachmentState(*MotionComponent);
            return true;
        }

        void DetachObjectKeepWorld()
        {
            UpdatedComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        }

        USplineComponent *CreateOwnerSpline()
        {
            USplineComponent *OwnerSpline = NewObject<USplineComponent>(MotionOwner, NAME_None, RF_Transient);
            OwnerSpline->SetMobility(EComponentMobility::Movable);
            OwnerSpline->SetupAttachment(UpdatedComponent);
            MotionOwner->AddInstanceComponent(OwnerSpline);
            OwnerSpline->RegisterComponentWithWorld(World);
            OwnerSpline->AddSplinePoint(FVector::ZeroVector, ESplineCoordinateSpace::Local, false);
            OwnerSpline->AddSplinePoint(FVector(100.0f, 0.0f, 0.0f), ESplineCoordinateSpace::Local, false);
            OwnerSpline->UpdateSpline();
            return OwnerSpline;
        }

        void ConfigureNormalKinematicGravity()
        {
            MotionComponent->NormalMotionMode = EVRExpGrabbableNormalMotionMode::FollowSpline;
            MotionComponent->bTeleportToSplineOnStart = false;
            MotionComponent->bUseKinematicGravityOnSplineStart = true;
            MotionComponent->KinematicGravitySource = EVRExpGrabbableKinematicGravitySource::CustomAcceleration;
            MotionComponent->CustomGravityAcceleration = 980.0f;
            MotionComponent->MaxFallSpeed = 4000.0f;
            MotionComponent->SplineSpeed = 200.0f;
        }

        void ConfigureReleaseKinematicGravity()
        {
            MotionComponent->NormalMotionMode = EVRExpGrabbableNormalMotionMode::FollowSpline;
            MotionComponent->ReleaseMotionMode = EVRExpGrabbableReleaseMotionMode::ReturnToSpline;
            MotionComponent->bTeleportToSplineOnStart = false;
            MotionComponent->bTeleportToSplineOnRelease = false;
            MotionComponent->bUseKinematicGravityOnSplineRelease = true;
            MotionComponent->KinematicGravitySource = EVRExpGrabbableKinematicGravitySource::CustomAcceleration;
            MotionComponent->CustomGravityAcceleration = 980.0f;
            MotionComponent->MaxFallSpeed = 4000.0f;
            MotionComponent->ReturnSpeed = 200.0f;
            MotionComponent->bSmoothReturn = false;
        }

        void Tick(float DeltaTime) { MotionComponent->TickComponent(DeltaTime, LEVELTICK_All, nullptr); }

        FPreviewScene PreviewScene;
        UWorld *World = nullptr;
        AActor *MotionOwner = nullptr;
        UBoxComponent *UpdatedComponent = nullptr;
        UVRExpGrabbableMotionComponent *MotionComponent = nullptr;
        AActor *SplineOwner = nullptr;
        USceneComponent *SplineRoot = nullptr;
        USplineComponent *SplineComponent = nullptr;
        AActor *InitialParentOwner = nullptr;
        USceneComponent *InitialParentComponent = nullptr;
    };
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVRExpGrabbableMotionKinematicGravityDefaultsTest,
                                 "VRExpansionExtensions.Motion.KinematicGravity.DefaultsAndVerticalFall",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpGrabbableMotionKinematicGravityDefaultsTest::RunTest(const FString &Parameters)
{
    FVRExpGrabbableMotionTestScene Scene;
    if (!TestTrue(TEXT("Motion test scene is valid"), Scene.IsReady()))
    {
        return false;
    }

    TestFalse(TEXT("Spline start kinematic gravity defaults off"),
              Scene.MotionComponent->bUseKinematicGravityOnSplineStart);
    TestFalse(TEXT("Spline release kinematic gravity defaults off"),
              Scene.MotionComponent->bUseKinematicGravityOnSplineRelease);
    TestEqual(TEXT("Vertical-then-approach is the default fall mode"), Scene.MotionComponent->SplineFallMode,
              EVRExpGrabbableSplineFallMode::VerticalThenApproach);
    TestEqual(TEXT("World gravity is the default source"), Scene.MotionComponent->KinematicGravitySource,
              EVRExpGrabbableKinematicGravitySource::WorldGravity);
    TestEqual(TEXT("Attached directions preserve world semantics by default"),
              Scene.MotionComponent->AttachedMotionDirectionMode,
              EVRExpGrabbableAttachedMotionDirectionMode::PreserveWorldDirections);
    TestEqual(TEXT("Attached FallWithGravity uses relative kinematic motion by default"),
              Scene.MotionComponent->AttachedFallWithGravityMode,
              EVRExpGrabbableAttachedFallWithGravityMode::RelativeKinematic);
    TestEqual(TEXT("Attached kinematic fall sweeps and stops by default"),
              Scene.MotionComponent->AttachedKinematicFallCollisionMode,
              EVRExpGrabbableAttachedKinematicFallCollisionMode::SweepAndStop);

    Scene.MotionComponent->WorldGravityScale = 0.5f;
    TestTrue(TEXT("World gravity source applies WorldGravityScale"),
             FMath::IsNearlyEqual(
                 FVRExpGrabbableMotionTestAccessor::ResolveKinematicGravityAcceleration(*Scene.MotionComponent),
                 FMath::Abs(Scene.MotionComponent->GetGravityZ()) * 0.5f, KINDA_SMALL_NUMBER));

    Scene.MotionComponent->NormalMotionMode = EVRExpGrabbableNormalMotionMode::FollowSpline;
    Scene.MotionComponent->bTeleportToSplineOnStart = false;
    Scene.MotionComponent->SplineSpeed = 200.0f;
    Scene.SetObjectTransform(FVector(0.0f, 0.0f, 100.0f));
    FVRExpGrabbableMotionTestAccessor::StartNormalMotion(*Scene.MotionComponent);
    Scene.Tick(0.1f);
    const FVector LegacyLocation = Scene.UpdatedComponent->GetComponentLocation();
    TestTrue(TEXT("Disabled kinematic gravity preserves direct three-dimensional approach"),
             LegacyLocation.X > 0.0f && LegacyLocation.Z < 100.0f);

    Scene.ConfigureNormalKinematicGravity();
    Scene.MotionComponent->SplineFallMode = EVRExpGrabbableSplineFallMode::VerticalThenApproach;
    Scene.SetObjectTransform(FVector(0.0f, 0.0f, 100.0f));
    FVRExpGrabbableMotionTestAccessor::StartNormalMotion(*Scene.MotionComponent);
    Scene.Tick(0.1f);

    const FVector FirstTickLocation = Scene.UpdatedComponent->GetComponentLocation();
    TestTrue(TEXT("Vertical fall keeps world XY fixed"),
             FVector2D(FirstTickLocation.X, FirstTickLocation.Y).Equals(FVector2D::ZeroVector, 0.01f));
    TestTrue(TEXT("Vertical fall decreases world Z"), FirstTickLocation.Z < 100.0f);
    TestFalse(TEXT("Kinematic fall does not enable physics simulation"), Scene.UpdatedComponent->IsSimulatingPhysics());

    for (int32 Step = 0; Step < 200 && FVRExpGrabbableMotionTestAccessor::IsMovingToSpline(*Scene.MotionComponent);
         ++Step)
    {
        Scene.Tick(0.05f);
    }

    TestFalse(TEXT("Vertical fall eventually reaches the spline"),
              FVRExpGrabbableMotionTestAccessor::IsMovingToSpline(*Scene.MotionComponent));
    TestTrue(TEXT("The component finishes at the captured spline point"),
             Scene.UpdatedComponent->GetComponentLocation().Equals(FVector(100.0f, 0.0f, 0.0f), 0.2f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVRExpGrabbableMotionKinematicGravityFallModesTest,
                                 "VRExpansionExtensions.Motion.KinematicGravity.FallModesAndLimits",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpGrabbableMotionKinematicGravityFallModesTest::RunTest(const FString &Parameters)
{
    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Approach-while-falling scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        Scene.ConfigureNormalKinematicGravity();
        Scene.MotionComponent->SplineFallMode = EVRExpGrabbableSplineFallMode::ApproachWhileFalling;
        Scene.SetObjectTransform(FVector(0.0f, 0.0f, 100.0f));
        FVRExpGrabbableMotionTestAccessor::StartNormalMotion(*Scene.MotionComponent);
        Scene.Tick(0.1f);

        const FVector Location = Scene.UpdatedComponent->GetComponentLocation();
        TestTrue(TEXT("Approach-while-falling changes world XY"), Location.X > 0.0f);
        TestTrue(TEXT("Approach-while-falling also decreases world Z"), Location.Z < 100.0f);
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Fall-speed limit scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        Scene.SetSplinePoints(FVector(100.0f, 0.0f, -10000.0f), FVector(200.0f, 0.0f, -10000.0f));
        Scene.ConfigureNormalKinematicGravity();
        Scene.MotionComponent->CustomGravityAcceleration = 10000.0f;
        Scene.MotionComponent->MaxFallSpeed = 25.0f;
        Scene.SetObjectTransform(FVector(0.0f, 0.0f, 1000.0f));
        FVRExpGrabbableMotionTestAccessor::StartNormalMotion(*Scene.MotionComponent);
        Scene.Tick(1.0f);

        TestTrue(
            TEXT("Fall speed is clamped to MaxFallSpeed"),
            FMath::IsNearlyEqual(FVRExpGrabbableMotionTestAccessor::GetSplineApproachFallSpeed(*Scene.MotionComponent),
                                 25.0f, KINDA_SMALL_NUMBER));
        TestTrue(TEXT("One-second fall does not exceed the configured speed limit"),
                 1000.0f - Scene.UpdatedComponent->GetComponentLocation().Z <= 25.1f);
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Zero-gravity fallback scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        Scene.ConfigureNormalKinematicGravity();
        Scene.MotionComponent->CustomGravityAcceleration = 0.0f;
        Scene.SetObjectTransform(FVector(0.0f, 0.0f, 100.0f));
        FVRExpGrabbableMotionTestAccessor::StartNormalMotion(*Scene.MotionComponent);
        Scene.Tick(0.1f);

        const FVector Location = Scene.UpdatedComponent->GetComponentLocation();
        TestTrue(TEXT("Zero effective gravity falls back to direct approach"),
                 Location.X > 0.0f && Location.Z < 100.0f);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVRExpGrabbableMotionKinematicGravityAboveTargetTest,
                                 "VRExpansionExtensions.Motion.KinematicGravity.AboveTargetAndOrientation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpGrabbableMotionKinematicGravityAboveTargetTest::RunTest(const FString &Parameters)
{
    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Direct-above-target scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        Scene.SetSplinePoints(FVector(100.0f, 0.0f, 100.0f), FVector(200.0f, 0.0f, 100.0f));
        Scene.ConfigureNormalKinematicGravity();
        Scene.MotionComponent->SplineAboveTargetMode = EVRExpGrabbableSplineAboveTargetMode::DirectApproach;
        Scene.SetObjectTransform(FVector::ZeroVector);
        FVRExpGrabbableMotionTestAccessor::StartNormalMotion(*Scene.MotionComponent);
        Scene.Tick(0.1f);

        const FVector Location = Scene.UpdatedComponent->GetComponentLocation();
        TestTrue(TEXT("Direct approach moves horizontally and upward together"),
                 Location.X > 0.0f && Location.Z > 0.0f);
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Vertical-rise scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        Scene.SetSplinePoints(FVector(100.0f, 0.0f, 100.0f), FVector(200.0f, 0.0f, 100.0f));
        Scene.ConfigureNormalKinematicGravity();
        Scene.MotionComponent->SplineAboveTargetMode = EVRExpGrabbableSplineAboveTargetMode::RiseVerticallyThenApproach;
        Scene.MotionComponent->bOrientDuringSplineHeightMotion = false;
        Scene.SetObjectTransform(FVector::ZeroVector);
        FVRExpGrabbableMotionTestAccessor::StartNormalMotion(*Scene.MotionComponent);
        Scene.Tick(0.1f);

        const FVector Location = Scene.UpdatedComponent->GetComponentLocation();
        TestTrue(TEXT("Vertical rise keeps world XY fixed"),
                 FVector2D(Location.X, Location.Y).Equals(FVector2D::ZeroVector, 0.01f));
        TestTrue(TEXT("Vertical rise increases world Z"), Location.Z > 0.0f);
        TestTrue(TEXT("Disabled height orientation preserves rotation"),
                 Scene.UpdatedComponent->GetComponentQuat().Equals(FQuat::Identity, KINDA_SMALL_NUMBER));
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Height-orientation scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        Scene.SetSplinePoints(FVector(100.0f, 0.0f, 100.0f), FVector(200.0f, 0.0f, 100.0f));
        Scene.ConfigureNormalKinematicGravity();
        Scene.MotionComponent->SplineAboveTargetMode = EVRExpGrabbableSplineAboveTargetMode::RiseVerticallyThenApproach;
        Scene.MotionComponent->bOrientDuringSplineHeightMotion = true;
        Scene.SetObjectTransform(FVector::ZeroVector);
        FVRExpGrabbableMotionTestAccessor::StartNormalMotion(*Scene.MotionComponent);
        Scene.Tick(0.1f);

        TestFalse(TEXT("Enabled height orientation faces the movement direction"),
                  Scene.UpdatedComponent->GetComponentQuat().Equals(FQuat::Identity, KINDA_SMALL_NUMBER));
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Global-orientation override scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        Scene.SetSplinePoints(FVector(100.0f, 0.0f, 100.0f), FVector(200.0f, 0.0f, 100.0f));
        Scene.ConfigureNormalKinematicGravity();
        Scene.MotionComponent->SplineAboveTargetMode = EVRExpGrabbableSplineAboveTargetMode::RiseVerticallyThenApproach;
        Scene.MotionComponent->bOrientDuringSplineHeightMotion = true;
        Scene.MotionComponent->bUpdateRotationDuringMotion = false;
        Scene.SetObjectTransform(FVector::ZeroVector);
        FVRExpGrabbableMotionTestAccessor::StartNormalMotion(*Scene.MotionComponent);
        Scene.Tick(0.1f);

        TestTrue(TEXT("bUpdateRotationDuringMotion remains the master orientation switch"),
                 Scene.UpdatedComponent->GetComponentQuat().Equals(FQuat::Identity, KINDA_SMALL_NUMBER));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVRExpGrabbableMotionKinematicGravityReleaseTest,
                                 "VRExpansionExtensions.Motion.KinematicGravity.ReleaseAndTeleportPrecedence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpGrabbableMotionKinematicGravityReleaseTest::RunTest(const FString &Parameters)
{
    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Release-return scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        Scene.ConfigureReleaseKinematicGravity();
        Scene.SetObjectTransform(FVector(0.0f, 0.0f, 100.0f));
        FVRExpGrabbableMotionTestAccessor::StartReleaseMotionFromDrop(*Scene.MotionComponent);
        TestEqual(TEXT("Release begins in Releasing state"), Scene.MotionComponent->GetMotionState(),
                  EVRExpGrabbableMotionState::Releasing);

        for (int32 Step = 0;
             Step < 200 && Scene.MotionComponent->GetMotionState() == EVRExpGrabbableMotionState::Releasing; ++Step)
        {
            Scene.Tick(0.05f);
        }

        TestEqual(TEXT("Release return resumes normal spline motion"), Scene.MotionComponent->GetMotionState(),
                  EVRExpGrabbableMotionState::NormalMotion);
        TestTrue(TEXT("Release return finishes at the captured point"),
                 Scene.UpdatedComponent->GetComponentLocation().Equals(FVector(100.0f, 0.0f, 0.0f), 0.2f));
        TestTrue(TEXT("Release return stores the captured spline progress"),
                 FMath::IsNearlyZero(
                     FVRExpGrabbableMotionTestAccessor::GetCurrentSplineProgress(*Scene.MotionComponent), 0.1f));
        TestFalse(TEXT("Release return keeps physics simulation disabled"),
                  Scene.UpdatedComponent->IsSimulatingPhysics());
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Release-teleport scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        Scene.ConfigureReleaseKinematicGravity();
        Scene.MotionComponent->bTeleportToSplineOnRelease = true;
        Scene.SetObjectTransform(FVector(0.0f, 0.0f, 100.0f));
        FVRExpGrabbableMotionTestAccessor::StartReleaseMotionFromDrop(*Scene.MotionComponent);
        TestFalse(TEXT("Release teleport bypasses the staged approach"),
                  FVRExpGrabbableMotionTestAccessor::IsSplineApproachActive(*Scene.MotionComponent));
        Scene.Tick(0.016f);
        TestTrue(TEXT("Release teleport reaches the spline in one tick"),
                 Scene.UpdatedComponent->GetComponentLocation().Equals(FVector(100.0f, 0.0f, 0.0f), 0.1f));
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Start-teleport scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        Scene.ConfigureNormalKinematicGravity();
        Scene.MotionComponent->bTeleportToSplineOnStart = true;
        Scene.SetObjectTransform(FVector(0.0f, 0.0f, 100.0f));
        FVRExpGrabbableMotionTestAccessor::StartNormalMotion(*Scene.MotionComponent);
        TestFalse(TEXT("Start teleport bypasses the staged approach"),
                  FVRExpGrabbableMotionTestAccessor::IsSplineApproachActive(*Scene.MotionComponent));
        TestTrue(TEXT("Start teleport immediately reaches the spline start"),
                 Scene.UpdatedComponent->GetComponentLocation().Equals(FVector(100.0f, 0.0f, 0.0f), 0.1f));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVRExpGrabbableMotionReleaseAttachmentDefaultsTest,
                                 "VRExpansionExtensions.Motion.ReleaseAttachment.DefaultsAndLegacyOptIn",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpGrabbableMotionReleaseAttachmentDefaultsTest::RunTest(const FString &Parameters)
{
    FVRExpGrabbableMotionTestScene Scene;
    if (!TestTrue(TEXT("Release-attachment defaults scene is valid"), Scene.IsReady()))
    {
        return false;
    }

    TestFalse(TEXT("Initial-parent reattachment defaults off"),
              Scene.MotionComponent->bReattachOwnerToInitialParentComponentOnRelease);
    TestFalse(TEXT("Spline attachment defaults off"),
              Scene.MotionComponent->bAttachOwnerToSplineOnRelease);

    const auto TestRule = [this](const TCHAR *Label,
                                 const FVRExpGrabbableReleaseAttachmentModeRule &Rule,
                                 bool bExpectedEnabled,
                                 EVRExpGrabbableReleaseAttachmentTiming ExpectedTiming,
                                 EVRExpGrabbableAttachmentPhysicsPolicy ExpectedPhysicsPolicy)
    {
        TestEqual(FString::Printf(TEXT("%s enabled"), Label), Rule.bEnableAttachment, bExpectedEnabled);
        TestEqual(FString::Printf(TEXT("%s timing"), Label), Rule.Timing, ExpectedTiming);
        TestEqual(FString::Printf(TEXT("%s initial-parent transform"), Label), Rule.InitialParentTransformRule,
                  EVRExpGrabbableInitialParentTransformRule::KeepWorld);
        TestEqual(FString::Printf(TEXT("%s spline transform"), Label), Rule.SplineTransformRule,
                  EVRExpGrabbableSplineAttachmentTransformRule::KeepWorld);
        TestEqual(FString::Printf(TEXT("%s physics policy"), Label), Rule.PhysicsPolicy,
                  ExpectedPhysicsPolicy);
    };

    const FVRExpGrabbableReleaseAttachmentRules &Rules = Scene.MotionComponent->ReleaseAttachmentRules;
    TestRule(TEXT("None"), Rules.NoneRule, true, EVRExpGrabbableReleaseAttachmentTiming::OnRelease,
             EVRExpGrabbableAttachmentPhysicsPolicy::DisablePhysicsAndAttach);
    TestRule(TEXT("ContinueMotion"), Rules.ContinueMotionRule, true,
             EVRExpGrabbableReleaseAttachmentTiming::OnRelease,
             EVRExpGrabbableAttachmentPhysicsPolicy::DisablePhysicsAndAttach);
    TestRule(TEXT("ReturnToStart"), Rules.ReturnToStartRule, true,
             EVRExpGrabbableReleaseAttachmentTiming::OnReleaseMotionCompleted,
             EVRExpGrabbableAttachmentPhysicsPolicy::DisablePhysicsAndAttach);
    TestRule(TEXT("ReturnToOrigin"), Rules.ReturnToOriginRule, true,
             EVRExpGrabbableReleaseAttachmentTiming::OnReleaseMotionCompleted,
             EVRExpGrabbableAttachmentPhysicsPolicy::DisablePhysicsAndAttach);
    TestRule(TEXT("ReturnToSpline"), Rules.ReturnToSplineRule, true,
             EVRExpGrabbableReleaseAttachmentTiming::OnReleaseMotionCompleted,
             EVRExpGrabbableAttachmentPhysicsPolicy::DisablePhysicsAndAttach);
    TestRule(TEXT("FlyToTarget"), Rules.FlyToTargetRule, true,
             EVRExpGrabbableReleaseAttachmentTiming::OnReleaseMotionCompleted,
             EVRExpGrabbableAttachmentPhysicsPolicy::DisablePhysicsAndAttach);
    TestRule(TEXT("FallWithGravity"), Rules.FallWithGravityRule, false,
             EVRExpGrabbableReleaseAttachmentTiming::OnRelease,
             EVRExpGrabbableAttachmentPhysicsPolicy::PreservePhysicsAndSkipIfSimulating);

    USceneComponent *InitialParent = Scene.CreateExternalParent();
    TestTrue(TEXT("Legacy-opt-in parent was created"), InitialParent != nullptr);
    TestTrue(TEXT("Initial parent was cached"),
             Scene.CacheInitialParent(InitialParent, FTransform(FVector(20.0f, 0.0f, 0.0f))));
    Scene.DetachObjectKeepWorld();
    FVRExpGrabbableMotionTestAccessor::HandleReleaseAttachmentAfterDrop(
        *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::None);
    TestTrue(TEXT("Both disabled switches preserve the detached legacy behavior"),
             Scene.UpdatedComponent->GetAttachParent() == nullptr);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVRExpGrabbableMotionInitialParentAttachmentTest,
                                 "VRExpansionExtensions.Motion.ReleaseAttachment.InitialParentAndSocket",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpGrabbableMotionInitialParentAttachmentTest::RunTest(const FString &Parameters)
{
    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Keep-world parent scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        USceneComponent *InitialParent = Scene.CreateExternalParent();
        const FName InitialSocket(TEXT("InitialSocket"));
        const FTransform InitialRelative(FRotator(5.0f, 25.0f, 0.0f), FVector(30.0f, 10.0f, 5.0f),
                                         FVector(1.2f, 0.8f, 1.1f));
        TestTrue(TEXT("Exact parent and socket were cached"),
                 Scene.CacheInitialParent(InitialParent, InitialRelative, InitialSocket));
        Scene.DetachObjectKeepWorld();
        Scene.SetObjectTransform(FVector(450.0f, 80.0f, 35.0f), FRotator(0.0f, 70.0f, 0.0f).Quaternion());
        const FTransform ReleasedWorldTransform = Scene.UpdatedComponent->GetComponentTransform();

        Scene.MotionComponent->bReattachOwnerToInitialParentComponentOnRelease = true;
        Scene.MotionComponent->ReleaseAttachmentRules.NoneRule.InitialParentTransformRule =
            EVRExpGrabbableInitialParentTransformRule::KeepWorld;
        FVRExpGrabbableMotionTestAccessor::HandleReleaseAttachmentAfterDrop(
            *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::None);

        TestTrue(TEXT("KeepWorld restores the exact initial parent component"),
                 Scene.UpdatedComponent->GetAttachParent() == InitialParent);
        TestEqual(TEXT("KeepWorld restores the exact initial socket"), Scene.UpdatedComponent->GetAttachSocketName(),
                  InitialSocket);
        TestTrue(TEXT("KeepWorld preserves the released world transform"),
                 Scene.UpdatedComponent->GetComponentTransform().Equals(ReleasedWorldTransform, 0.01f));
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Restore-relative parent scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        USceneComponent *InitialParent = Scene.CreateExternalParent();
        const FName InitialSocket(TEXT("RestoreSocket"));
        const FTransform InitialRelative(FRotator(12.0f, -35.0f, 4.0f), FVector(12.0f, 34.0f, 56.0f),
                                         FVector(0.9f, 1.1f, 1.25f));
        TestTrue(TEXT("Restore-relative parent was cached"),
                 Scene.CacheInitialParent(InitialParent, InitialRelative, InitialSocket));
        Scene.DetachObjectKeepWorld();
        Scene.SetObjectTransform(FVector(-300.0f, 90.0f, 125.0f), FRotator(10.0f, 110.0f, 5.0f).Quaternion());

        Scene.MotionComponent->bReattachOwnerToInitialParentComponentOnRelease = true;
        Scene.MotionComponent->ReleaseAttachmentRules.NoneRule.InitialParentTransformRule =
            EVRExpGrabbableInitialParentTransformRule::RestoreInitialRelative;
        FVRExpGrabbableMotionTestAccessor::HandleReleaseAttachmentAfterDrop(
            *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::None);

        TestTrue(TEXT("RestoreInitialRelative restores the exact initial parent"),
                 Scene.UpdatedComponent->GetAttachParent() == InitialParent);
        TestEqual(TEXT("RestoreInitialRelative restores the socket"), Scene.UpdatedComponent->GetAttachSocketName(),
                  InitialSocket);
        TestTrue(TEXT("RestoreInitialRelative restores the cached full relative transform"),
                 Scene.UpdatedComponent->GetRelativeTransform().Equals(InitialRelative, 0.01f));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVRExpGrabbableMotionSplineAttachmentTest,
                                 "VRExpansionExtensions.Motion.ReleaseAttachment.SplineRulesAndFallback",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpGrabbableMotionSplineAttachmentTest::RunTest(const FString &Parameters)
{
    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Spline-priority scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        USceneComponent *InitialParent = Scene.CreateExternalParent();
        TestTrue(TEXT("Spline-priority initial parent was cached"),
                 Scene.CacheInitialParent(InitialParent, FTransform(FVector(25.0f, 0.0f, 0.0f))));
        Scene.DetachObjectKeepWorld();
        Scene.SetObjectTransform(FVector(350.0f, 45.0f, 20.0f), FRotator(0.0f, 35.0f, 0.0f).Quaternion());
        const FTransform ReleasedWorldTransform = Scene.UpdatedComponent->GetComponentTransform();

        Scene.MotionComponent->bAttachOwnerToSplineOnRelease = true;
        Scene.MotionComponent->bReattachOwnerToInitialParentComponentOnRelease = true;
        Scene.MotionComponent->ReleaseAttachmentRules.NoneRule.SplineTransformRule =
            EVRExpGrabbableSplineAttachmentTransformRule::KeepWorld;
        FVRExpGrabbableMotionTestAccessor::HandleReleaseAttachmentAfterDrop(
            *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::None);

        TestTrue(TEXT("A valid external SplineToFollow has priority over the initial parent"),
                 Scene.UpdatedComponent->GetAttachParent() == Scene.SplineComponent);
        TestTrue(TEXT("Spline KeepWorld preserves the released world transform"),
                 Scene.UpdatedComponent->GetComponentTransform().Equals(ReleasedWorldTransform, 0.01f));
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Spline-relative scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        USceneComponent *InitialParent = Scene.CreateExternalParent();
        TestTrue(TEXT("Spline-relative state was cached"),
                 Scene.CacheInitialParent(InitialParent,
                                          FTransform(FRotator(0.0f, 20.0f, 0.0f),
                                                     FVector(40.0f, 15.0f, 10.0f),
                                                     FVector(1.1f, 0.9f, 1.2f))));
        const FTransform ExpectedSplineRelative =
            Scene.UpdatedComponent->GetComponentTransform().GetRelativeTransform(
                Scene.SplineComponent->GetComponentTransform());
        Scene.DetachObjectKeepWorld();
        Scene.SetObjectTransform(FVector(-250.0f, 75.0f, 90.0f), FRotator(5.0f, 85.0f, 0.0f).Quaternion());

        Scene.MotionComponent->bAttachOwnerToSplineOnRelease = true;
        Scene.MotionComponent->ReleaseAttachmentRules.NoneRule.SplineTransformRule =
            EVRExpGrabbableSplineAttachmentTransformRule::RestoreInitialRelativeToSpline;
        FVRExpGrabbableMotionTestAccessor::HandleReleaseAttachmentAfterDrop(
            *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::None);

        TestTrue(TEXT("RestoreInitialRelativeToSpline attaches to the configured spline"),
                 Scene.UpdatedComponent->GetAttachParent() == Scene.SplineComponent);
        TestTrue(TEXT("RestoreInitialRelativeToSpline restores the cached relative transform"),
                 Scene.UpdatedComponent->GetRelativeTransform().Equals(ExpectedSplineRelative, 0.01f));
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Closest-spline-point scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        FVRExpGrabbableMotionTestAccessor::CaptureInitialReleaseAttachmentState(*Scene.MotionComponent);
        Scene.SetObjectTransform(FVector(160.0f, 40.0f, 25.0f), FRotator(0.0f, 15.0f, 0.0f).Quaternion());
        const FVector ReleasedScale(1.25f, 0.85f, 1.1f);
        Scene.UpdatedComponent->SetWorldScale3D(ReleasedScale);
        Scene.MotionComponent->bAttachOwnerToSplineOnRelease = true;
        Scene.MotionComponent->bReverseDirection = true;
        Scene.MotionComponent->ReleaseAttachmentRules.NoneRule.SplineTransformRule =
            EVRExpGrabbableSplineAttachmentTransformRule::SnapToClosestSplinePoint;
        FVRExpGrabbableMotionTestAccessor::HandleReleaseAttachmentAfterDrop(
            *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::None);

        const FVector SnappedLocation = Scene.UpdatedComponent->GetComponentLocation();
        TestTrue(TEXT("SnapToClosestSplinePoint attaches to the configured spline"),
                 Scene.UpdatedComponent->GetAttachParent() == Scene.SplineComponent);
        TestTrue(TEXT("SnapToClosestSplinePoint reaches the straight spline"),
                 SnappedLocation.X >= 99.9f && SnappedLocation.X <= 200.1f &&
                     FMath::IsNearlyZero(SnappedLocation.Y, 0.01f) &&
                     FMath::IsNearlyZero(SnappedLocation.Z, 0.01f));
        TestTrue(TEXT("SnapToClosestSplinePoint preserves current world scale"),
                 Scene.UpdatedComponent->GetComponentScale().Equals(ReleasedScale, 0.01f));
        TestTrue(TEXT("Reverse spline attachment faces the reverse spline direction"),
                 FVector::DotProduct(Scene.UpdatedComponent->GetForwardVector(), -FVector::ForwardVector) > 0.99f);
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Spline-fallback scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        TestTrue(TEXT("Initial spline parent was cached"),
                 Scene.CacheInitialParent(Scene.SplineComponent, FTransform(FVector(10.0f, 0.0f, 0.0f))));
        Scene.DetachObjectKeepWorld();
        USplineComponent *OwnerSpline = Scene.CreateOwnerSpline();
        Scene.MotionComponent->SplineToFollow = OwnerSpline;
        Scene.MotionComponent->bAttachOwnerToSplineOnRelease = true;
        FVRExpGrabbableMotionTestAccessor::HandleReleaseAttachmentAfterDrop(
            *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::None);

        TestTrue(TEXT("An invalid same-owner spline falls back to the initial spline parent"),
                 Scene.UpdatedComponent->GetAttachParent() == Scene.SplineComponent);
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Same-owner rejection scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        USplineComponent *OwnerSpline = Scene.CreateOwnerSpline();
        Scene.MotionComponent->SplineToFollow = OwnerSpline;
        FVRExpGrabbableMotionTestAccessor::CaptureInitialReleaseAttachmentState(*Scene.MotionComponent);
        Scene.MotionComponent->bAttachOwnerToSplineOnRelease = true;
        FVRExpGrabbableMotionTestAccessor::HandleReleaseAttachmentAfterDrop(
            *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::None);
        TestTrue(TEXT("A same-owner spline cannot create a circular owner hierarchy"),
                 Scene.UpdatedComponent->GetAttachParent() == nullptr);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVRExpGrabbableMotionReleaseAttachmentTimingTest,
                                 "VRExpansionExtensions.Motion.ReleaseAttachment.TimingSnapshotAndCancellation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpGrabbableMotionReleaseAttachmentTimingTest::RunTest(const FString &Parameters)
{
    FVRExpGrabbableMotionTestScene Scene;
    if (!TestTrue(TEXT("Release-attachment timing scene is valid"), Scene.IsReady()))
    {
        return false;
    }

    USceneComponent *InitialParent = Scene.CreateExternalParent();
    TestTrue(TEXT("Timing test initial parent was cached"),
             Scene.CacheInitialParent(InitialParent, FTransform(FVector(15.0f, 0.0f, 0.0f))));
    Scene.DetachObjectKeepWorld();
    Scene.MotionComponent->bReattachOwnerToInitialParentComponentOnRelease = true;
    Scene.MotionComponent->ReleaseMotionMode = EVRExpGrabbableReleaseMotionMode::ReturnToStart;

    FVRExpGrabbableMotionTestAccessor::StartReleaseMotionFromDrop(*Scene.MotionComponent);
    FVRExpGrabbableMotionTestAccessor::HandleReleaseAttachmentAfterDrop(
        *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::ReturnToStart);
    TestTrue(TEXT("ReturnToStart stores an OnReleaseMotionCompleted attachment"),
             FVRExpGrabbableMotionTestAccessor::HasPendingReleaseAttachment(*Scene.MotionComponent));
    TestTrue(TEXT("A deferred rule does not attach at release time"),
             Scene.UpdatedComponent->GetAttachParent() == nullptr);

    Scene.MotionComponent->bReattachOwnerToInitialParentComponentOnRelease = false;
    Scene.MotionComponent->ReleaseAttachmentRules.ReturnToStartRule.bEnableAttachment = false;
    FVRExpGrabbableMotionTestAccessor::CompletePendingReleaseAttachment(*Scene.MotionComponent);
    TestTrue(TEXT("The release-time rule and target switches are snapshotted"),
             Scene.UpdatedComponent->GetAttachParent() == InitialParent);

    Scene.DetachObjectKeepWorld();
    Scene.MotionComponent->bReattachOwnerToInitialParentComponentOnRelease = true;
    Scene.MotionComponent->ReleaseAttachmentRules.ReturnToStartRule.bEnableAttachment = true;
    FVRExpGrabbableMotionTestAccessor::StartReleaseMotionFromDrop(*Scene.MotionComponent);
    FVRExpGrabbableMotionTestAccessor::HandleReleaseAttachmentAfterDrop(
        *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::ReturnToStart);
    TestTrue(TEXT("A second deferred attachment was stored"),
             FVRExpGrabbableMotionTestAccessor::HasPendingReleaseAttachment(*Scene.MotionComponent));
    Scene.MotionComponent->StopMotion();
    FVRExpGrabbableMotionTestAccessor::CompletePendingReleaseAttachment(*Scene.MotionComponent);
    TestTrue(TEXT("StopMotion cancels the pending release attachment"),
             Scene.UpdatedComponent->GetAttachParent() == nullptr);

    FVRExpGrabbableMotionTestAccessor::StartReleaseMotionFromDrop(*Scene.MotionComponent);
    FVRExpGrabbableMotionTestAccessor::HandleReleaseAttachmentAfterDrop(
        *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::ReturnToStart);
    TestTrue(TEXT("A third deferred attachment was stored"),
             FVRExpGrabbableMotionTestAccessor::HasPendingReleaseAttachment(*Scene.MotionComponent));
    FVRExpGrabbableMotionTestAccessor::CancelPendingReleaseAttachment(*Scene.MotionComponent);
    TestFalse(TEXT("The grip-begin cancellation operation clears the pending release attachment"),
              FVRExpGrabbableMotionTestAccessor::HasPendingReleaseAttachment(*Scene.MotionComponent));

    Scene.MotionComponent->ReleaseAttachmentRules.FallWithGravityRule.bEnableAttachment = false;
    FVRExpGrabbableMotionTestAccessor::HandleReleaseAttachmentAfterDrop(
        *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::FallWithGravity);
    TestTrue(TEXT("A disabled per-mode rule does not attach"),
             Scene.UpdatedComponent->GetAttachParent() == nullptr);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVRExpGrabbableMotionReleaseAttachmentTerminalAndPhysicsTest,
                                 "VRExpansionExtensions.Motion.ReleaseAttachment.TerminalPathsAndPhysicsPolicy",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpGrabbableMotionReleaseAttachmentTerminalAndPhysicsTest::RunTest(const FString &Parameters)
{
    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Invalid release target scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        USceneComponent *InitialParent = Scene.CreateExternalParent();
        TestTrue(TEXT("Invalid-target initial parent was cached"),
                 Scene.CacheInitialParent(InitialParent, FTransform(FVector(10.0f, 0.0f, 0.0f))));
        Scene.DetachObjectKeepWorld();
        Scene.MotionComponent->bReattachOwnerToInitialParentComponentOnRelease = true;
        Scene.MotionComponent->ReleaseMotionMode = EVRExpGrabbableReleaseMotionMode::FlyToTarget;
        Scene.MotionComponent->FlyToTargetComponent = Scene.SplineRoot;
        FVRExpGrabbableMotionTestAccessor::StartReleaseMotionFromDrop(*Scene.MotionComponent);
        FVRExpGrabbableMotionTestAccessor::HandleReleaseAttachmentAfterDrop(
            *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::FlyToTarget);
        TestTrue(TEXT("FlyToTarget stored a deferred attachment"),
                 FVRExpGrabbableMotionTestAccessor::HasPendingReleaseAttachment(*Scene.MotionComponent));

        Scene.MotionComponent->FlyToTargetComponent = nullptr;
        Scene.Tick(0.016f);
        TestTrue(TEXT("An invalidated release target completes attachment at the current pose"),
                 Scene.UpdatedComponent->GetAttachParent() == InitialParent);
        TestEqual(TEXT("An invalidated release target ends release motion"), Scene.MotionComponent->GetMotionState(),
                  EVRExpGrabbableMotionState::Idle);
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Destroyed parent scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        USceneComponent *InitialParent = Scene.CreateExternalParent();
        TestTrue(TEXT("Destroyable initial parent was cached"),
                 Scene.CacheInitialParent(InitialParent, FTransform(FVector(10.0f, 0.0f, 0.0f))));
        Scene.DetachObjectKeepWorld();
        InitialParent->DestroyComponent();
        Scene.MotionComponent->bReattachOwnerToInitialParentComponentOnRelease = true;
        FVRExpGrabbableMotionTestAccessor::HandleReleaseAttachmentAfterDrop(
            *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::None);
        TestTrue(TEXT("A destroyed cached parent fails safely"),
                 Scene.UpdatedComponent->GetAttachParent() == nullptr);
    }

    {
        FVRExpGrabbableMotionTestScene Scene(true);
        if (!TestTrue(TEXT("Physics-policy scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        USceneComponent *InitialParent = Scene.CreateExternalParent();
        TestTrue(TEXT("Physics-policy initial parent was cached"),
                 Scene.CacheInitialParent(InitialParent, FTransform(FVector(10.0f, 0.0f, 0.0f))));
        Scene.DetachObjectKeepWorld();
        Scene.UpdatedComponent->SetSimulatePhysics(true);
        TestTrue(TEXT("Physics simulation was enabled for the policy test"),
                 Scene.UpdatedComponent->IsSimulatingPhysics());
        Scene.MotionComponent->bReattachOwnerToInitialParentComponentOnRelease = true;
        Scene.MotionComponent->ReleaseAttachmentRules.NoneRule.PhysicsPolicy =
            EVRExpGrabbableAttachmentPhysicsPolicy::PreservePhysicsAndSkipIfSimulating;
        FVRExpGrabbableMotionTestAccessor::HandleReleaseAttachmentAfterDrop(
            *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::None);
        TestTrue(TEXT("PreservePhysicsAndSkipIfSimulating keeps simulation enabled"),
                 Scene.UpdatedComponent->IsSimulatingPhysics());
        TestTrue(TEXT("PreservePhysicsAndSkipIfSimulating skips attachment"),
                 Scene.UpdatedComponent->GetAttachParent() == nullptr);

        Scene.MotionComponent->ReleaseAttachmentRules.NoneRule.PhysicsPolicy =
            EVRExpGrabbableAttachmentPhysicsPolicy::DisablePhysicsAndAttach;
        FVRExpGrabbableMotionTestAccessor::HandleReleaseAttachmentAfterDrop(
            *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::None);
        TestFalse(TEXT("DisablePhysicsAndAttach leaves successful attachments non-simulating"),
                  Scene.UpdatedComponent->IsSimulatingPhysics());
        TestTrue(TEXT("DisablePhysicsAndAttach attaches to the cached parent"),
                 Scene.UpdatedComponent->GetAttachParent() == InitialParent);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVRExpGrabbableMotionReleaseAttachmentGripGatesTest,
                                 "VRExpansionExtensions.Motion.ReleaseAttachment.FinalGripSocketAndAuthorityGates",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpGrabbableMotionReleaseAttachmentGripGatesTest::RunTest(const FString &Parameters)
{
    TestFalse(TEXT("A non-final grip release does not start release motion"),
              FVRExpGrabbableMotionTestAccessor::ShouldStartReleaseMotionFromGripEnd(
                  false, false, true));
    TestFalse(TEXT("A socketed final release preserves the socket result"),
              FVRExpGrabbableMotionTestAccessor::ShouldStartReleaseMotionFromGripEnd(
                  true, true, true));
    TestFalse(TEXT("A final release without movement authority does not start release motion"),
              FVRExpGrabbableMotionTestAccessor::ShouldStartReleaseMotionFromGripEnd(
                  true, false, false));
    TestTrue(TEXT("A final non-socketed authoritative release starts release motion"),
             FVRExpGrabbableMotionTestAccessor::ShouldStartReleaseMotionFromGripEnd(
                 true, false, true));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVRExpGrabbableMotionAttachedSplineSpaceTest,
                                 "VRExpansionExtensions.Motion.RelativeSpace.AttachedSplineAndParentSwitch",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpGrabbableMotionAttachedSplineSpaceTest::RunTest(const FString &Parameters)
{
    FVRExpGrabbableMotionTestScene Scene;
    if (!TestTrue(TEXT("Attached spline scene is valid"), Scene.IsReady()))
    {
        return false;
    }

    USceneComponent *ParentA = Scene.CreateExternalParent();
    if (!TestNotNull(TEXT("First moving parent was created"), ParentA))
    {
        return false;
    }
    TestTrue(TEXT("Motion object was attached to the first parent"),
             Scene.CacheInitialParent(ParentA, FTransform::Identity));
    TestTrue(TEXT("Spline hierarchy shares the first moving parent"),
             Scene.SplineRoot->AttachToComponent(ParentA, FAttachmentTransformRules::KeepWorldTransform));

    Scene.MotionComponent->NormalMotionMode = EVRExpGrabbableNormalMotionMode::FollowSpline;
    Scene.MotionComponent->bTeleportToSplineOnStart = true;
    Scene.MotionComponent->SplineSpeed = 40.0f;
    Scene.MotionComponent->bLoopSpline = true;
    FVRExpGrabbableMotionTestAccessor::StartNormalMotion(*Scene.MotionComponent);

    ParentA->SetWorldLocationAndRotation(FVector(5000.0f, -1200.0f, 300.0f),
                                         FRotator(15.0f, 90.0f, 5.0f).Quaternion(), false, nullptr,
                                         ETeleportType::TeleportPhysics);
    Scene.Tick(0.25f);

    const float FirstProgress =
        FVRExpGrabbableMotionTestAccessor::GetCurrentSplineProgress(*Scene.MotionComponent);
    const FVector FirstExpectedWorldLocation = Scene.SplineComponent->GetLocationAtDistanceAlongSpline(
        FirstProgress, ESplineCoordinateSpace::World);
    const FQuat FirstExpectedWorldRotation = Scene.SplineComponent->GetQuaternionAtDistanceAlongSpline(
        FirstProgress, ESplineCoordinateSpace::World);
    TestTrue(TEXT("Fast parent translation and rotation are inherited while following the spline"),
             Scene.UpdatedComponent->GetComponentLocation().Equals(FirstExpectedWorldLocation, 0.1f));
    TestTrue(TEXT("Spline rotation is converted back through the current parent"),
             Scene.UpdatedComponent->GetComponentQuat().AngularDistance(FirstExpectedWorldRotation) < 0.001f);
    TestTrue(TEXT("Parent travel is not added to SplineSpeed"), FMath::IsNearlyEqual(FirstProgress, 10.0f, 0.01f));
    TestTrue(TEXT("Attached spline motion uses parent-relative space"),
             FVRExpGrabbableMotionTestAccessor::IsUsingParentRelativeMotionSpace(*Scene.MotionComponent));

    USceneComponent *ParentB = Scene.CreateExternalParent();
    if (!TestNotNull(TEXT("Replacement moving parent was created"), ParentB))
    {
        return false;
    }
    ParentB->SetWorldLocationAndRotation(FVector(-3000.0f, 800.0f, 100.0f),
                                         FRotator(-10.0f, -35.0f, 0.0f).Quaternion(), false, nullptr,
                                         ETeleportType::TeleportPhysics);
    TestTrue(TEXT("Motion object switched parents without changing world pose"),
             Scene.UpdatedComponent->AttachToComponent(ParentB, FAttachmentTransformRules::KeepWorldTransform));
    ParentB->SetWorldLocation(FVector(-9000.0f, 1200.0f, 500.0f), false, nullptr,
                              ETeleportType::TeleportPhysics);
    Scene.Tick(0.25f);

    const float SecondProgress =
        FVRExpGrabbableMotionTestAccessor::GetCurrentSplineProgress(*Scene.MotionComponent);
    const FVector SecondExpectedWorldLocation = Scene.SplineComponent->GetLocationAtDistanceAlongSpline(
        SecondProgress, ESplineCoordinateSpace::World);
    TestTrue(TEXT("A runtime parent switch re-resolves the moving spline target"),
             Scene.UpdatedComponent->GetComponentLocation().Equals(SecondExpectedWorldLocation, 0.1f));
    TestTrue(TEXT("Spline progress remains based only on configured motion speed after parent switch"),
             FMath::IsNearlyEqual(SecondProgress, 20.0f, 0.01f));

    const FQuat PreservedRelativeRotation = FRotator(12.0f, 34.0f, 56.0f).Quaternion();
    Scene.UpdatedComponent->SetRelativeRotation(PreservedRelativeRotation, false, nullptr,
                                                ETeleportType::TeleportPhysics);
    Scene.MotionComponent->bUpdateRotationDuringMotion = false;
    ParentA->SetWorldLocationAndRotation(FVector(8000.0f, 4000.0f, -200.0f),
                                         FRotator(25.0f, 150.0f, -15.0f).Quaternion(), false, nullptr,
                                         ETeleportType::TeleportPhysics);
    ParentB->SetWorldLocationAndRotation(FVector(-12000.0f, -2500.0f, 900.0f),
                                         FRotator(5.0f, 70.0f, 20.0f).Quaternion(), false, nullptr,
                                         ETeleportType::TeleportPhysics);
    Scene.Tick(0.25f);

    const float ThirdProgress =
        FVRExpGrabbableMotionTestAccessor::GetCurrentSplineProgress(*Scene.MotionComponent);
    const FVector ThirdExpectedWorldLocation = Scene.SplineComponent->GetLocationAtDistanceAlongSpline(
        ThirdProgress, ESplineCoordinateSpace::World);
    TestTrue(TEXT("Spline location stays correct when spline and object have different moving parents"),
             Scene.UpdatedComponent->GetComponentLocation().Equals(ThirdExpectedWorldLocation, 0.1f));
    TestTrue(TEXT("Disabled rotation updates preserve relative rotation and inherit parent rotation"),
             Scene.UpdatedComponent->GetRelativeTransform().GetRotation().AngularDistance(
                 PreservedRelativeRotation) < 0.001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVRExpGrabbableMotionDynamicReturnAnchorTest,
                                 "VRExpansionExtensions.Motion.RelativeSpace.DynamicReturnAnchorAndReleaseOrder",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpGrabbableMotionDynamicReturnAnchorTest::RunTest(const FString &Parameters)
{
    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Dynamic return anchor scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        USceneComponent *InitialParent = Scene.CreateExternalParent();
        InitialParent->SetWorldLocationAndRotation(FVector(100.0f, 200.0f, 50.0f),
                                                   FRotator(0.0f, 15.0f, 0.0f).Quaternion(), false, nullptr,
                                                   ETeleportType::TeleportPhysics);
        const FTransform InitialRelative(FRotator(5.0f, 25.0f, 10.0f).Quaternion(),
                                         FVector(40.0f, -20.0f, 15.0f));
        TestTrue(TEXT("Initial moving-parent anchor was captured"),
                 Scene.CacheInitialParent(InitialParent, InitialRelative));
        Scene.DetachObjectKeepWorld();

        InitialParent->SetWorldLocationAndRotation(FVector(4000.0f, -3000.0f, 700.0f),
                                                   FRotator(20.0f, 105.0f, -10.0f).Quaternion(), false, nullptr,
                                                   ETeleportType::TeleportPhysics);
        Scene.SetObjectTransform(FVector(-5000.0f, 2000.0f, 1200.0f),
                                 FRotator(0.0f, -90.0f, 0.0f).Quaternion());
        Scene.MotionComponent->ReleaseMotionMode = EVRExpGrabbableReleaseMotionMode::ReturnToStart;
        Scene.MotionComponent->ReturnSpeed = 100000.0f;
        Scene.MotionComponent->bSmoothReturn = false;
        FVRExpGrabbableMotionTestAccessor::StartReleaseMotionFromDrop(*Scene.MotionComponent);
        Scene.Tick(0.1f);
        Scene.Tick(0.1f);

        const FTransform ExpectedWorld = InitialRelative * InitialParent->GetComponentTransform();
        TestTrue(TEXT("ReturnToStart follows the original parent after it moved during the grab"),
                 Scene.UpdatedComponent->GetComponentLocation().Equals(ExpectedWorld.GetLocation(), 0.1f));
        TestTrue(TEXT("ReturnToStart restores the dynamically resolved world orientation"),
                 Scene.UpdatedComponent->GetComponentQuat().AngularDistance(ExpectedWorld.GetRotation()) < 0.001f);
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Release ordering scene is valid"), Scene.IsReady()))
        {
            return false;
        }

        USceneComponent *InitialParent = Scene.CreateExternalParent();
        TestTrue(TEXT("Release ordering parent was cached"),
                 Scene.CacheInitialParent(InitialParent, FTransform(FVector(25.0f, 0.0f, 0.0f))));
        Scene.DetachObjectKeepWorld();
        Scene.MotionComponent->bReattachOwnerToInitialParentComponentOnRelease = true;
        Scene.MotionComponent->ReleaseMotionMode = EVRExpGrabbableReleaseMotionMode::ReturnToStart;
        Scene.MotionComponent->ReleaseAttachmentRules.ReturnToStartRule.Timing =
            EVRExpGrabbableReleaseAttachmentTiming::OnRelease;

        FVRExpGrabbableMotionTestAccessor::PrepareReleaseAttachmentBeforeMotion(
            *Scene.MotionComponent, EVRExpGrabbableReleaseMotionMode::ReturnToStart);
        TestTrue(TEXT("OnRelease attachment is complete before release motion initialization"),
                 Scene.UpdatedComponent->GetAttachParent() == InitialParent);
        FVRExpGrabbableMotionTestAccessor::StartReleaseMotionFromDrop(*Scene.MotionComponent);
        FVRExpGrabbableMotionTestAccessor::FinalizeReleaseAttachmentAfterMotionStart(*Scene.MotionComponent);
        TestEqual(TEXT("Return motion starts in the newly attached parent space"),
                  Scene.MotionComponent->GetMotionState(), EVRExpGrabbableMotionState::Releasing);
        TestTrue(TEXT("The newly attached parent is the active relative motion space"),
                 FVRExpGrabbableMotionTestAccessor::IsUsingParentRelativeMotionSpace(*Scene.MotionComponent));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVRExpGrabbableMotionAttachedFallModesTest,
                                 "VRExpansionExtensions.Motion.RelativeSpace.AttachedFallModesAndDirections",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpGrabbableMotionAttachedFallModesTest::RunTest(const FString &Parameters)
{
    auto ConfigureKinematicFall = [](FVRExpGrabbableMotionTestScene &Scene)
    {
        Scene.MotionComponent->ReleaseMotionMode = EVRExpGrabbableReleaseMotionMode::FallWithGravity;
        Scene.MotionComponent->AttachedFallWithGravityMode =
            EVRExpGrabbableAttachedFallWithGravityMode::RelativeKinematic;
        Scene.MotionComponent->AttachedKinematicFallCollisionMode =
            EVRExpGrabbableAttachedKinematicFallCollisionMode::NoSweep;
        Scene.MotionComponent->KinematicGravitySource =
            EVRExpGrabbableKinematicGravitySource::CustomAcceleration;
        Scene.MotionComponent->CustomGravityAcceleration = 1000.0f;
        Scene.MotionComponent->MaxFallSpeed = 5000.0f;
    };

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("World-direction fall scene is valid"), Scene.IsReady()))
        {
            return false;
        }
        USceneComponent *Parent = Scene.CreateExternalParent();
        Parent->SetWorldRotation(FRotator(90.0f, 0.0f, 0.0f), false, nullptr,
                                 ETeleportType::TeleportPhysics);
        TestTrue(TEXT("World-direction fall object was attached"),
                 Scene.CacheInitialParent(Parent, FTransform(FVector(0.0f, 0.0f, 100.0f))));
        ConfigureKinematicFall(Scene);
        Scene.MotionComponent->AttachedMotionDirectionMode =
            EVRExpGrabbableAttachedMotionDirectionMode::PreserveWorldDirections;
        const FVector StartWorld = Scene.UpdatedComponent->GetComponentLocation();
        FVRExpGrabbableMotionTestAccessor::StartReleaseMotionFromDrop(*Scene.MotionComponent);
        Scene.Tick(0.1f);

        TestTrue(TEXT("Attached RelativeKinematic fall keeps the attachment"),
                 Scene.UpdatedComponent->GetAttachParent() == Parent);
        TestFalse(TEXT("Attached RelativeKinematic fall does not enable physics"),
                  Scene.UpdatedComponent->IsSimulatingPhysics());
        TestTrue(TEXT("PreserveWorldDirections falls along world down through a rotated parent"),
                 (Scene.UpdatedComponent->GetComponentLocation() - StartWorld).Equals(
                     FVector(0.0f, 0.0f, -5.0f), 0.1f));
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Parent-direction fall scene is valid"), Scene.IsReady()))
        {
            return false;
        }
        USceneComponent *Parent = Scene.CreateExternalParent();
        Parent->SetWorldRotation(FRotator(90.0f, 0.0f, 0.0f), false, nullptr,
                                 ETeleportType::TeleportPhysics);
        TestTrue(TEXT("Parent-direction fall object was attached"),
                 Scene.CacheInitialParent(Parent, FTransform(FVector(0.0f, 0.0f, 100.0f))));
        ConfigureKinematicFall(Scene);
        Scene.MotionComponent->AttachedMotionDirectionMode =
            EVRExpGrabbableAttachedMotionDirectionMode::FollowAttachmentParent;
        const FVector StartWorld = Scene.UpdatedComponent->GetComponentLocation();
        const FVector ExpectedWorldDelta = Parent->GetComponentQuat().RotateVector(
            -FVector::UpVector * 5.0f);
        FVRExpGrabbableMotionTestAccessor::StartReleaseMotionFromDrop(*Scene.MotionComponent);
        Scene.Tick(0.1f);

        TestTrue(TEXT("FollowAttachmentParent falls along parent-local down"),
                 (Scene.UpdatedComponent->GetComponentLocation() - StartWorld).Equals(
                     ExpectedWorldDelta, 0.1f));
    }

    {
        FVRExpGrabbableMotionTestScene Scene(true);
        if (!TestTrue(TEXT("Physics compatibility fall scene is valid"), Scene.IsReady()))
        {
            return false;
        }
        USceneComponent *Parent = Scene.CreateExternalParent();
        TestTrue(TEXT("Physics compatibility object was attached"),
                 Scene.CacheInitialParent(Parent, FTransform(FVector(0.0f, 0.0f, 100.0f))));
        Scene.MotionComponent->ReleaseMotionMode = EVRExpGrabbableReleaseMotionMode::FallWithGravity;
        Scene.MotionComponent->AttachedFallWithGravityMode =
            EVRExpGrabbableAttachedFallWithGravityMode::DetachAndSimulatePhysics;
        FVRExpGrabbableMotionTestAccessor::StartReleaseMotionFromDrop(*Scene.MotionComponent);
        TestTrue(TEXT("DetachAndSimulatePhysics removes the attachment"),
                 Scene.UpdatedComponent->GetAttachParent() == nullptr);
        TestTrue(TEXT("DetachAndSimulatePhysics enables real physics"),
                 Scene.UpdatedComponent->IsSimulatingPhysics());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVRExpGrabbableMotionAttachedTargetAndEffectTest,
                                 "VRExpansionExtensions.Motion.RelativeSpace.FollowTargetAndWorldEffect",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpGrabbableMotionAttachedTargetAndEffectTest::RunTest(const FString &Parameters)
{
    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Attached follow scene is valid"), Scene.IsReady()))
        {
            return false;
        }
        USceneComponent *Parent = Scene.CreateExternalParent();
        TestTrue(TEXT("Follow object was attached"),
                 Scene.CacheInitialParent(Parent, FTransform(FVector(-100.0f, 0.0f, 0.0f))));
        TestTrue(TEXT("Follow target shares the moving parent"),
                 Scene.SplineRoot->AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform));
        Scene.MotionComponent->NormalMotionMode = EVRExpGrabbableNormalMotionMode::FollowTarget;
        Scene.MotionComponent->TargetToFollow = Scene.SplineRoot;
        Scene.MotionComponent->FollowDistance = 0.0f;
        Scene.MotionComponent->FollowSpeed = 100.0f;
        FVRExpGrabbableMotionTestAccessor::StartNormalMotion(*Scene.MotionComponent);

        Parent->SetWorldLocationAndRotation(FVector(10000.0f, -5000.0f, 400.0f),
                                            FRotator(10.0f, 90.0f, 0.0f).Quaternion(), false, nullptr,
                                            ETeleportType::TeleportPhysics);
        Scene.Tick(0.1f);
        const FVector ExpectedRelative(-90.0f, 0.0f, 0.0f);
        TestTrue(TEXT("FollowSpeed reduces only relative error and does not chase parent travel"),
                 Scene.UpdatedComponent->GetRelativeLocation().Equals(ExpectedRelative, 0.1f));
        TestTrue(TEXT("Follow result inherits the complete parent transform"),
                 Scene.UpdatedComponent->GetComponentLocation().Equals(
                     Parent->GetComponentTransform().TransformPosition(ExpectedRelative), 0.1f));
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Attached effect scene is valid"), Scene.IsReady()))
        {
            return false;
        }
        USceneComponent *Parent = Scene.CreateExternalParent();
        Parent->SetWorldRotation(FRotator(90.0f, 0.0f, 0.0f), false, nullptr,
                                 ETeleportType::TeleportPhysics);
        TestTrue(TEXT("Effect object was attached"),
                 Scene.CacheInitialParent(Parent, FTransform::Identity));
        Scene.MotionComponent->NormalMotionMode = EVRExpGrabbableNormalMotionMode::None;
        Scene.MotionComponent->FloatingEffect.bEnableFloating = true;
        Scene.MotionComponent->FloatingEffect.Space = EVRExpGrabbableMotionEffectSpace::World;
        Scene.MotionComponent->FloatingEffect.Axis = FVector::UpVector;
        Scene.MotionComponent->FloatingEffect.Amplitude = 10.0f;
        Scene.MotionComponent->FloatingEffect.Frequency = 0.25f;
        Scene.MotionComponent->AttachedMotionDirectionMode =
            EVRExpGrabbableAttachedMotionDirectionMode::PreserveWorldDirections;
        const FVector BaseWorld = Scene.UpdatedComponent->GetComponentLocation();
        FVRExpGrabbableMotionTestAccessor::StartNormalMotion(*Scene.MotionComponent);
        Scene.Tick(1.0f);

        TestTrue(TEXT("World floating keeps world direction through a rotated parent"),
                 Scene.UpdatedComponent->GetComponentLocation().Equals(
                     BaseWorld + FVector(0.0f, 0.0f, 10.0f), 0.1f));
        TestTrue(TEXT("World floating is still written as a relative transform"),
                 FVRExpGrabbableMotionTestAccessor::IsUsingParentRelativeMotionSpace(*Scene.MotionComponent));
    }

    {
        FVRExpGrabbableMotionTestScene Scene;
        if (!TestTrue(TEXT("Attached wander and orbit scene is valid"), Scene.IsReady()))
        {
            return false;
        }
        USceneComponent *Parent = Scene.CreateExternalParent();
        TestTrue(TEXT("Wander object was attached"),
                 Scene.CacheInitialParent(Parent, FTransform(FVector(30.0f, 0.0f, 0.0f))));
        TestTrue(TEXT("Orbit center shares the moving parent"),
                 Scene.SplineRoot->AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform));

        Scene.MotionComponent->NormalMotionMode = EVRExpGrabbableNormalMotionMode::RandomWander;
        Scene.MotionComponent->WanderRadius = 0.0f;
        Scene.MotionComponent->WanderSpeed = 100.0f;
        FVRExpGrabbableMotionTestAccessor::StartNormalMotion(*Scene.MotionComponent);
        Parent->SetWorldLocationAndRotation(FVector(7000.0f, 2500.0f, -300.0f),
                                            FRotator(10.0f, 120.0f, 5.0f).Quaternion(), false, nullptr,
                                            ETeleportType::TeleportPhysics);
        Scene.Tick(0.1f);
        TestTrue(TEXT("Zero-radius RandomWander keeps its captured parent-relative origin"),
                 Scene.UpdatedComponent->GetRelativeLocation().Equals(FVector(30.0f, 0.0f, 0.0f), 0.1f));

        Scene.UpdatedComponent->SetRelativeLocation(FVector(130.0f, 0.0f, 0.0f), false, nullptr,
                                                    ETeleportType::TeleportPhysics);
        Scene.MotionComponent->ReleaseMotionMode = EVRExpGrabbableReleaseMotionMode::ReturnToOrigin;
        Scene.MotionComponent->ReturnSpeed = 10000.0f;
        Scene.MotionComponent->bSmoothReturn = false;
        FVRExpGrabbableMotionTestAccessor::StartReleaseMotionFromDrop(*Scene.MotionComponent);
        Parent->SetWorldLocation(FVector(15000.0f, -6000.0f, 900.0f), false, nullptr,
                                 ETeleportType::TeleportPhysics);
        Scene.Tick(0.1f);
        Scene.Tick(0.1f);
        TestTrue(TEXT("ReturnToOrigin resolves the captured origin in the moving parent space"),
                 Scene.UpdatedComponent->GetRelativeLocation().Equals(FVector(30.0f, 0.0f, 0.0f), 0.1f));

        Scene.MotionComponent->OrbitCenter = Scene.SplineRoot;
        Scene.MotionComponent->OrbitRadius = 100.0f;
        Scene.MotionComponent->OrbitHeightOffset = 10.0f;
        Scene.MotionComponent->OrbitSpeed = 0.0f;
        Scene.MotionComponent->AttachedMotionDirectionMode =
            EVRExpGrabbableAttachedMotionDirectionMode::FollowAttachmentParent;
        Scene.MotionComponent->SetNormalMotionMode(EVRExpGrabbableNormalMotionMode::OrbitTarget);
        Parent->SetWorldLocationAndRotation(FVector(-4000.0f, 9000.0f, 500.0f),
                                            FRotator(-20.0f, -75.0f, 15.0f).Quaternion(), false, nullptr,
                                            ETeleportType::TeleportPhysics);
        Scene.Tick(0.1f);
        TestTrue(TEXT("OrbitTarget writes its configured parent-local offset in relative space"),
                 Scene.UpdatedComponent->GetRelativeLocation().Equals(
                     FVector(100.0f, 0.0f, 10.0f), 0.1f));
    }
    return true;
}

#endif
