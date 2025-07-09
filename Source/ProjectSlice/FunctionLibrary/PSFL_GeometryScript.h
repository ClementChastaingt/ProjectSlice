// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/BaseDynamicMeshSceneProxy.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Parameterization/MeshDijkstra.h"
#include "PSFL_GeometryScript.generated.h"

class UProceduralMeshComponent;

using namespace UE::Geometry;


/**
 * 
 */
UCLASS()
class PROJECTSLICE_API UPSFL_GeometryScript : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//Geometry Script
	static void ComputeGeodesicPath(UMeshComponent* meshComp, const FVector& startPoint, const FVector& endPoint,TArray<FVector>& outPoints);

	static bool ReconstructPath(const TMeshDijkstra<FDynamicMesh3>& Dijkstra, const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath);
	
	static FVector3d GetClosestPointOnTriangle(const FDynamicMesh3& Mesh, int32 TriangleID, const FVector3d& Point);

	static int32 FindNearestVertex(FDynamicMesh3& Mesh, const FVector& Point);

	static bool ConvertProceduralMeshToDynamicMesh(UProceduralMeshComponent* ProcMesh, FDynamicMesh3& OutMesh, int32 SectionIndex = 0);

	static bool ConvertStaticMeshToDynamicMesh(UStaticMeshComponent* StaticMeshComp, FDynamicMesh3& OutMesh, int32 LODIndex = 0);

	static bool ConvertMeshComponentToDynamicMesh(UMeshComponent* MeshComp, FDynamicMesh3& OutMesh, int32 SectionOrLODIndex = 0);


};
