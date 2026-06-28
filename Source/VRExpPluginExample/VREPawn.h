// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "VREPawn.generated.h"

class UGripMotionControllerComponent;
class UParentRelativeAttachmentComponent;
class UReplicatedVRCameraComponent;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVREPawnTeleportedSignature);

UCLASS(Blueprintable, BlueprintType)
class VREXPPLUGINEXAMPLE_API AVREPawn : public APawn
{
	GENERATED_BODY()

public:
	AVREPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostInitializeComponents() override;
	virtual bool TeleportTo(const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest = false, bool bNoCheck = false) override;
	virtual FVector GetTargetLocation(AActor* RequestedBy) const override;

	// HMD (头戴显示器) and controller (控制器) tracking stays Pawn-relative (相对Pawn) for large-room VR (大空间VR).
	UPROPERTY(Category = VREPawn, EditAnywhere, BlueprintReadOnly)
	bool bRetainRoomscale = true;

	UPROPERTY(BlueprintAssignable, Category = "VREPawn|VRMovement")
	FVREPawnTeleportedSignature OnPawnTeleported_Bind;

	UPROPERTY(BlueprintAssignable, Category = "VREPawn|VRMovement")
	FVREPawnTeleportedSignature OnCharacterTeleported_Bind;

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
	void RegenerateOffsetComponentToWorld();

	UFUNCTION(BlueprintCallable, Category = "VREPawn|VRLocations")
	FVector AddActorWorldRotationVR(FRotator DeltaRot, bool bUseYawOnly = true, bool bRotateAroundHead = true);

	UFUNCTION(BlueprintCallable, Category = "VREPawn|VRLocations")
	FVector SetActorRotationVR(FRotator NewRot, bool bUseYawOnly = true, bool bAccountForHMDRotation = true, bool bRotateAroundHead = true);

	UFUNCTION(BlueprintCallable, Category = "VREPawn|VRLocations")
	FVector SetActorLocationAndRotationVR(FVector NewLoc, FRotator NewRot, bool bUseYawOnly = true, bool bAccountForHMDRotation = true, bool bTeleport = false, bool bSetHeadLocation = true);

	UFUNCTION(BlueprintCallable, Category = "VREPawn|VRLocations")
	FVector SetActorLocationVR(FVector NewLoc, bool bTeleport, bool bSetHeadLocation = true);

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
};
