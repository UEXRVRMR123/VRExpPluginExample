// Copyright Zhang Shunlin. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class CJKFontFallback : ModuleRules
{
    public CJKFontFallback(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "SlateCore"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "PlatformCrypto",
                "PlatformCryptoOpenSSL",
                "Projects",
                "Slate"
            }
        );

        RuntimeDependencies.Add(
            Path.Combine(PluginDirectory, "Resources", "Fonts", "SourceHanSansSC-Regular.otf"),
            StagedFileType.UFS
        );
        RuntimeDependencies.Add(
            Path.Combine(PluginDirectory, "Resources", "Fonts", "PlangothicP1-Regular.ttf"),
            StagedFileType.UFS
        );
        RuntimeDependencies.Add(
            Path.Combine(PluginDirectory, "Resources", "Fonts", "PlangothicP2-Regular.ttf"),
            StagedFileType.UFS
        );

        RuntimeDependencies.Add(
            Path.Combine(PluginDirectory, "Resources", "Licenses", "SourceHanSans-OFL-1.1.txt"),
            StagedFileType.NonUFS
        );
        RuntimeDependencies.Add(
            Path.Combine(PluginDirectory, "Resources", "Licenses", "Plangothic-OFL-1.1.txt"),
            StagedFileType.NonUFS
        );
        RuntimeDependencies.Add(
            Path.Combine(PluginDirectory, "ThirdPartyNotices.txt"),
            StagedFileType.NonUFS
        );
    }
}
