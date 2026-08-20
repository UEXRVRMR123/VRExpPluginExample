#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "HandTracking/AnimNode_ApplyVRExpHandPose.h"
#include "HandTracking/VRExpHandPoseAnimNodeRuntime.h"
#include "HandTracking/VRExpHandPoseRuntimeUtils.h"

namespace
{
bool QuatsAreNearlyEqual(const FQuat& A, const FQuat& B, float Tolerance = KINDA_SMALL_NUMBER)
{
	return A.AngularDistance(B) <= Tolerance;
}

TArray<FTransform> MakeIdentityHandTransforms()
{
	TArray<FTransform> Transforms;
	Transforms.Init(FTransform::Identity, EHandKeypointCount);
	return Transforms;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVRExpHandPoseRuntimeDefaultsTest,
	"VRExpansionExtensions.HandTracking.AnimNode.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpHandPoseRuntimeDefaultsTest::RunTest(const FString& Parameters)
{
	const FAnimNode_ApplyVRExpHandPose Node;
	TestEqual(TEXT("The node defaults to the left hand"), Node.HandType, EVRExpHandType::HandLeft);
	TestFalse(TEXT("The supplied config leaves the wrist pose to the owner by default"), Node.HandConfig.bApplyWristBone);
	TestEqual(TEXT("The supplied config only applies rotations by default"), Node.HandConfig.TransformApplicationMode, EVRExpHandPoseTransformApplicationMode::RotationOnly);
	TestEqual(TEXT("The supplied config exposes every hand keypoint"), Node.HandConfig.BoneMappings.Num(), EHandKeypointCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVRExpHandPoseTransformApplicationModeTest,
	"VRExpansionExtensions.HandTracking.AnimNode.TransformApplicationMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpHandPoseTransformApplicationModeTest::RunTest(const FString& Parameters)
{
	TestFalse(
		TEXT("Rotation-only mode preserves wrist location"),
		FVRExpHandPoseAnimNodeRuntime::ShouldApplyTrackedLocation(
			EVRExpHandPoseTransformApplicationMode::RotationOnly,
			true));
	TestFalse(
		TEXT("Rotation-only mode preserves finger locations"),
		FVRExpHandPoseAnimNodeRuntime::ShouldApplyTrackedLocation(
			EVRExpHandPoseTransformApplicationMode::RotationOnly,
			false));
	TestTrue(
		TEXT("Wrist-location mode applies wrist location"),
		FVRExpHandPoseAnimNodeRuntime::ShouldApplyTrackedLocation(
			EVRExpHandPoseTransformApplicationMode::RotationAndWristLocation,
			true));
	TestFalse(
		TEXT("Wrist-location mode preserves finger locations"),
		FVRExpHandPoseAnimNodeRuntime::ShouldApplyTrackedLocation(
			EVRExpHandPoseTransformApplicationMode::RotationAndWristLocation,
			false));
	TestTrue(
		TEXT("All-locations mode applies finger locations"),
		FVRExpHandPoseAnimNodeRuntime::ShouldApplyTrackedLocation(
			EVRExpHandPoseTransformApplicationMode::RotationAndAllLocations,
			false));

	FVRExpHandRotationAdjustment ComponentAdjustment;
	ComponentAdjustment.bEnableRotationOffset = true;
	ComponentAdjustment.RotationOffset = FRotator(0.0f, 25.0f, 0.0f);
	const FQuat RawComponentRotation = FRotator(10.0f, 5.0f, 0.0f).Quaternion();
	const FQuat ExpectedComponentRotation = (
		RawComponentRotation * ComponentAdjustment.RotationOffset.Quaternion()).GetNormalized();
	TestTrue(
		TEXT("Component rotation adjustment uses the component-space multiplication order"),
		QuatsAreNearlyEqual(
			FVRExpHandPoseRuntimeUtils::ApplyComponentRotationAdjustment(
				RawComponentRotation,
				ComponentAdjustment),
			ExpectedComponentRotation));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVRExpHandPoseMappingSignatureTest,
	"VRExpansionExtensions.HandTracking.AnimNode.MappingSignature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpHandPoseMappingSignatureTest::RunTest(const FString& Parameters)
{
	TMap<EHandKeypoint, FVRExpHandBoneMapping> BoneMappings;
	FVRExpHandBoneMapping& Mapping = BoneMappings.Add(EHandKeypoint::Wrist);
	Mapping.BoneName = TEXT("hand_r");
	const uint32 OriginalSignature = FVRExpHandPoseAnimNodeRuntime::CalculateBoneMappingsSignature(BoneMappings);

	Mapping.BoneName = TEXT("hand_l");
	const uint32 RenamedBoneSignature = FVRExpHandPoseAnimNodeRuntime::CalculateBoneMappingsSignature(BoneMappings);
	TestNotEqual(TEXT("Changing a mapped bone invalidates the runtime cache"), OriginalSignature, RenamedBoneSignature);

	Mapping.RotationAdjustment.bEnableRotationOffset = true;
	Mapping.RotationAdjustment.RotationOffset = FRotator(1.0f, 2.0f, 3.0f);
	const uint32 AdjustedSignature = FVRExpHandPoseAnimNodeRuntime::CalculateBoneMappingsSignature(BoneMappings);
	TestNotEqual(TEXT("Changing rotation correction invalidates the runtime cache"), RenamedBoneSignature, AdjustedSignature);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVRExpHandPoseAxisAndMirrorPolicyTest,
	"VRExpansionExtensions.HandTracking.PoseMath.AxisAndMirrorPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpHandPoseAxisAndMirrorPolicyTest::RunTest(const FString& Parameters)
{
	FVRExpHandBoneAxisSettings ValidAxes;
	ValidAxes.ForwardAxis = EVRExpHandBoneAxis::X;
	ValidAxes.UpAxis = EVRExpHandBoneAxis::Z;
	FQuat AxisCorrection = FQuat::Identity;
	TestTrue(TEXT("Orthogonal forward/up axes produce a correction"), FVRExpHandPoseRuntimeUtils::TryBuildAxisCorrection(ValidAxes, AxisCorrection));
	TestTrue(TEXT("The generated axis correction is normalized"), AxisCorrection.IsNormalized());

	FVRExpHandBoneAxisSettings InvalidAxes;
	InvalidAxes.ForwardAxis = EVRExpHandBoneAxis::Y;
	InvalidAxes.UpAxis = EVRExpHandBoneAxis::NegativeY;
	TestFalse(TEXT("Parallel forward/up axes are rejected"), FVRExpHandPoseRuntimeUtils::TryBuildAxisCorrection(InvalidAxes, AxisCorrection));
	TestTrue(TEXT("An invalid axis pair falls back to identity"), QuatsAreNearlyEqual(AxisCorrection, FQuat::Identity));

	TestFalse(
		TEXT("Mirroring remains disabled when its master switch is off"),
		FVRExpHandPoseRuntimeUtils::ShouldMirror(EVRExpHandType::HandRight, false, true, EVRExpHandType::HandLeft));
	TestTrue(
		TEXT("Automatic mirroring activates when source and target hands differ"),
		FVRExpHandPoseRuntimeUtils::ShouldMirror(EVRExpHandType::HandRight, true, true, EVRExpHandType::HandLeft));
	TestFalse(
		TEXT("Automatic mirroring stays off when source and target hands match"),
		FVRExpHandPoseRuntimeUtils::ShouldMirror(EVRExpHandType::HandLeft, true, true, EVRExpHandType::HandLeft));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVRExpHandPoseComponentSpaceSolveTest,
	"VRExpansionExtensions.HandTracking.PoseMath.ComponentSpaceSolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpHandPoseComponentSpaceSolveTest::RunTest(const FString& Parameters)
{
	TArray<FTransform> WorldTransforms = MakeIdentityHandTransforms();
	const FQuat ChildRotation = FRotator(0.0f, 30.0f, 0.0f).Quaternion();
	WorldTransforms[static_cast<int32>(EHandKeypoint::Wrist)] = FTransform(FQuat::Identity, FVector(60.0f, 0.0f, 0.0f));
	WorldTransforms[static_cast<int32>(EHandKeypoint::IndexProximal)] = FTransform(ChildRotation, FVector(70.0f, 0.0f, 0.0f));

	TArray<FTransform> CurrentComponentTransforms;
	CurrentComponentTransforms.Add(FTransform::Identity);
	CurrentComponentTransforms.Add(FTransform(FQuat::Identity, FVector(10.0f, 0.0f, 0.0f)));

	TArray<FVRExpResolvedHandBoneMapping> Mappings;
	FVRExpResolvedHandBoneMapping& WristMapping = Mappings.AddDefaulted_GetRef();
	WristMapping.HandKeypoint = EHandKeypoint::Wrist;
	WristMapping.BoneIndex = 0;
	WristMapping.ParentIndex = INDEX_NONE;

	FVRExpResolvedHandBoneMapping& ChildMapping = Mappings.AddDefaulted_GetRef();
	ChildMapping.HandKeypoint = EHandKeypoint::IndexProximal;
	ChildMapping.BoneIndex = 1;
	ChildMapping.ParentIndex = 0;

	TArray<FVRExpSolvedHandBoneTransform> SolvedTransforms;
	const bool bSolved = FVRExpHandPoseRuntimeUtils::SolveComponentSpacePose(
		WorldTransforms,
		FTransform(FQuat::Identity, FVector(50.0f, 0.0f, 0.0f)),
		CurrentComponentTransforms,
		Mappings,
		false,
		false,
		EVRExpHandMirrorAxis::Y,
		EVRExpHandMirrorAxis::Y,
		SolvedTransforms);

	TestTrue(TEXT("A valid wrist and child mapping solve successfully"), bSolved);
	TestEqual(TEXT("Both mapped bones produce results"), SolvedTransforms.Num(), 2);
	if (SolvedTransforms.Num() == 2)
	{
		TestTrue(TEXT("The wrist is converted into mesh component space"), SolvedTransforms[0].ComponentTransform.GetLocation().Equals(FVector(10.0f, 0.0f, 0.0f)));
		TestTrue(TEXT("The child is converted into mesh component space"), SolvedTransforms[1].ComponentTransform.GetLocation().Equals(FVector(20.0f, 0.0f, 0.0f)));
		TestTrue(TEXT("The tracked child rotation survives hierarchy reconstruction"), QuatsAreNearlyEqual(SolvedTransforms[1].ComponentTransform.GetRotation(), ChildRotation));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVRExpHandPosePreservedWristRebaseTest,
	"VRExpansionExtensions.HandTracking.PoseMath.PreservedWristRebase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpHandPosePreservedWristRebaseTest::RunTest(const FString& Parameters)
{
	TArray<FTransform> WorldTransforms = MakeIdentityHandTransforms();
	const FTransform TrackedWristTransform(
		FRotator(0.0f, 70.0f, 0.0f).Quaternion(),
		FVector(100.0f, 20.0f, 5.0f));
	const FTransform TrackedChildLocalTransform(
		FRotator(25.0f, 0.0f, 10.0f).Quaternion(),
		FVector(12.0f, 0.0f, 0.0f));
	WorldTransforms[static_cast<int32>(EHandKeypoint::Wrist)] = TrackedWristTransform;
	WorldTransforms[static_cast<int32>(EHandKeypoint::IndexProximal)] = TrackedChildLocalTransform * TrackedWristTransform;

	const FTransform InputWristTransform(
		FRotator(0.0f, -35.0f, 0.0f).Quaternion(),
		FVector(5.0f, 6.0f, 7.0f));
	TArray<FTransform> CurrentComponentTransforms;
	CurrentComponentTransforms.Add(InputWristTransform);
	CurrentComponentTransforms.Add(FTransform(FQuat::Identity, FVector(10.0f, 0.0f, 0.0f)) * InputWristTransform);

	TArray<FVRExpResolvedHandBoneMapping> Mappings;
	FVRExpResolvedHandBoneMapping& WristMapping = Mappings.AddDefaulted_GetRef();
	WristMapping.HandKeypoint = EHandKeypoint::Wrist;
	WristMapping.BoneIndex = 0;
	WristMapping.ParentIndex = INDEX_NONE;

	FVRExpResolvedHandBoneMapping& ChildMapping = Mappings.AddDefaulted_GetRef();
	ChildMapping.HandKeypoint = EHandKeypoint::IndexProximal;
	ChildMapping.BoneIndex = 1;
	ChildMapping.ParentIndex = 0;

	TArray<FVRExpSolvedHandBoneTransform> SolvedTransforms;
	TestTrue(
		TEXT("Finger pose solves while preserving the input wrist"),
		FVRExpHandPoseRuntimeUtils::SolveComponentSpacePose(
			WorldTransforms,
			FTransform::Identity,
			CurrentComponentTransforms,
			Mappings,
			true,
			false,
			EVRExpHandMirrorAxis::Y,
			EVRExpHandMirrorAxis::Y,
			SolvedTransforms));

	TestEqual(TEXT("The preserved wrist is not emitted as a bone modification"), SolvedTransforms.Num(), 1);
	if (SolvedTransforms.Num() == 1)
	{
		const FTransform ExpectedChildTransform = TrackedChildLocalTransform * InputWristTransform;
		TestEqual(TEXT("The remaining result is the tracked finger"), SolvedTransforms[0].HandKeypoint, EHandKeypoint::IndexProximal);
		TestTrue(
			TEXT("The tracked finger local pose is rebased onto the input wrist"),
			SolvedTransforms[0].ComponentTransform.Equals(ExpectedChildTransform));
	}

	TestTrue(
		TEXT("Mirrored finger pose also solves while preserving the input wrist"),
		FVRExpHandPoseRuntimeUtils::SolveComponentSpacePose(
			WorldTransforms,
			FTransform::Identity,
			CurrentComponentTransforms,
			Mappings,
			true,
			true,
			EVRExpHandMirrorAxis::Y,
			EVRExpHandMirrorAxis::Y,
			SolvedTransforms));
	if (SolvedTransforms.Num() == 1)
	{
		FTransform MirroredTrackedWrist = TrackedWristTransform;
		MirroredTrackedWrist.Mirror(EAxis::Y, EAxis::Y);
		MirroredTrackedWrist.NormalizeRotation();
		FTransform MirroredTrackedChild = WorldTransforms[static_cast<int32>(EHandKeypoint::IndexProximal)];
		MirroredTrackedChild.Mirror(EAxis::Y, EAxis::Y);
		MirroredTrackedChild.NormalizeRotation();
		const FTransform ExpectedMirroredChild = MirroredTrackedChild.GetRelativeTransform(MirroredTrackedWrist) * InputWristTransform;
		TestTrue(
			TEXT("The mirrored finger local pose is rebased onto the input wrist"),
			SolvedTransforms[0].ComponentTransform.Equals(ExpectedMirroredChild));
	}

	TArray<FTransform> GappedCurrentTransforms;
	GappedCurrentTransforms.SetNum(3);
	GappedCurrentTransforms[0] = InputWristTransform;
	GappedCurrentTransforms[1] = FTransform(
		FRotator(4.0f, -7.0f, 11.0f).Quaternion(),
		FVector(2.0f, 3.0f, -1.0f)) * InputWristTransform;
	GappedCurrentTransforms[2] = FTransform(
		FRotator(-8.0f, 13.0f, 5.0f).Quaternion(),
		FVector(4.0f, 0.0f, 2.0f)) * GappedCurrentTransforms[1];

	TArray<FVRExpResolvedHandBoneMapping> GappedMappings = Mappings;
	GappedMappings[1].BoneIndex = 2;
	GappedMappings[1].ParentIndex = 1;

	TArray<FVRExpSolvedHandBoneTransform> GappedSolvedTransforms;
	TestTrue(
		TEXT("Preserved wrist solve supports an unmapped intermediary skeleton bone"),
		FVRExpHandPoseRuntimeUtils::SolveComponentSpacePose(
			WorldTransforms,
			FTransform::Identity,
			GappedCurrentTransforms,
			GappedMappings,
			true,
			false,
			EVRExpHandMirrorAxis::Y,
			EVRExpHandMirrorAxis::Y,
			GappedSolvedTransforms));
	TestEqual(TEXT("Only the mapped child is emitted through the skeleton gap"), GappedSolvedTransforms.Num(), 1);
	if (GappedSolvedTransforms.Num() == 1)
	{
		const FTransform ExpectedGappedChild = TrackedChildLocalTransform * InputWristTransform;
		TestEqual(TEXT("Gapped child keypoint is preserved"), GappedSolvedTransforms[0].HandKeypoint, EHandKeypoint::IndexProximal);
		TestEqual(TEXT("Gapped child keeps its mapped target bone"), GappedSolvedTransforms[0].BoneIndex, 2);
		TestTrue(
			TEXT("Gapped child is rebased through the tracked wrist instead of the input intermediary bone"),
			GappedSolvedTransforms[0].ComponentTransform.Equals(ExpectedGappedChild));
	}

	TArray<FVRExpResolvedHandBoneMapping> AdjustedWristGappedMappings = GappedMappings;
	FVRExpHandRotationAdjustment& WristAdjustment = AdjustedWristGappedMappings[0].RotationAdjustment;
	WristAdjustment.bEnableRotationOffset = true;
	WristAdjustment.RotationOffset = FRotator(9.0f, -17.0f, 6.0f);
	WristAdjustment.bEnableAxisAdjustment = true;
	WristAdjustment.AxisSettings.ForwardAxis = EVRExpHandBoneAxis::Y;
	WristAdjustment.AxisSettings.UpAxis = EVRExpHandBoneAxis::Z;

	TArray<FVRExpSolvedHandBoneTransform> AdjustedWristSolvedTransforms;
	TestTrue(
		TEXT("A preserved wrist still contributes its rotation offset and axis adjustment"),
		FVRExpHandPoseRuntimeUtils::SolveComponentSpacePose(
			WorldTransforms,
			FTransform::Identity,
			GappedCurrentTransforms,
			AdjustedWristGappedMappings,
			true,
			false,
			EVRExpHandMirrorAxis::Y,
			EVRExpHandMirrorAxis::Y,
			AdjustedWristSolvedTransforms));
	TestEqual(TEXT("The adjusted preserved wrist is still not emitted"), AdjustedWristSolvedTransforms.Num(), 1);
	if (AdjustedWristSolvedTransforms.Num() == 1)
	{
		FTransform AdjustedTrackedWrist = TrackedWristTransform;
		AdjustedTrackedWrist.SetRotation(FVRExpHandPoseRuntimeUtils::ApplyParentBoneRotationOffset(
			AdjustedTrackedWrist.GetRotation(),
			WristAdjustment));
		AdjustedTrackedWrist.SetRotation(FVRExpHandPoseRuntimeUtils::ApplyAxisAdjustment(
			AdjustedTrackedWrist.GetRotation(),
			WristAdjustment));
		const FTransform ExpectedAdjustedWristChild =
			WorldTransforms[static_cast<int32>(EHandKeypoint::IndexProximal)].GetRelativeTransform(AdjustedTrackedWrist)
			* InputWristTransform;
		TestTrue(
			TEXT("The child is rebased through the fully adjusted tracked wrist"),
			AdjustedWristSolvedTransforms[0].ComponentTransform.Equals(ExpectedAdjustedWristChild));
	}

	Mappings.RemoveAt(0);
	TestFalse(
		TEXT("Preserving the wrist fails safely when Wrist is not mapped"),
		FVRExpHandPoseRuntimeUtils::SolveComponentSpacePose(
			WorldTransforms,
			FTransform::Identity,
			CurrentComponentTransforms,
			Mappings,
			true,
			false,
			EVRExpHandMirrorAxis::Y,
			EVRExpHandMirrorAxis::Y,
			SolvedTransforms));
	TestTrue(TEXT("A failed rebase leaves no stale transforms"), SolvedTransforms.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVRExpHandPoseRotationAdjustmentTest,
	"VRExpansionExtensions.HandTracking.PoseMath.RotationAdjustment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpHandPoseRotationAdjustmentTest::RunTest(const FString& Parameters)
{
	TArray<FTransform> WorldTransforms = MakeIdentityHandTransforms();
	const FQuat TrackedRotation = FRotator(0.0f, 30.0f, 0.0f).Quaternion();
	WorldTransforms[static_cast<int32>(EHandKeypoint::Wrist)] = FTransform::Identity;
	WorldTransforms[static_cast<int32>(EHandKeypoint::IndexProximal)] = FTransform(TrackedRotation, FVector(10.0f, 0.0f, 0.0f));

	TArray<FTransform> CurrentComponentTransforms;
	CurrentComponentTransforms.Add(FTransform::Identity);
	CurrentComponentTransforms.Add(FTransform(FQuat::Identity, FVector(10.0f, 0.0f, 0.0f)));

	TArray<FVRExpResolvedHandBoneMapping> Mappings;
	FVRExpResolvedHandBoneMapping& WristMapping = Mappings.AddDefaulted_GetRef();
	WristMapping.HandKeypoint = EHandKeypoint::Wrist;
	WristMapping.BoneIndex = 0;
	WristMapping.ParentIndex = INDEX_NONE;

	FVRExpResolvedHandBoneMapping& ChildMapping = Mappings.AddDefaulted_GetRef();
	ChildMapping.HandKeypoint = EHandKeypoint::IndexProximal;
	ChildMapping.BoneIndex = 1;
	ChildMapping.ParentIndex = 0;
	ChildMapping.RotationAdjustment.bEnableRotationOffset = true;
	ChildMapping.RotationAdjustment.RotationOffset = FRotator(0.0f, 15.0f, 0.0f);

	TArray<FVRExpSolvedHandBoneTransform> SolvedTransforms;
	TestTrue(
		TEXT("The adjusted pose solves"),
		FVRExpHandPoseRuntimeUtils::SolveComponentSpacePose(
			WorldTransforms,
			FTransform::Identity,
			CurrentComponentTransforms,
			Mappings,
			false,
			false,
			EVRExpHandMirrorAxis::Y,
			EVRExpHandMirrorAxis::Y,
			SolvedTransforms));

	if (SolvedTransforms.Num() == 2)
	{
		const FQuat ExpectedRotation = (FRotator(0.0f, 15.0f, 0.0f).Quaternion() * TrackedRotation).GetNormalized();
		TestTrue(TEXT("The parent-space rotation offset is pre-multiplied"), QuatsAreNearlyEqual(SolvedTransforms[1].ComponentTransform.GetRotation(), ExpectedRotation));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVRExpHandPoseMirrorAndInvalidInputTest,
	"VRExpansionExtensions.HandTracking.PoseMath.MirrorAndInvalidInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVRExpHandPoseMirrorAndInvalidInputTest::RunTest(const FString& Parameters)
{
	TArray<FTransform> WorldTransforms = MakeIdentityHandTransforms();
	FTransform WristTransform(FRotator(10.0f, 20.0f, 30.0f).Quaternion(), FVector(10.0f, 20.0f, 30.0f));
	WorldTransforms[static_cast<int32>(EHandKeypoint::Wrist)] = WristTransform;

	TArray<FTransform> CurrentComponentTransforms;
	CurrentComponentTransforms.Add(FTransform::Identity);
	TArray<FVRExpResolvedHandBoneMapping> Mappings;
	FVRExpResolvedHandBoneMapping& WristMapping = Mappings.AddDefaulted_GetRef();
	WristMapping.HandKeypoint = EHandKeypoint::Wrist;
	WristMapping.BoneIndex = 0;
	WristMapping.ParentIndex = INDEX_NONE;

	TArray<FVRExpSolvedHandBoneTransform> SolvedTransforms;
	TestTrue(
		TEXT("A mirrored wrist pose solves"),
		FVRExpHandPoseRuntimeUtils::SolveComponentSpacePose(
			WorldTransforms,
			FTransform::Identity,
			CurrentComponentTransforms,
			Mappings,
			false,
			true,
			EVRExpHandMirrorAxis::Y,
			EVRExpHandMirrorAxis::Y,
			SolvedTransforms));

	WristTransform.Mirror(EAxis::Y, EAxis::Y);
	WristTransform.NormalizeRotation();
	if (SolvedTransforms.Num() == 1)
	{
		TestTrue(TEXT("The solver mirrors the complete wrist transform"), SolvedTransforms[0].ComponentTransform.Equals(WristTransform));
	}

	WorldTransforms[static_cast<int32>(EHandKeypoint::Wrist)].SetLocation(FVector(NAN, 0.0f, 0.0f));
	TestFalse(
		TEXT("A NaN wrist invalidates the pose"),
		FVRExpHandPoseRuntimeUtils::SolveComponentSpacePose(
			WorldTransforms,
			FTransform::Identity,
			CurrentComponentTransforms,
			Mappings,
			false,
			false,
			EVRExpHandMirrorAxis::Y,
			EVRExpHandMirrorAxis::Y,
			SolvedTransforms));
	TestTrue(TEXT("Invalid input leaves no stale solved transforms"), SolvedTransforms.IsEmpty());
	return true;
}

#endif
