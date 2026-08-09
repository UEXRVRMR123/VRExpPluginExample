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
			"DeveloperSettings",
			"Engine",
			"UnrealEd",
			"VRExpansionExtensions"
		});
	}
}
