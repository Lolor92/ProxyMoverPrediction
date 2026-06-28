// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SyncGasMover : ModuleRules
{
	public SyncGasMover(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDefinitions.Add("SGM_SILENCE_MONTAGE_COMPONENT_LOGS=1");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"MotionWarping",
			"Mover"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
