// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "VREPawn.generated.h"

class AVRPlayerController;
class UGripMotionControllerComponent;
class UParentRelativeAttachmentComponent;
class UReplicatedVRCameraComponent;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVREPawnTeleportedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVREPawnPlayerStateReplicatedSignature, const APlayerState*, NewPlayerState);

USTRUCT(BlueprintType)
struct FVREPawnReplicatedTeleportState
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 TeleportSequence = 0;

	UPROPERTY()
	uint8 GripTeleportSequence = 0;
};

UCLASS(Blueprintable, BlueprintType)
class VREXPPLUGINEXAMPLE_API AVREPawn : public APawn
{
	GENERATED_BODY()

public:
	AVREPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostInitializeComponents() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool TeleportTo(const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest = false, bool bNoCheck = false) override;
	virtual FVector GetTargetLocation(AActor* RequestedBy) const override;

	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<AVRPlayerController> OwningVRPlayerController;

	// HMD (头戴显示器) and controller (控制器) tracking stays Pawn-relative (相对Pawn) for large-room VR (大空间VR).
	UPROPERTY(Category = VREPawn, EditAnywhere, BlueprintReadOnly)
	bool bRetainRoomscale = true;

	UPROPERTY(BlueprintAssignable, Category = "VREPawn|VRMovement")
	FVREPawnTeleportedSignature OnPawnTeleported_Bind;

	UPROPERTY(BlueprintAssignable, Category = "VREPawn|VRMovement")
	FVREPawnTeleportedSignature OnCharacterTeleported_Bind;

	UPROPERTY(BlueprintAssignable, Category = "VREPawn|VRMovement")
	FVREPawnTeleportedSignature OnCharacterNetworkCorrected_Bind;

	UPROPERTY(BlueprintAssignable, Category = "VREPawn|VRMovement")
	FVREPawnPlayerStateReplicatedSignature OnPlayerStateReplicated_Bind;

	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedTeleportState)
	FVREPawnReplicatedTeleportState ReplicatedTeleportState;

	UFUNCTION()
	void OnRep_ReplicatedTeleportState();

	UPROPERTY(BlueprintReadOnly, Transient, Category = "VREPawn|VRLocations")
	FTransform OffsetComponentToWorld;

	UFUNCTION(BlueprintPure, Category = "VREPawn|VRLocations")
	FVector GetVRForwardVector() const;

	UFUNCTION(BlueprintPure, Category = "VREPawn|VRLocations")
	FVector GetVRRightVector() const;

	UFUNCTION(BlueprintPure, Category = "VREPawn|VRLocations")
	FVector GetVRUpVector() const;

	UFUNCTION(BlueprintPure, Category = "VREPawn|VRLocations")
	FVector GetVRLocation() const;

	FVector GetVRLocation_Inline() const
	{
		return GetVRLocation();
	}

	UFUNCTION(BlueprintPure, Category = "VREPawn|VRLocations")
	FRotator GetVRRotation() const;

	UFUNCTION(BlueprintPure, Category = "VREPawn|VRLocations")
	FVector GetProjectedVRLocation() const;

	UFUNCTION(BlueprintPure, Category = "VREPawn|VRLocations", meta = (DisplayName = "GetVRHeadLocation", ScriptName = "GetVRHeadLocation", Keywords = "position"))
	FVector K2_GetVRHeadLocation() const
	{
		return GetVRHeadLocation();
	}

	FVector GetVRHeadLocation() const;

	UFUNCTION(BlueprintCallable, Category = "VREPawn|VRLocations")
	void RegenerateOffsetComponentToWorld(bool bUpdateBounds = true, bool bCalculatePureYaw = true);

	UFUNCTION(BlueprintCallable, Category = "VREPawn|VRLocations")
	FVector AddActorWorldRotationVR(FRotator DeltaRot, bool bUseYawOnly = true, bool bRotateAroundHead = true);

	UFUNCTION(BlueprintCallable, Category = "VREPawn|VRLocations")
	FVector SetActorRotationVR(FRotator NewRot, bool bUseYawOnly = true, bool bAccountForHMDRotation = true, bool bRotateAroundHead = true);

	UFUNCTION(BlueprintCallable, Category = "VREPawn|VRLocations")
	FVector SetActorLocationAndRotationVR(FVector NewLoc, FRotator NewRot, bool bUseYawOnly = true, bool bAccountForHMDRotation = true, bool bTeleport = false, bool bSetHeadLocation = true);

	UFUNCTION(BlueprintCallable, Category = "VREPawn|VRLocations")
	FVector SetActorLocationVR(FVector NewLoc, bool bTeleport, bool bSetHeadLocation = true);

	UFUNCTION(BlueprintPure, Category = "VREPawn|VRLocations")
	virtual FVector GetTargetHeightOffset();

	UFUNCTION(BlueprintPure, Category = "VREPawn|VRGrip")
	virtual FVector GetTeleportLocation(FVector OriginalLocation);

	UFUNCTION(BlueprintCallable, Category = "VREPawn|VRGrip")
	virtual void NotifyOfTeleport(bool bRegisterAsTeleport = true);

	UPROPERTY(Category = VREPawn, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> NetSmoother;

	UPROPERTY(Category = VREPawn, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> VRProxyComponent;

	UPROPERTY(Category = VREPawn, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UReplicatedVRCameraComponent> VRReplicatedCamera;

	UPROPERTY(Category = VREPawn, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UParentRelativeAttachmentComponent> ParentRelativeAttachment;

	UPROPERTY(Category = VREPawn, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGripMotionControllerComponent> LeftMotionController;

	UPROPERTY(Category = VREPawn, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGripMotionControllerComponent> RightMotionController;

	UPROPERTY(Category = VREPawn, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> Mesh;

	static FName LeftMotionControllerComponentName;
	static FName RightMotionControllerComponentName;
	static FName ReplicatedCameraComponentName;
	static FName ParentRelativeAttachmentComponentName;
	static FName SmoothingSceneParentComponentName;
	static FName VRProxyComponentName;
	static FName MeshComponentName;

private:
	UPROPERTY(Transient)
	uint8 LastProcessedTeleportSequence = 0;

	UPROPERTY(Transient)
	uint8 LastProcessedGripTeleportSequence = 0;
};
