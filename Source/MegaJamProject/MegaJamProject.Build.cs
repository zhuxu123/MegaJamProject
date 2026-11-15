// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MegaJamProject : ModuleRules
{
	public MegaJamProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Paper2D", "AudioExtensions" });

		PrivateDependencyModuleNames.AddRange(new string[] { "AngelscriptRuntime", "Slate", "SlateCore", "UMG", "ImageWrapper", "Json", "AudioMixer" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
