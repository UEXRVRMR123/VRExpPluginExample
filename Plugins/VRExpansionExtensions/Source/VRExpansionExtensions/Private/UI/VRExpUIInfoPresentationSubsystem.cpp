#include "UI/VRExpUIInfoPresentationSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UI/VRExpDetectableUIInfoTriggerLogic.h"

void UVRExpUIInfoPresentationSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);

    TickLogics.Reset();
    ExclusiveClaims.Reset();
    ExclusiveWinners.Reset();
    NextActivationSequence = 0;
}

void UVRExpUIInfoPresentationSubsystem::Deinitialize()
{
    ExclusiveWinners.Reset();
    ExclusiveClaims.Reset();
    TickLogics.Reset();

    Super::Deinitialize();
}

bool UVRExpUIInfoPresentationSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UVRExpUIInfoPresentationSubsystem::Tick(float DeltaTime)
{
    const TArray<TWeakObjectPtr<UVRExpDetectableUIInfoTriggerLogic>> LogicSnapshot = TickLogics.Array();
    for (const TWeakObjectPtr<UVRExpDetectableUIInfoTriggerLogic> &LogicPointer : LogicSnapshot)
    {
        if (UVRExpDetectableUIInfoTriggerLogic *TriggerLogic = LogicPointer.Get())
        {
            TriggerLogic->TickPresentation(DeltaTime);
        }
    }

    PruneInvalidEntries();
}

bool UVRExpUIInfoPresentationSubsystem::IsTickable() const
{
    return !IsTemplate(RF_ClassDefaultObject) && !TickLogics.IsEmpty();
}

UWorld *UVRExpUIInfoPresentationSubsystem::GetTickableGameObjectWorld() const
{
    return GetWorld();
}

ETickableTickType UVRExpUIInfoPresentationSubsystem::GetTickableTickType() const
{
    return IsTemplate(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

TStatId UVRExpUIInfoPresentationSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UVRExpUIInfoPresentationSubsystem, STATGROUP_Tickables);
}

AActor *UVRExpUIInfoPresentationSubsystem::GetDisplayedExclusiveUIActor(
    APlayerController *LocalPlayerController) const
{
    if (!IsValid(LocalPlayerController))
    {
        if (const UWorld *World = GetWorld())
        {
            for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
            {
                APlayerController *Candidate = Iterator->Get();
                if (IsValid(Candidate) && Candidate->IsLocalController())
                {
                    LocalPlayerController = Candidate;
                    break;
                }
            }
        }
    }

    if (!IsValid(LocalPlayerController))
    {
        return nullptr;
    }

    const TWeakObjectPtr<UVRExpDetectableUIInfoTriggerLogic> *Winner =
        ExclusiveWinners.Find(LocalPlayerController);
    if (!Winner)
    {
        return nullptr;
    }

    UVRExpDetectableUIInfoTriggerLogic *WinnerLogic = Winner->Get();
    return IsValid(WinnerLogic) && WinnerLogic->IsUIActorVisible()
               ? WinnerLogic->GetSpawnedUIActor()
               : nullptr;
}

uint64 UVRExpUIInfoPresentationSubsystem::AllocateActivationSequence()
{
    ++NextActivationSequence;
    if (NextActivationSequence == 0)
    {
        ++NextActivationSequence;
    }
    return NextActivationSequence;
}

void UVRExpUIInfoPresentationSubsystem::RegisterTickLogic(
    UVRExpDetectableUIInfoTriggerLogic *TriggerLogic)
{
    if (IsValid(TriggerLogic))
    {
        TickLogics.Add(TriggerLogic);
    }
}

void UVRExpUIInfoPresentationSubsystem::UnregisterTickLogic(
    UVRExpDetectableUIInfoTriggerLogic *TriggerLogic)
{
    if (!TriggerLogic)
    {
        return;
    }

    TickLogics.Remove(TriggerLogic);
}

void UVRExpUIInfoPresentationSubsystem::UpdateExclusiveClaim(
    UVRExpDetectableUIInfoTriggerLogic *TriggerLogic, APlayerController *InLocalPlayerController,
    int32 Priority, uint64 ActivationSequence, bool bEligible)
{
    if (!IsValid(TriggerLogic) || !IsValid(InLocalPlayerController) ||
        !InLocalPlayerController->IsLocalController())
    {
        RemoveExclusiveClaim(TriggerLogic);
        return;
    }

    TWeakObjectPtr<APlayerController> PreviousPlayerController;
    FExclusivePresentationClaim *ExistingClaim = ExclusiveClaims.FindByPredicate(
        [TriggerLogic](const FExclusivePresentationClaim &Claim)
        {
            return Claim.TriggerLogic.Get() == TriggerLogic;
        });

    if (ExistingClaim)
    {
        PreviousPlayerController = ExistingClaim->LocalPlayerController;
        const bool bClaimUnchanged =
            ExistingClaim->LocalPlayerController.Get() == InLocalPlayerController &&
            ExistingClaim->Priority == Priority &&
            ExistingClaim->ActivationSequence == ActivationSequence &&
            ExistingClaim->bEligible == bEligible;
        if (bClaimUnchanged)
        {
            return;
        }
        ExistingClaim->LocalPlayerController = InLocalPlayerController;
        ExistingClaim->Priority = Priority;
        ExistingClaim->ActivationSequence = ActivationSequence;
        ExistingClaim->bEligible = bEligible;
    }
    else
    {
        FExclusivePresentationClaim &NewClaim = ExclusiveClaims.AddDefaulted_GetRef();
        NewClaim.TriggerLogic = TriggerLogic;
        NewClaim.LocalPlayerController = InLocalPlayerController;
        NewClaim.Priority = Priority;
        NewClaim.ActivationSequence = ActivationSequence;
        NewClaim.bEligible = bEligible;
    }

    if (PreviousPlayerController.IsValid() && PreviousPlayerController.Get() != InLocalPlayerController)
    {
        RefreshExclusivePresentation(PreviousPlayerController.Get());
    }
    RefreshExclusivePresentation(InLocalPlayerController);
}

