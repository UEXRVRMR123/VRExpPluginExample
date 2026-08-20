// Fill out your copyright notice in the Description page of Project Settings.

#include "HandTracking/AnimGraphNode_ApplyVRExpHandPose.h"

#include "Animation/Skeleton.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/CompilerResultsLog.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "VRExpAnimGraphNodeApplyHandPose"

namespace
{
FVector GetAxisVector(EVRExpHandBoneAxis Axis)
{
	switch (Axis)
	{
	case EVRExpHandBoneAxis::X:
		return FVector::ForwardVector;
	case EVRExpHandBoneAxis::Y:
		return FVector::RightVector;
	case EVRExpHandBoneAxis::Z:
		return FVector::UpVector;
	case EVRExpHandBoneAxis::NegativeX:
		return -FVector::ForwardVector;
	case EVRExpHandBoneAxis::NegativeY:
		return -FVector::RightVector;
	case EVRExpHandBoneAxis::NegativeZ:
		return -FVector::UpVector;
	default:
		return FVector::ForwardVector;
	}
}

bool HasValidAxisPair(const FVRExpHandBoneAxisSettings& AxisSettings)
{
	return FMath::Abs(FVector::DotProduct(
		GetAxisVector(AxisSettings.ForwardAxis),
		GetAxisVector(AxisSettings.UpAxis))) <= 1.0f - KINDA_SMALL_NUMBER;
}

void ValidateHandConfig(
	const FText& ConfigLabel,
	const FVRExpHandPoseConfig& Config,
	USkeleton* ForSkeleton,
	FCompilerResultsLog& MessageLog,
	UAnimGraphNode_ApplyVRExpHandPose* GraphNode)
{
	TMap<FName, EHandKeypoint> FirstKeypointByBone;
	int32 MappedBoneCount = 0;
	if (Config.ComponentRotationAdjustment.bEnableAxisAdjustment
		&& !HasValidAxisPair(Config.ComponentRotationAdjustment.AxisSettings))
	{
		MessageLog.Warning(
			*FText::Format(
				LOCTEXT("InvalidComponentRotationAxes", "@@ - {0}: Component Rotation Adjustment has parallel Forward and Up axes; its axis adjustment will be ignored."),
				ConfigLabel).ToString(),
			GraphNode);
	}

	for (const TPair<EHandKeypoint, FVRExpHandBoneMapping>& MappingPair : Config.BoneMappings)
	{
		const FVRExpHandBoneMapping& Mapping = MappingPair.Value;
		if (Mapping.BoneName.IsNone())
		{
			continue;
		}

		++MappedBoneCount;
		if (ForSkeleton && !ForSkeleton->HasAnyFlags(RF_NeedPostLoad)
			&& ForSkeleton->GetReferenceSkeleton().FindBoneIndex(Mapping.BoneName) == INDEX_NONE)
		{
			MessageLog.Error(
				*FText::Format(
					LOCTEXT("MissingMappedBone", "@@ - {0}: mapped bone '{1}' was not found in the target Skeleton."),
					ConfigLabel,
					FText::FromName(Mapping.BoneName)).ToString(),
				GraphNode);
		}

		if (FirstKeypointByBone.Contains(Mapping.BoneName))
		{
			MessageLog.Error(
				*FText::Format(
					LOCTEXT("DuplicateMappedBone", "@@ - {0}: bone '{1}' is mapped by more than one hand keypoint."),
					ConfigLabel,
					FText::FromName(Mapping.BoneName)).ToString(),
				GraphNode);
		}
		else
		{
			FirstKeypointByBone.Add(Mapping.BoneName, MappingPair.Key);
		}

		if (Mapping.RotationAdjustment.bEnableAxisAdjustment
			&& !HasValidAxisPair(Mapping.RotationAdjustment.AxisSettings))
		{
			MessageLog.Warning(
				*FText::Format(
					LOCTEXT("InvalidMappedBoneAxes", "@@ - {0}: bone '{1}' has parallel Forward and Up axes; its axis adjustment will be ignored."),
					ConfigLabel,
					FText::FromName(Mapping.BoneName)).ToString(),
				GraphNode);
		}
	}

	if (MappedBoneCount == 0)
	{
		MessageLog.Warning(
			*FText::Format(
				LOCTEXT("EmptyBoneMappings", "@@ - {0} has no mapped hand keypoints; that hand passes through the input pose."),
				ConfigLabel).ToString(),
			GraphNode);
	}
}
}

void UAnimGraphNode_ApplyVRExpHandPose::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		const UScriptStruct* NodeStruct = FAnimNode_ApplyVRExpHandPose::StaticStruct();
		ShowPinForProperties.RemoveAll([NodeStruct](const FOptionalPinFromProperty& OptionalPin)
		{
			return FindFProperty<FProperty>(NodeStruct, OptionalPin.PropertyName) == nullptr;
		});
	}
}

FText UAnimGraphNode_ApplyVRExpHandPose::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FText UAnimGraphNode_ApplyVRExpHandPose::GetTooltipText() const
{
	return LOCTEXT(
		"ApplyVRExpHandPoseTooltip",
		"Applies the selected hand's tracking snapshot using the supplied Hand Config. It modifies bones only and never moves, hides, or scales the skeletal mesh component.");
}

FLinearColor UAnimGraphNode_ApplyVRExpHandPose::GetNodeTitleColor() const
{
	return FLinearColor(0.10f, 0.55f, 0.85f);
}

FString UAnimGraphNode_ApplyVRExpHandPose::GetNodeCategory() const
{
	return TEXT("VRExp|Hand Tracking");
}

void UAnimGraphNode_ApplyVRExpHandPose::ValidateAnimNodeDuringCompilation(
	USkeleton* ForSkeleton,
	FCompilerResultsLog& MessageLog)
{
	const FName HandConfigPropertyName = GET_MEMBER_NAME_CHECKED(FAnimNode_ApplyVRExpHandPose, HandConfig);
	const UEdGraphPin* HandConfigPin = FindPin(HandConfigPropertyName);
	const bool bHandConfigIsDriven = HasBinding(HandConfigPropertyName)
		|| (HandConfigPin != nullptr && !HandConfigPin->LinkedTo.IsEmpty());
	if (!bHandConfigIsDriven)
	{
		ValidateHandConfig(
			LOCTEXT("HandConfigLabel", "Hand Config"),
			Node.HandConfig,
			ForSkeleton,
			MessageLog,
			this);
	}

	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

FText UAnimGraphNode_ApplyVRExpHandPose::GetControllerDescription() const
{
	return LOCTEXT("ApplyVRExpHandPose", "Apply VRExp Hand Pose");
}

#undef LOCTEXT_NAMESPACE
