using UnrealBuildTool;

public class VRExpansionExtensionsEditor : ModuleRules
{
	public VRExpansionExtensionsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"AnimGraph",
			"AnimGraphRuntime",
			"BlueprintGraph",
			"DeveloperSettings",
			"Engine",
			"UnrealEd",
			"VRExpansionExtensions"
		});
	}
}
