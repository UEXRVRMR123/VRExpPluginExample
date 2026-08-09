#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/SceneComponent.h"
#include "Curves/CurveFloat.h"
#include "Detection/VRExpDetectableComponent.h"
#include "Detection/VRExpDetectableTriggerLogicBase.h"
#include "GameFramework/Actor.h"
#include "Interaction/VRExpGrabbableMotionComponent.h"
#include "Interaction/VRExpGripInteractionTypes.h"
#include "UI/VRExpDetectableUIInfoTriggerLogic.h"
#include "UI/VRExpUIInfoPlacementResolver.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVRExpMotionPhaseDerivationTest,
    "VRExpansionExtensions.Detection.MotionPhase.Derivation",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FVRExpMotionPhaseDerivationTest::RunTest(
    const FString &Parameters)
{
    TestEqual(
        TEXT("An active grip has priority over Releasing"),
        ResolveVRExpGrabbableMotionPhase(
            EVRExpGrabbableMotionState::Releasing,
            1),
        EVRExpGrabbableMotionPhase::Grabbed);
    TestEqual(
        TEXT("Releasing with no grips remains Releasing"),
        ResolveVRExpGrabbableMotionPhase(
            EVRExpGrabbableMotionState::Releasing,
            0),
        EVRExpGrabbableMotionPhase::Releasing);
    TestEqual(
        TEXT("Idle with no grips is Normal"),
        ResolveVRExpGrabbableMotionPhase(
            EVRExpGrabbableMotionState::Idle,
            0),
        EVRExpGrabbableMotionPhase::Normal);
    TestEqual(
        TEXT("Paused with no grips is still the coarse Normal phase"),
        ResolveVRExpGrabbableMotionPhase(
            EVRExpGrabbableMotionState::Paused,
            0),
        EVRExpGrabbableMotionPhase::Normal);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVRExpAutomaticGripSourceTest,
    "VRExpansionExtensions.Detection.GripSource.AutomaticResolution",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FVRExpAutomaticGripSourceTest::RunTest(
    const FString &Parameters)
{
    TestEqual(
        TEXT("One Motion component is authoritative"),
        ResolveVRExpAutomaticGripSource(1, true),
        EVRExpDetectableGripSource::GrabbableMotionComponent);
    TestEqual(
        TEXT("No Motion component falls back to direct Grip targets"),
        ResolveVRExpAutomaticGripSource(0, true),
        EVRExpDetectableGripSource::DetectableTargets);
    TestEqual(
        TEXT("No Motion component and no Grip interface resolves None"),
        ResolveVRExpAutomaticGripSource(0, false),
        EVRExpDetectableGripSource::None);
    TestEqual(
        TEXT("Multiple Motion components remain ambiguous"),
        ResolveVRExpAutomaticGripSource(2, true),
        EVRExpDetectableGripSource::None);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVRExpGetTriggerLogicByClassTest,
    "VRExpansionExtensions.Detection.TriggerLogic.GetByClass",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FVRExpGetTriggerLogicByClassTest::RunTest(
    const FString &Parameters)
{
    UVRExpDetectableComponent *DetectableComponent =
        NewObject<UVRExpDetectableComponent>();
    UVRExpDetectableUIInfoTriggerLogic *FirstLogic =
        NewObject<UVRExpDetectableUIInfoTriggerLogic>(
            DetectableComponent);
    UVRExpDetectableUIInfoTriggerLogic *SecondLogic =
        NewObject<UVRExpDetectableUIInfoTriggerLogic>(
            DetectableComponent);

    DetectableComponent->TriggerLogics.Add(nullptr);
    DetectableComponent->TriggerLogics.Add(FirstLogic);
    DetectableComponent->TriggerLogics.Add(SecondLogic);

    TestEqual(
        TEXT("The first matching Trigger Logic is returned"),
        DetectableComponent->GetTriggerLogicByClass(
            UVRExpDetectableUIInfoTriggerLogic::StaticClass()),
        static_cast<UVRExpDetectableTriggerLogicBase *>(FirstLogic));
    TestEqual(
        TEXT("A base-class query accepts derived Trigger Logics"),
        DetectableComponent->GetTriggerLogicByClass(
            UVRExpDetectableTriggerLogicBase::StaticClass()),
        static_cast<UVRExpDetectableTriggerLogicBase *>(FirstLogic));
    TestNull(
        TEXT("An unset class returns null"),
        DetectableComponent->GetTriggerLogicByClass(
            TSubclassOf<UVRExpDetectableTriggerLogicBase>()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVRExpUISourceSwitchActivationTest,
    "VRExpansionExtensions.UI.Activation.SourceSwitches",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FVRExpUISourceSwitchActivationTest::RunTest(
    const FString &Parameters)
{
    UVRExpDetectableUIInfoTriggerLogic *Logic =
        NewObject<UVRExpDetectableUIInfoTriggerLogic>();
    UVRExpGrabbableMotionComponent *MotionComponent =
        NewObject<UVRExpGrabbableMotionComponent>();

    FVRExpDetectableInteractionContext Context;
    Context.bIsHeadDetected = true;
    Context.MotionPhase =
        EVRExpGrabbableMotionPhase::Normal;
    Context.GrabbableMotionComponent =
        MotionComponent;

    Logic->Initialize(nullptr, Context);
    TestTrue(
        TEXT("Head Detection activates during Normal"),
        Logic->IsActive());
    TestEqual(
        TEXT("Head Detection is the reported match"),
        Logic->GetTriggerRuntimeDebugState().
            MatchedActivationSource,
        EVRExpDetectableActivationMatchSource::
            HeadDetection);

    Context.MotionPhase =
        EVRExpGrabbableMotionPhase::Grabbed;
    Logic->EvaluateContext(Context);
    TestFalse(
        TEXT("Head Detection is blocked outside Normal when Motion exists"),
        Logic->IsActive());
    TestTrue(
        TEXT("Head phase failure is specific"),
        Logic->GetTriggerRuntimeDebugState().
            FirstFailureReason.Contains(
                TEXT("Motion Phase is not Normal")));

    Context.GrabbableMotionComponent = nullptr;
    Context.MotionPhase =
        EVRExpGrabbableMotionPhase::Unavailable;
    Logic->EvaluateContext(Context);
    TestTrue(
        TEXT("Head Detection remains available without Motion"),
        Logic->IsActive());

    Context.bIsHeadDetected = false;
    Context.bIsGripped = true;
    Logic->EvaluateContext(Context);
    TestTrue(
        TEXT("Direct Grip activates without Motion"),
        Logic->IsActive());
    TestEqual(
        TEXT("Direct Grip is the reported match"),
        Logic->GetTriggerRuntimeDebugState().
            MatchedActivationSource,
        EVRExpDetectableActivationMatchSource::Grip);

    Context.GrabbableMotionComponent =
        MotionComponent;
    Context.MotionPhase =
        EVRExpGrabbableMotionPhase::Grabbed;
    Logic->EvaluateContext(Context);
    TestTrue(
        TEXT("Motion Component Grip also activates"),
        Logic->IsActive());

    Context.bIsGripped = false;
    Context.MotionPhase =
        EVRExpGrabbableMotionPhase::Releasing;
    Logic->GripPresentationSettings.AnchorType =
        EVRExpUIInfoAnchorType::World;
    Logic->ReleasePresentationSettings.AnchorType =
        EVRExpUIInfoAnchorType::DetectableOwner;
    Logic->ReleaseBehavior =
        EVRExpUIInfoReleaseBehavior::
            DeferToCurrentInteraction;
    Logic->EvaluateContext(Context);
    TestFalse(
        TEXT("Defer does not create a Release source"),
        Logic->IsActive());

    Logic->ReleaseBehavior =
        EVRExpUIInfoReleaseBehavior::HideDuringRelease;
    Logic->EvaluateContext(Context);
    TestTrue(
        TEXT("Hide During Release activates Release"),
        Logic->IsActive());
    TestEqual(
        TEXT("Release is the reported match"),
        Logic->GetTriggerRuntimeDebugState().
            MatchedActivationSource,
        EVRExpDetectableActivationMatchSource::Release);
    TestEqual(
        TEXT("Hide During Release publishes the Release interaction source"),
        Logic->GetRuntimeDebugState().
            InteractionSource,
        EVRExpUIInfoInteractionSource::Release);
    TestFalse(
        TEXT("Hide During Release keeps the presentation hidden"),
        Logic->IsUIActorVisible());

    Logic->ReleaseBehavior =
        EVRExpUIInfoReleaseBehavior::KeepGripPresentation;
    AddExpectedError(
        TEXT("cannot resolve a local player for Release presentation"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    Logic->EvaluateContext(Context);
    TestTrue(
        TEXT("Keep Grip Presentation activates Release"),
        Logic->IsActive());
    TestEqual(
        TEXT("Keep Grip Presentation uses Grip placement settings"),
        Logic->GetRuntimeDebugState().AnchorType,
        EVRExpUIInfoAnchorType::World);

    Logic->ReleaseBehavior =
        EVRExpUIInfoReleaseBehavior::UseReleasePresentation;
    Logic->EvaluateContext(Context);
    TestTrue(
        TEXT("Use Release Presentation activates Release"),
        Logic->IsActive());
    TestEqual(
        TEXT("Use Release Presentation uses Release placement settings"),
        Logic->GetRuntimeDebugState().AnchorType,
        EVRExpUIInfoAnchorType::DetectableOwner);

    Context.MotionPhase =
        EVRExpGrabbableMotionPhase::Normal;
    Logic->bEnableMotionOnlyPresentation = true;
    Logic->EvaluateContext(Context);
    TestTrue(
        TEXT("Motion-only switch activates with Motion"),
        Logic->IsActive());
    TestEqual(
        TEXT("Motion is the reported fallback match"),
        Logic->GetTriggerRuntimeDebugState().
            MatchedActivationSource,
        EVRExpDetectableActivationMatchSource::Motion);

    Context.bIsHeadDetected = true;
    Context.bIsGripped = true;
    Context.MotionPhase =
        EVRExpGrabbableMotionPhase::Releasing;
    Logic->EvaluateContext(Context);
    TestEqual(
        TEXT("Grip remains higher priority than Release, Head, and Motion"),
        Logic->GetTriggerRuntimeDebugState().
            MatchedActivationSource,
        EVRExpDetectableActivationMatchSource::Grip);

    Context.bIsHeadDetected = false;
    Context.bIsGripped = false;
    Context.MotionPhase =
        EVRExpGrabbableMotionPhase::Normal;
    Logic->bActivateOnHeadDetection = false;
    Logic->bActivateWhileGripped = false;
    Logic->ReleaseBehavior =
        EVRExpUIInfoReleaseBehavior::DeferToCurrentInteraction;
    Logic->bEnableMotionOnlyPresentation = false;
    Logic->EvaluateContext(Context);
    TestFalse(
        TEXT("Disabling every source deactivates the Trigger Logic"),
        Logic->IsActive());
    TestTrue(
        TEXT("Disabled sources report a specific activation failure"),
        Logic->GetTriggerRuntimeDebugState().
            FirstFailureReason.Contains(
                TEXT("No presentation sources are enabled")));

    Logic->Deinitialize(Context);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVRExpUIHeadTimedPresentationTest,
    "VRExpansionExtensions.UI.Head.TimedPresentation",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FVRExpUIHeadTimedPresentationTest::RunTest(
    const FString &Parameters)
{
    UVRExpDetectableUIInfoTriggerLogic *Logic =
        NewObject<UVRExpDetectableUIInfoTriggerLogic>();

    TestEqual(
        TEXT("Existing assets default to While Detected"),
        Logic->HeadPresentationMode,
        EVRExpUIInfoHeadPresentationMode::WhileDetected);
    TestEqual(
        TEXT("Timed Head visibility defaults to three seconds"),
        Logic->HeadVisibleDurationSeconds,
        3.0f);
    TestEqual(
        TEXT("Timed Head cooldown defaults to ten seconds"),
        Logic->HeadCooldownSeconds,
        10.0f);
    TestEqual(
        TEXT("Detection loss defaults to keeping the visible window"),
        Logic->HeadDetectionLossBehavior,
        EVRExpUIInfoHeadDetectionLossBehavior::
            KeepVisibleUntilDurationExpires);
    TestEqual(
        TEXT("Cooldown defaults to reactivating while still detected"),
        Logic->HeadCooldownCompletionBehavior,
        EVRExpUIInfoHeadCooldownCompletionBehavior::
            ReactivateIfStillDetected);

    Logic->HeadPresentationMode =
        EVRExpUIInfoHeadPresentationMode::TimedWithCooldown;

    TestFalse(
        TEXT("Reactivate If Still Detected does not request a fresh detection"),
        Logic->ShouldAwaitFreshHeadDetectionAfterCooldown(true));
    Logic->HeadCooldownCompletionBehavior =
        EVRExpUIInfoHeadCooldownCompletionBehavior::RequireNewDetection;
    TestTrue(
        TEXT("Require New Detection waits when Head is still detected"),
        Logic->ShouldAwaitFreshHeadDetectionAfterCooldown(true));
    TestFalse(
        TEXT("Require New Detection is ready when Head already left"),
        Logic->ShouldAwaitFreshHeadDetectionAfterCooldown(false));

    FVRExpDetectableInteractionContext Context;
    Context.bIsHeadDetected = true;
    Context.MotionPhase =
        EVRExpGrabbableMotionPhase::Unavailable;
    TestTrue(
        TEXT("Timed Head presentation is initially eligible"),
        Logic->IsHeadSourceEligible(Context));

    Logic->bHeadCooldownActive = true;
    TestFalse(
        TEXT("Cooldown blocks only the Head source"),
        Logic->IsHeadSourceEligible(Context));

    Context.bIsGripped = true;
    EVRExpDetectableActivationMatchSource MatchedSource =
        EVRExpDetectableActivationMatchSource::None;
    FString FailureReason;
    TestTrue(
        TEXT("Grip remains eligible during Head cooldown"),
        Logic->EvaluateSourceSwitches(
            Context,
            MatchedSource,
            FailureReason));
    TestEqual(
        TEXT("Grip keeps priority during Head cooldown"),
        MatchedSource,
        EVRExpDetectableActivationMatchSource::Grip);

    Context.bIsGripped = false;
    Logic->bHeadCooldownActive = false;
    Logic->bAwaitingFreshHeadDetection = true;
    TestFalse(
        TEXT("Require New Detection blocks a continuously detected Head"),
        Logic->IsHeadSourceEligible(Context));

    Context.ChangeSource =
        EVRExpDetectableChangeSource::HeadDetection;
    Context.ChangePhase =
        EVRExpDetectableChangePhase::Began;
    Logic->HandleHeadDetectionStarted(Context);
    TestFalse(
        TEXT("A fresh Head begin clears the re-detection latch"),
        Logic->bAwaitingFreshHeadDetection);
    TestTrue(
        TEXT("Head becomes eligible after the fresh begin"),
        Logic->IsHeadSourceEligible(Context));

    Logic->bHeadTimedPresentationActive = true;
    Context.bIsHeadDetected = false;
    Context.ChangePhase =
        EVRExpDetectableChangePhase::Ended;
    Logic->HeadDetectionLossBehavior =
        EVRExpUIInfoHeadDetectionLossBehavior::
            KeepVisibleUntilDurationExpires;
    TestTrue(
        TEXT("Keep Visible retains Head eligibility after detection ends"),
        Logic->IsHeadSourceEligible(Context));
    TestFalse(
        TEXT("Keep Visible does not end the timed window early"),
        Logic->ShouldEndTimedHeadPresentationOnDetectionLoss(Context));

    Logic->HeadDetectionLossBehavior =
        EVRExpUIInfoHeadDetectionLossBehavior::HideAndStartCooldown;
    TestFalse(
        TEXT("Hide And Start Cooldown removes Head eligibility"),
        Logic->IsHeadSourceEligible(Context));
    TestTrue(
        TEXT("Hide And Start Cooldown ends the timed window on detection loss"),
        Logic->ShouldEndTimedHeadPresentationOnDetectionLoss(Context));

    Logic->bUIActorVisible = true;
    Logic->bPresentationTargetAvailable = true;
    Logic->CurrentPresentationSettings.PresentationPolicy =
        EVRExpUIInfoPresentationPolicy::Independent;
    TestTrue(
        TEXT("A visible resolved non-exclusive Head presentation consumes time"),
        Logic->ShouldCountHeadVisibleDuration(true));
    Logic->bPresentationTargetAvailable = false;
    TestFalse(
        TEXT("A missing Head anchor pauses visible-duration accounting"),
        Logic->ShouldCountHeadVisibleDuration(true));
    Logic->bPresentationTargetAvailable = true;
    Logic->CurrentPresentationSettings.PresentationPolicy =
        EVRExpUIInfoPresentationPolicy::ExclusivePerLocalPlayer;
    Logic->bExclusivePresentationGranted = false;
    TestFalse(
        TEXT("An ungranted exclusive presentation pauses visible-duration accounting"),
        Logic->ShouldCountHeadVisibleDuration(true));
    Logic->bExclusivePresentationGranted = true;
    TestTrue(
        TEXT("A granted exclusive presentation consumes visible duration"),
        Logic->ShouldCountHeadVisibleDuration(true));
    TestFalse(
        TEXT("An externally destroyed UI Actor pauses visible-duration accounting"),
        Logic->ShouldCountHeadVisibleDuration(false));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVRExpUIPlacementResolverTest,
    "VRExpansionExtensions.UI.Placement.SharedResolver",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FVRExpUIPlacementResolverTest::RunTest(
    const FString &Parameters)
{
    FVRExpUIInfoPresentationSettings WorldSettings;
    WorldSettings.AnchorType = EVRExpUIInfoAnchorType::World;
    WorldSettings.BindingMode = EVRExpUIInfoBindingMode::Follow;
    WorldSettings.WorldTransform = FTransform(
        FRotator(0.0f, 35.0f, 0.0f),
        FVector(120.0f, -45.0f, 210.0f),
        FVector(1.2f));
    FVRExpUIInfoPlacementResolver::NormalizeSettings(WorldSettings);
    TestEqual(
        TEXT("World anchor is normalized to ResolveOnce"),
        WorldSettings.BindingMode,
        EVRExpUIInfoBindingMode::ResolveOnce);

    FVRExpUIInfoPlacementRequest WorldRequest;
    WorldRequest.Settings = &WorldSettings;
    FVRExpUIInfoPlacementResult WorldResult;
    FString FailureReason;
    TestTrue(
        TEXT("Fixed world transform resolves"),
        FVRExpUIInfoPlacementResolver::Resolve(
            WorldRequest,
            WorldResult,
            FailureReason));
    TestTrue(
        TEXT("Fixed world transform is unchanged"),
        WorldResult.TargetWorldTransform.Equals(
            WorldSettings.WorldTransform));

    FVRExpUIInfoPresentationSettings CameraSettings;
    CameraSettings.AnchorType = EVRExpUIInfoAnchorType::Camera;
    CameraSettings.BindingMode = EVRExpUIInfoBindingMode::Follow;
    CameraSettings.RelativeOffset = FTransform(
        FRotator::ZeroRotator,
        FVector(100.0f, 0.0f, 0.0f));
    FVRExpUIInfoPlacementRequest CameraRequest;
    CameraRequest.Settings = &CameraSettings;
    CameraRequest.bHasViewTransform = true;
    CameraRequest.ViewTransform = FTransform(
        FRotator(0.0f, 90.0f, 0.0f),
        FVector(10.0f, 20.0f, 30.0f));
    FVRExpUIInfoPlacementResult CameraResult;
    TestTrue(
        TEXT("Camera Follow transform resolves from an editor/runtime view"),
        FVRExpUIInfoPlacementResolver::Resolve(
            CameraRequest,
            CameraResult,
            FailureReason));
    const FTransform ExpectedCameraTarget =
        CameraSettings.RelativeOffset *
        CameraRequest.ViewTransform;
    TestTrue(
        TEXT("Camera Follow uses the shared transform composition"),
        CameraResult.TargetWorldTransform.Equals(
            ExpectedCameraTarget));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVRExpUIInteractionSourcePriorityTest,
    "VRExpansionExtensions.UI.Release.SourcePriority",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FVRExpUIInteractionSourcePriorityTest::RunTest(
    const FString &Parameters)
{
    TestEqual(
        TEXT("Grip has priority over Release and Head"),
        ResolveVRExpUIInfoInteractionSource(
            true,
            EVRExpGrabbableMotionPhase::Releasing,
            EVRExpUIInfoReleaseBehavior::UseReleasePresentation,
            true,
            true,
            true),
        EVRExpUIInfoInteractionSource::Grip);
    TestEqual(
        TEXT("Independent Release has priority over Head"),
        ResolveVRExpUIInfoInteractionSource(
            false,
            EVRExpGrabbableMotionPhase::Releasing,
            EVRExpUIInfoReleaseBehavior::UseReleasePresentation,
            true,
            true,
            true),
        EVRExpUIInfoInteractionSource::Release);
    TestEqual(
        TEXT("Legacy Defer behavior returns to Head"),
        ResolveVRExpUIInfoInteractionSource(
            false,
            EVRExpGrabbableMotionPhase::Releasing,
            EVRExpUIInfoReleaseBehavior::DeferToCurrentInteraction,
            true,
            true,
            true),
        EVRExpUIInfoInteractionSource::HeadDetection);
    TestEqual(
        TEXT("Motion is the final presentation fallback"),
        ResolveVRExpUIInfoInteractionSource(
            false,
            EVRExpGrabbableMotionPhase::Normal,
            EVRExpUIInfoReleaseBehavior::UseReleasePresentation,
            false,
            true,
            true),
        EVRExpUIInfoInteractionSource::Motion);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVRExpUIExitTransitionTest,
    "VRExpansionExtensions.UI.ExitTransition.StateAndPolicy",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FVRExpUIExitTransitionTest::RunTest(
    const FString &Parameters)
{
    UVRExpDetectableUIInfoTriggerLogic *Logic =
        NewObject<UVRExpDetectableUIInfoTriggerLogic>();

    TestEqual(
        TEXT("Exit transitions are enabled by default"),
        Logic->ExitSettings.Mode,
        EVRExpUIInfoExitMode::Transition);
    TestTrue(
        TEXT("The default exit duration is 0.2 seconds"),
        FMath::IsNearlyEqual(
            Logic->ExitSettings.DurationSeconds,
            0.2f));
    TestTrue(
        TEXT("Widget opacity fade is enabled by default"),
        Logic->ExitSettings.bFadeWidgetOpacity);
    TestTrue(
        TEXT("Relative transform animation is enabled by default"),
        Logic->ExitSettings.bApplyRelativeTransform);
    TestTrue(
        TEXT("The default exit scale is 0.9"),
        Logic->ExitSettings.RelativeTransform.GetScale3D().Equals(
            FVector(0.9f)));

    TestTrue(
        TEXT("Inactivity timeout uses the exit transition"),
        Logic->ShouldAnimateExit(
            EVRExpUIInfoVisibilityReason::InactivityTimeout));
    TestTrue(
        TEXT("Release hiding uses the exit transition"),
        Logic->ShouldAnimateExit(
            EVRExpUIInfoVisibilityReason::HiddenDuringRelease));
    TestTrue(
        TEXT("Timed Head expiry uses the exit transition"),
        Logic->ShouldAnimateExit(
            EVRExpUIInfoVisibilityReason::HeadVisibleDurationExpired));
    TestTrue(
        TEXT("Head detection loss uses the exit transition"),
        Logic->ShouldAnimateExit(
            EVRExpUIInfoVisibilityReason::HeadDetectionLost));
    TestFalse(
        TEXT("Priority displacement remains immediate"),
        Logic->ShouldAnimateExit(
            EVRExpUIInfoVisibilityReason::PriorityDisplaced));
    TestFalse(
        TEXT("Missing placement targets remain immediate"),
        Logic->ShouldAnimateExit(
            EVRExpUIInfoVisibilityReason::PlacementTargetUnavailable));
    TestFalse(
        TEXT("Deinitialization remains immediate"),
        Logic->ShouldAnimateExit(
            EVRExpUIInfoVisibilityReason::Deinitialized));

    UCurveFloat *Curve = NewObject<UCurveFloat>();
    Curve->FloatCurve.UpdateOrAddKey(0.0f, 0.0f);
    Curve->FloatCurve.UpdateOrAddKey(1.0f, 2.0f);
    Logic->ExitSettings.InterpolationCurve = Curve;
    TestTrue(
        TEXT("Custom curve output is clamped to one"),
        FMath::IsNearlyEqual(
            Logic->EvaluateExitTransitionAlpha(1.0f),
            1.0f));
    Logic->ExitSettings.InterpolationCurve = nullptr;
    TestTrue(
        TEXT("Default easing preserves the midpoint"),
        FMath::IsNearlyEqual(
            Logic->EvaluateExitTransitionAlpha(0.5f),
            0.5f));

    AActor *UIActor = NewObject<AActor>();
    USceneComponent *RootComponent =
        NewObject<USceneComponent>(UIActor);
    UIActor->SetRootComponent(RootComponent);
    const FTransform StartTransform(
        FRotator(0.0f, 15.0f, 0.0f),
        FVector(100.0f, 20.0f, 30.0f),
        FVector::OneVector);
    RootComponent->SetWorldTransform(StartTransform);

    Logic->ExitSettings.bFadeWidgetOpacity = false;
    Logic->SpawnedUIActor = UIActor;
    Logic->bHasSpawnedUIActor = true;
    Logic->bUIActorVisible = true;

    Logic->StartExitTransition(
        EVRExpUIInfoVisibilityReason::InactivityTimeout);
    TestTrue(
        TEXT("A normal hide starts an exit transition"),
        Logic->bExitTransitionActive);
    TestFalse(
        TEXT("The Actor remains rendered while exiting"),
        UIActor->IsHidden());
    Logic->StartExitTransition(
        EVRExpUIInfoVisibilityReason::HiddenDuringRelease);
    TestEqual(
        TEXT("A duplicate forward hide keeps the original exit reason"),
        Logic->ExitVisibilityReason,
        EVRExpUIInfoVisibilityReason::InactivityTimeout);

    Logic->TickExitTransition(0.1f);
    TestTrue(
        TEXT("Half-duration exit reaches half progress"),
        FMath::IsNearlyEqual(
            Logic->ExitTransitionLinearAlpha,
            0.5f));
    TestTrue(
        TEXT("Half-duration exit reaches scale 0.95"),
        FMath::IsNearlyEqual(
            UIActor->GetActorScale3D().X,
            0.95f));

    Logic->bHasLocalInteraction = false;
    Logic->HandleDestroyTimerExpired();
    TestTrue(
        TEXT("A destroy timeout waits for an active exit transition"),
        Logic->bDestroyAfterExitTransition &&
            Logic->SpawnedUIActor.IsValid());
    Logic->bDestroyAfterExitTransition = false;

    Logic->ReverseExitTransition();
    Logic->TickExitTransition(0.1f);
    TestFalse(
        TEXT("Reversing to zero completes restoration"),
        Logic->bExitTransitionActive);
    TestTrue(
        TEXT("Reversal restores the starting transform"),
        UIActor->GetActorTransform().Equals(StartTransform));
    TestTrue(
        TEXT("Reversal never hides the Actor"),
        Logic->bUIActorVisible && !UIActor->IsHidden());
    TestEqual(
        TEXT("Reversal does not broadcast a completed hide reason"),
        Logic->RuntimeDebugState.LastVisibilityReason,
        EVRExpUIInfoVisibilityReason::None);

    Logic->StartExitTransition(
        EVRExpUIInfoVisibilityReason::InactivityTimeout);
    Logic->TickExitTransition(0.2f);
    TestFalse(
        TEXT("A completed exit clears its transition state"),
        Logic->bExitTransitionActive);
    TestTrue(
        TEXT("A completed exit hides the Actor"),
        UIActor->IsHidden() && !Logic->bUIActorVisible);
    TestEqual(
        TEXT("A completed exit records its visibility reason"),
        Logic->RuntimeDebugState.LastVisibilityReason,
        EVRExpUIInfoVisibilityReason::InactivityTimeout);

    Logic->SetUIActorVisibleImmediately(
        true,
        EVRExpUIInfoVisibilityReason::Activated);
    Logic->SetUIActorVisible(
        false,
        EVRExpUIInfoVisibilityReason::PriorityDisplaced);
    TestFalse(
        TEXT("Priority displacement does not start a transition"),
        Logic->bExitTransitionActive);
    TestTrue(
        TEXT("Priority displacement hides immediately"),
        UIActor->IsHidden() && !Logic->bUIActorVisible);

    Logic->ExitSettings.Mode = EVRExpUIInfoExitMode::Immediate;
    TestFalse(
        TEXT("Immediate mode disables normal exit transitions"),
        Logic->ShouldAnimateExit(
            EVRExpUIInfoVisibilityReason::InactivityTimeout));
    Logic->ExitSettings.Mode = EVRExpUIInfoExitMode::Transition;
    Logic->ExitSettings.DurationSeconds = 0.0f;
    TestFalse(
        TEXT("Zero duration falls back to immediate hiding"),
        Logic->ShouldAnimateExit(
            EVRExpUIInfoVisibilityReason::InactivityTimeout));

    return true;
}

#endif
