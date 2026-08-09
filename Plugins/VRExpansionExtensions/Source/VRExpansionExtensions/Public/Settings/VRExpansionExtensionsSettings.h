#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HandTracking/VRExpHandTrackingTypes.h"
#include "VRExpansionExtensionsSettings.generated.h"

UCLASS(Config = Engine, DefaultConfig, meta = (DisplayName = "VRExpansion Extensions"))
class VREXPANSIONEXTENSIONS_API UVRExpansionExtensionsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "HandTracking|PalmFacing")
	FVRExpPalmFacingRecognitionSettings PalmFacingRecognitionSettings;

	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }
};
