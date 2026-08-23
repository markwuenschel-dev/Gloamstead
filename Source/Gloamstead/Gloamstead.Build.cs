// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Gloamstead : ModuleRules
{
	public Gloamstead(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"PCG"
		});

		// Gloamstead's Cycle II projection is the one-way gameplay-owned caller
		// of WorldForgeCore's generic state mirror. Keep the dependency private:
		// no public Gloamstead header exposes WorldForge types.
		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "AssetRegistry", "WorldForgeCore" });

		PublicIncludePaths.AddRange(new string[] {
			"Gloamstead",
			"Gloamstead/Variant_Platforming",
			"Gloamstead/Variant_Platforming/Animation",
			"Gloamstead/Variant_Combat",
			"Gloamstead/Variant_Combat/AI",
			"Gloamstead/Variant_Combat/Animation",
			"Gloamstead/Variant_Combat/Gameplay",
			"Gloamstead/Variant_Combat/Interfaces",
			"Gloamstead/Variant_Combat/UI",
			"Gloamstead/Variant_SideScrolling",
			"Gloamstead/Variant_SideScrolling/AI",
			"Gloamstead/Variant_SideScrolling/Gameplay",
			"Gloamstead/Variant_SideScrolling/Interfaces",
			"Gloamstead/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
