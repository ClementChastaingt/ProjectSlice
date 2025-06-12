// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProjectSlice : ModuleRules
{
	public ProjectSlice(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "PhysicsCore", "ProceduralMeshComponent", "EnhancedInput", "CableComponent", "GeometryCollectionEngine", "FieldSystemEngine"});
		PrivateDependencyModuleNames.AddRange(new string[] { "CableComponent", "AITestSuite" });
	}
}
