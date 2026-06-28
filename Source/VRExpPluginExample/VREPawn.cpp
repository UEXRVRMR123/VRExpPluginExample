// Fill out your copyright notice in the Description page of Project Settings.

#include "VREPawn.h"

#include "Components/SkeletalMeshComponent.h"
#include "GripMotionControllerComponent.h"
#include "IMotionController.h"
#include "Net/UnrealNetwork.h"
#include "ParentRelativeAttachmentComponent.h"
#include "ReplicatedVRCameraComponent.h"
#include "VRExpansionFunctionLibrary.h"
#include "VRPlayerController.h"

FName AVREPawn::LeftMotionControllerComponentName(TEXT("Left Grip Motion Controller"));
FName AVREPawn::RightMotionControllerComponentName(TEXT("Right Grip Motion Controller"));
FName AVREPawn::ReplicatedCameraComponentName(TEXT("VR Replicated Camera"));
FName AVREPawn::ParentRelativeAttachmentComponentName(TEXT("Parent Relative Attachment"));
FName AVREPawn::SmoothingSceneParentComponentName(TEXT("NetSmoother"));
FName AVREPawn::VRProxyComponentName(TEXT("VRProxy"));
FName AVREPawn::MeshComponentName(TEXT("Mesh"));

AVREPawn::AVREPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicatingMovement(true);
	SetMinNetUpdateFrequency(100.0f);

	NetSmoother = CreateDefaultSubobject<USceneComponent>(AVREPawn::SmoothingSceneParentComponentName);
	RootComponent = NetSmoother;

	VRProxyComponent = CreateDefaultSubobject<USceneComponent>(AVREPawn::VRProxyComponentName);
	if (VRProxyComponent)
	{
		VRProxyComponent->SetupAttachment(NetSmoother);
	}

	VRReplicatedCamera = CreateDefaultSubobject<UReplicatedVRCameraComponent>(AVREPawn::ReplicatedCameraComponentName);
	if (VRReplicatedCamera)
	{
		VRReplicatedCamera->SetupAttachment(VRProxyComponent ? VRProxyComponent : NetSmoother);
		VRReplicatedCamera->bUpdateInCharacterMovement = false;
	}

	ParentRelativeAttachment = CreateDefaultSubobject<UParentRelativeAttachmentComponent>(AVREPawn::ParentRelativeAttachmentComponentName);
	if (ParentRelativeAttachment)
	{
		ParentRelativeAttachment->SetupAttachment(VRProxyComponent ? VRProxyComponent : NetSmoother);
		ParentRelativeAttachment->bUpdateInCharacterMovement = false;

		if (VRReplicatedCamera)
		{
			ParentRelativeAttachment->AddTickPrerequisiteComponent(VRReplicatedCamera);
		}
	}

	Mesh = CreateOptionalDefaultSubobject<USkeletalMeshComponent>(AVREPawn::MeshComponentName);
	if (Mesh)
	{
		Mesh->SetupAttachment(ParentRelativeAttachment ? ParentRelativeAttachment : VRProxyComponent ? VRProxyComponent : NetSmoother);
		Mesh->AlwaysLoadOnClient = true;
		Mesh->AlwaysLoadOnServer = true;
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
	}

	LeftMotionController = CreateDefaultSubobject<UGripMotionControllerComponent>(AVREPawn::LeftMotionControllerComponentName);
	if (LeftMotionController)
	{
		LeftMotionController->SetupAttachment(VRProxyComponent ? VRProxyComponent : NetSmoother);
		LeftMotionController->SetTrackingMotionSource(IMotionController::LeftHandSourceId);
	}

	RightMotionController = CreateDefaultSubobject<UGripMotionControllerComponent>(AVREPawn::RightMotionControllerComponentName);
	if (RightMotionController)
	{
		RightMotionController->SetupAttachment(VRProxyComponent ? VRProxyComponent : NetSmoother);
		RightMotionController->SetTrackingMotionSource(IMotionController::RightHandSourceId);
	}

	OffsetComponentToWorld = FTransform::Identity;
}

void AVREPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	RegenerateOffsetComponentToWorld();
}

void AVREPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	OwningVRPlayerController = Cast<AVRPlayerController>(Controller);
}

void AVREPawn::OnRep_Controller()
{
	Super::OnRep_Controller();
	OwningVRPlayerController = Cast<AVRPlayerController>(Controller);
}

