// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class D_A : ModuleRules
{
	public D_A(ReadOnlyTargetRules Target) : base(Target)
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
			"D_A",
			"D_A/Variant_Platforming",
			"D_A/Variant_Platforming/Animation",
			"D_A/Variant_Combat",
			"D_A/Variant_Combat/AI",
			"D_A/Variant_Combat/Animation",
			"D_A/Variant_Combat/Gameplay",
			"D_A/Variant_Combat/Interfaces",
			"D_A/Variant_Combat/UI",
			"D_A/Variant_SideScrolling",
			"D_A/Variant_SideScrolling/AI",
			"D_A/Variant_SideScrolling/Gameplay",
			"D_A/Variant_SideScrolling/Interfaces",
			"D_A/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
