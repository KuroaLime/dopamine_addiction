// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DA : ModuleRules
{
	public DA(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"DA",
			"DA/Variant_Platforming",
			"DA/Variant_Platforming/Animation",
			"DA/Variant_Combat",
			"DA/Variant_Combat/AI",
			"DA/Variant_Combat/Animation",
			"DA/Variant_Combat/Gameplay",
			"DA/Variant_Combat/Interfaces",
			"DA/Variant_Combat/UI",
			"DA/Variant_SideScrolling",
			"DA/Variant_SideScrolling/AI",
			"DA/Variant_SideScrolling/Gameplay",
			"DA/Variant_SideScrolling/Interfaces",
			"DA/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
