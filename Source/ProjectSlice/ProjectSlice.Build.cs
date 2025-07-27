// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProjectSlice : ModuleRules
{
	public ProjectSlice(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "PhysicsCore",
			"InputCore","EnhancedInput",
			"CableComponent",
			"Niagara",
			"GeometryCollectionEngine", "FieldSystemEngine", 
			"ProceduralMeshComponent", 
			"GeometryCore", "GeometryScriptingCore", "GeometryFramework", "GeometryAlgorithms",
			"DynamicMesh"});
		
		PrivateDependencyModuleNames.AddRange(new string[] 
		{
			"AITestSuite"
		});
	}
}