void UVRExpUIInfoPresentationSubsystem::RemoveExclusiveClaim(
    UVRExpDetectableUIInfoTriggerLogic *TriggerLogic)
{
    if (!TriggerLogic)
    {
        return;
    }

    TSet<TWeakObjectPtr<APlayerController>> AffectedPlayerControllers;
    for (int32 Index = ExclusiveClaims.Num() - 1; Index >= 0; --Index)
    {
        if (ExclusiveClaims[Index].TriggerLogic.Get() == TriggerLogic)
        {
            if (ExclusiveClaims[Index].LocalPlayerController.IsValid())
            {
                AffectedPlayerControllers.Add(ExclusiveClaims[Index].LocalPlayerController);
            }
            ExclusiveClaims.RemoveAtSwap(Index);
        }
    }

    for (const TWeakObjectPtr<APlayerController> &PlayerControllerPointer : AffectedPlayerControllers)
    {
        if (APlayerController *PlayerController = PlayerControllerPointer.Get())
        {
            RefreshExclusivePresentation(PlayerController);
        }
    }
}

void UVRExpUIInfoPresentationSubsystem::PruneInvalidEntries()
{
    bool bRemovedExclusiveClaim = false;

    for (auto Iterator = TickLogics.CreateIterator(); Iterator; ++Iterator)
    {
        if (!Iterator->IsValid())
        {
            Iterator.RemoveCurrent();
        }
    }

    for (int32 Index = ExclusiveClaims.Num() - 1; Index >= 0; --Index)
    {
        if (!ExclusiveClaims[Index].TriggerLogic.IsValid() ||
            !ExclusiveClaims[Index].LocalPlayerController.IsValid())
        {
            ExclusiveClaims.RemoveAtSwap(Index);
            bRemovedExclusiveClaim = true;
        }
    }

    for (auto Iterator = ExclusiveWinners.CreateIterator(); Iterator; ++Iterator)
    {
        if (!Iterator.Key().IsValid() || !Iterator.Value().IsValid())
        {
            Iterator.RemoveCurrent();
        }
    }

    if (bRemovedExclusiveClaim)
    {
        RefreshAllExclusivePresentations();
    }
}

void UVRExpUIInfoPresentationSubsystem::RefreshExclusivePresentation(
    APlayerController *LocalPlayerController)
{
    if (!IsValid(LocalPlayerController))
    {
        return;
    }

    int32 WinningClaimIndex = INDEX_NONE;
    for (int32 Index = 0; Index < ExclusiveClaims.Num(); ++Index)
    {
        const FExclusivePresentationClaim &Claim = ExclusiveClaims[Index];
        if (!Claim.bEligible || Claim.LocalPlayerController.Get() != LocalPlayerController ||
            !Claim.TriggerLogic.IsValid() ||
            !IsValid(Claim.TriggerLogic.Get()->GetSpawnedUIActor()))
        {
            continue;
        }

        if (WinningClaimIndex == INDEX_NONE ||
            Claim.Priority > ExclusiveClaims[WinningClaimIndex].Priority ||
            (Claim.Priority == ExclusiveClaims[WinningClaimIndex].Priority &&
             Claim.ActivationSequence > ExclusiveClaims[WinningClaimIndex].ActivationSequence))
        {
            WinningClaimIndex = Index;
        }
    }

    UVRExpDetectableUIInfoTriggerLogic *WinningLogic =
        WinningClaimIndex != INDEX_NONE ? ExclusiveClaims[WinningClaimIndex].TriggerLogic.Get() : nullptr;

    for (FExclusivePresentationClaim &Claim : ExclusiveClaims)
    {
        if (Claim.LocalPlayerController.Get() != LocalPlayerController)
        {
            continue;
        }

        UVRExpDetectableUIInfoTriggerLogic *CandidateLogic = Claim.TriggerLogic.Get();
        if (!IsValid(CandidateLogic))
        {
            continue;
        }

        const bool bGranted = CandidateLogic == WinningLogic;
        CandidateLogic->HandleExclusivePresentationGranted(bGranted);

        if (!bGranted && Claim.bEligible &&
            !CandidateLogic->ShouldRestoreAfterPriorityLoss())
        {
            Claim.bEligible = false;
            CandidateLogic->HandleExclusiveClaimMadeIneligible();
        }
    }

    if (IsValid(WinningLogic))
    {
        ExclusiveWinners.FindOrAdd(LocalPlayerController) = WinningLogic;
    }
    else
    {
        ExclusiveWinners.Remove(LocalPlayerController);
    }
}

void UVRExpUIInfoPresentationSubsystem::RefreshAllExclusivePresentations()
{
    TSet<TWeakObjectPtr<APlayerController>> PlayerControllers;

    for (const FExclusivePresentationClaim &Claim : ExclusiveClaims)
    {
        if (Claim.LocalPlayerController.IsValid())
        {
            PlayerControllers.Add(Claim.LocalPlayerController);
        }
    }

    for (const auto &WinnerPair : ExclusiveWinners)
    {
        if (WinnerPair.Key.IsValid())
        {
            PlayerControllers.Add(WinnerPair.Key);
        }
    }

    for (const TWeakObjectPtr<APlayerController> &PlayerControllerPointer : PlayerControllers)
    {
        if (APlayerController *PlayerController = PlayerControllerPointer.Get())
        {
            RefreshExclusivePresentation(PlayerController);
        }
    }
}
