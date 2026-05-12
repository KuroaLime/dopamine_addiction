// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Manager : ModuleRules
{
	public Manager(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate",
			"SlateCore",
            "GameplayTags",
            "AnimGraphRuntime",
			"Niagara"
        });

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Manager",
			"Manager/Variant_Platforming",
			"Manager/Variant_Platforming/Animation",
			"Manager/Variant_Combat",
			"Manager/Variant_Combat/AI",
			"Manager/Variant_Combat/Animation",
			"Manager/Variant_Combat/Gameplay",
			"Manager/Variant_Combat/Interfaces",
			"Manager/Variant_Combat/UI",
			"Manager/Variant_SideScrolling",
			"Manager/Variant_SideScrolling/AI",
			"Manager/Variant_SideScrolling/Gameplay",
			"Manager/Variant_SideScrolling/Interfaces",
			"Manager/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
