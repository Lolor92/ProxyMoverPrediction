// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProxyMoverPrediction : ModuleRules
{
	public ProxyMoverPrediction(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"Mover",
			"MotionWarping"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