void AVREPawn::OnRep_PlayerState()
{
	OnPlayerStateReplicated_Bind.Broadcast(GetPlayerState());
	Super::OnRep_PlayerState();
}

void AVREPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AVREPawn, ReplicatedTeleportState);
}

bool AVREPawn::TeleportTo(const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest, bool bNoCheck)
{
	const bool bTeleportSucceeded = Super::TeleportTo(DestLocation, DestRotation, bIsATest, bNoCheck);

	if (bTeleportSucceeded)
	{
		NotifyOfTeleport();
		RegenerateOffsetComponentToWorld();
	}

	return bTeleportSucceeded;
}

FVector AVREPawn::GetTargetLocation(AActor* RequestedBy) const
{
	return GetVRLocation();
}

void AVREPawn::OnRep_ReplicatedTeleportState()
{
	if (!IsLocallyControlled())
	{
		if (ReplicatedTeleportState.TeleportSequence != LastProcessedTeleportSequence)
		{
			LastProcessedTeleportSequence = ReplicatedTeleportState.TeleportSequence;
			LastProcessedGripTeleportSequence = ReplicatedTeleportState.GripTeleportSequence;
			NotifyOfTeleport();
			return;
		}

		if (ReplicatedTeleportState.GripTeleportSequence != LastProcessedGripTeleportSequence)
		{
			LastProcessedGripTeleportSequence = ReplicatedTeleportState.GripTeleportSequence;
			NotifyOfTeleport(false);
		}
	}

	LastProcessedTeleportSequence = ReplicatedTeleportState.TeleportSequence;
	LastProcessedGripTeleportSequence = ReplicatedTeleportState.GripTeleportSequence;
}

FVector AVREPawn::GetVRForwardVector() const
{
	return GetVRRotation().Vector();
}

FVector AVREPawn::GetVRRightVector() const
{
	return FRotationMatrix(GetVRRotation()).GetScaledAxis(EAxis::Y);
}

FVector AVREPawn::GetVRUpVector() const
{
	return FRotationMatrix(GetVRRotation()).GetScaledAxis(EAxis::Z);
}

FVector AVREPawn::GetVRLocation() const
{
	return NetSmoother ? NetSmoother->GetComponentLocation() : GetActorLocation();
}

FRotator AVREPawn::GetVRRotation() const
{
	return NetSmoother ? NetSmoother->GetComponentRotation() : GetActorRotation();
}

FVector AVREPawn::GetProjectedVRLocation() const
{
	const FVector VRLocation = GetVRLocation();
	const FVector HeadLocation = GetVRHeadLocation();
	return FVector(HeadLocation.X, HeadLocation.Y, VRLocation.Z);
}

FVector AVREPawn::GetVRHeadLocation() const
{
	return VRReplicatedCamera ? VRReplicatedCamera->GetComponentLocation() : GetVRLocation();
}

void AVREPawn::RegenerateOffsetComponentToWorld(bool, bool)
{
	OffsetComponentToWorld = NetSmoother ? NetSmoother->GetComponentTransform() : GetActorTransform();
}

FVector AVREPawn::AddActorWorldRotationVR(FRotator DeltaRot, bool bUseYawOnly, bool bRotateAroundHead)
{
	AController* OwningController = GetController();
	const FVector OrigLocation = GetActorLocation();
	FVector PivotPoint = bRotateAroundHead ? GetActorTransform().InverseTransformPosition(GetProjectedVRLocation()) : FVector::ZeroVector;
	PivotPoint.Z = 0.0f;

	FRotator NewRotation = bUseControllerRotationYaw && OwningController ? OwningController->GetControlRotation() : GetActorRotation();

	if (bUseYawOnly)
	{
		NewRotation.Pitch = 0.0f;
		NewRotation.Roll = 0.0f;
	}

	FVector NewLocation = OrigLocation + NewRotation.RotateVector(PivotPoint);
	NewRotation = (NewRotation.Quaternion() * DeltaRot.Quaternion()).Rotator();
	NewLocation -= NewRotation.RotateVector(PivotPoint);

	if (bUseControllerRotationYaw && OwningController)
	{
		OwningController->SetControlRotation(NewRotation);
	}

	SetActorLocationAndRotation(NewLocation, NewRotation);
	RegenerateOffsetComponentToWorld();
	return NewLocation - OrigLocation;
}

