// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Project_GJ : ModuleRules
{
	public Project_GJ(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
            "EnhancedInput",        // 향상된 입력 시스템용
			"GameplayAbilities",    // GAS 코어
			"GameplayTags",         // 게임플레이 태그
			"GameplayTasks",         // 게임플레이 태스크
            "Niagara",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Project_GJ",
			"Project_GJ/Variant_Strategy",
			"Project_GJ/Variant_Strategy/UI",
			"Project_GJ/Variant_TwinStick",
			"Project_GJ/Variant_TwinStick/AI",
			"Project_GJ/Variant_TwinStick/Gameplay",
			"Project_GJ/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
