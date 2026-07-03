// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VRExpansionExtensions : ModuleRules
{
	public VRExpansionExtensions(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"HeadMountedDisplay",
			"VRExpansionPlugin"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"XRBase"
		});
	}
}