FVector AVREPawn::SetActorRotationVR(FRotator NewRot, bool bUseYawOnly, bool bAccountForHMDRotation, bool bRotateAroundHead)
{
	AController* OwningController = GetController();
	const FVector OrigLocation = GetActorLocation();
	FVector PivotPoint = bRotateAroundHead ? GetActorTransform().InverseTransformPosition(GetProjectedVRLocation()) : FVector::ZeroVector;
	PivotPoint.Z = 0.0f;

	const FRotator OrigRotation = bUseControllerRotationYaw && OwningController ? OwningController->GetControlRotation() : GetActorRotation();

	if (bUseYawOnly)
	{
		NewRot.Pitch = 0.0f;
		NewRot.Roll = 0.0f;
	}

	FRotator NewRotation;
	if (bAccountForHMDRotation && VRReplicatedCamera)
	{
		NewRotation = UVRExpansionFunctionLibrary::GetHMDPureYaw_I(VRReplicatedCamera->GetRelativeRotation());
		NewRotation = (NewRot.Quaternion() * NewRotation.Quaternion().Inverse()).Rotator();
	}
	else
	{
		NewRotation = NewRot;
	}

	FVector NewLocation = OrigLocation + OrigRotation.RotateVector(PivotPoint);
	NewLocation -= NewRotation.RotateVector(PivotPoint);

	if (bUseControllerRotationYaw && OwningController)
	{
		OwningController->SetControlRotation(NewRotation);
	}

	SetActorLocationAndRotation(NewLocation, NewRotation);
	RegenerateOffsetComponentToWorld();
	return NewLocation - OrigLocation;
}

FVector AVREPawn::SetActorLocationAndRotationVR(FVector NewLoc, FRotator NewRot, bool bUseYawOnly, bool bAccountForHMDRotation, bool bTeleport, bool bSetHeadLocation)
{
	AController* OwningController = GetController();
	FVector PivotPoint = bSetHeadLocation ? GetActorTransform().InverseTransformPosition(GetProjectedVRLocation()) : FVector::ZeroVector;
	PivotPoint.Z = 0.0f;

	if (bUseYawOnly)
	{
		NewRot.Pitch = 0.0f;
		NewRot.Roll = 0.0f;
	}

	FRotator NewRotation;
	if (bAccountForHMDRotation && VRReplicatedCamera)
	{
		NewRotation = UVRExpansionFunctionLibrary::GetHMDPureYaw_I(VRReplicatedCamera->GetRelativeRotation());
		NewRotation = (NewRot.Quaternion() * NewRotation.Quaternion().Inverse()).Rotator();
	}
	else
	{
		NewRotation = NewRot;
	}

	const FVector NewLocation = NewLoc - NewRotation.RotateVector(PivotPoint);

	if (bUseControllerRotationYaw && OwningController)
	{
		OwningController->SetControlRotation(NewRotation);
	}

	SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, bTeleport ? ETeleportType::TeleportPhysics : ETeleportType::None);
	RegenerateOffsetComponentToWorld();
	return NewLocation - NewLoc;
}

FVector AVREPawn::SetActorLocationVR(FVector NewLoc, bool bTeleport, bool bSetHeadLocation)
{
	FVector PivotOffsetVal = bSetHeadLocation ? GetProjectedVRLocation() - GetActorLocation() : FVector::ZeroVector;
	PivotOffsetVal.Z = 0.0f;

	const FVector NewLocation = NewLoc - PivotOffsetVal;
	SetActorLocation(NewLocation, false, nullptr, bTeleport ? ETeleportType::TeleportPhysics : ETeleportType::None);
	RegenerateOffsetComponentToWorld();
	return NewLocation - NewLoc;
}

FVector AVREPawn::GetTargetHeightOffset()
{
	return FVector::ZeroVector;
}

FVector AVREPawn::GetTeleportLocation(FVector OriginalLocation)
{
	return OriginalLocation;
}

void AVREPawn::NotifyOfTeleport(bool bRegisterAsTeleport)
{
	if (GetNetMode() < ENetMode::NM_Client)
	{
		if (bRegisterAsTeleport)
		{
			++ReplicatedTeleportState.TeleportSequence;
		}
		else
		{
			++ReplicatedTeleportState.GripTeleportSequence;
		}

		ForceNetUpdate();
	}

	if (LeftMotionController)
	{
		LeftMotionController->bIsPostTeleport = true;
	}

	if (RightMotionController)
	{
		RightMotionController->bIsPostTeleport = true;
	}

	if (bRegisterAsTeleport)
	{
		OnPawnTeleported_Bind.Broadcast();
		OnCharacterTeleported_Bind.Broadcast();
	}
}
