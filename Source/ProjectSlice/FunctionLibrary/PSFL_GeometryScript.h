// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/BaseDynamicMeshSceneProxy.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Parameterization/MeshDijkstra.h"
#include "PSFL_GeometryScript.generated.h"

class UProceduralMeshComponent;

using namespace UE::Geometry;

/**
 * 
 */

// Notre propre nœud pour Dijkstra
struct FMeshDijkstraNode
{
	int32 VertexID;
	float Distance;
	int32 ParentID;
	bool bVisited;
    
	FMeshDijkstraNode()
		: VertexID(FDynamicMesh3::InvalidID)
		, Distance(TNumericLimits<float>::Max())
		, ParentID(FDynamicMesh3::InvalidID)
		, bVisited(false)
	{}
    
	FMeshDijkstraNode(int32 InVertexID)
		: VertexID(InVertexID)
		, Distance(TNumericLimits<float>::Max())
		, ParentID(FDynamicMesh3::InvalidID)
		, bVisited(false)
	{}
};

UCLASS()
class PROJECTSLICE_API UPSFL_GeometryScript : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static void ComputeGeodesicPath(UMeshComponent* meshComp, const FVector& startPoint, const FVector& endPoint, TArray<FVector>& outPoints,const bool bDebug = false);

private:
	static bool ProjectPointToMeshSurface(const FDynamicMesh3& Mesh, const FDynamicMeshAABBTree3& Spatial, const FVector3d& Point, FVector3d& OutProjectedPoint, int32& OutTriangleID);
	
	static bool AreVerticesConnected(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID);

	static bool AreVerticesInSameComponent(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID);

	static bool ReconstructPath(const TMeshDijkstra<FDynamicMesh3>& Dijkstra, const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath);
	
	static void AnalyzeMeshConnectivity(const FDynamicMesh3& Mesh);

	static bool FindPathBetweenComponents(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath);

	static float CalculatePathLength(const FDynamicMesh3& Mesh, const TArray<int32>& Path);

	static bool CreateSurfacePath(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath);
	
	static inline bool _bDebug = false;
	

#pragma region Triangle
	//------------------

private:
	static FVector3d GetClosestPointOnTriangle(const FDynamicMesh3& Mesh, int32 TriangleID, const FVector3d& Point);

	static int32 FindNearestTriangleBruteForce(const FDynamicMesh3& Mesh, const FVector3d& Point);

#pragma endregion Triangle

#pragma region Vertex
	//------------------

private:
	static int32 FindNearestVertex(FDynamicMesh3& Mesh, const FVector3d& Point);
	
	// Fonction pour trouver le nœud non visité avec la distance minimale
	static int32 FindMinDistanceVertex(const TMap<int32, FMeshDijkstraNode>& NodeMap, const TSet<int32>& UnvisitedVertices);

	static TArray<int32> GetVertexComponent(const FDynamicMesh3& Mesh, int32 VertexID);
	
	//------------------
#pragma endregion Vertex

#pragma region MeshConverter
	//------------------
private:
	static bool ConvertProceduralMeshToDynamicMesh(UProceduralMeshComponent* ProcMesh, FDynamicMesh3& OutMesh, int32 SectionIndex = 0);

	static bool ConvertStaticMeshToDynamicMesh(UStaticMeshComponent* StaticMeshComp, FDynamicMesh3& OutMesh, int32 LODIndex = 0);

	static bool ConvertMeshComponentToDynamicMesh(UMeshComponent* MeshComp, FDynamicMesh3& OutMesh, int32 SectionOrLODIndex = 0);

#pragma endregion MeshConverter

#pragma region Dijkstra
	//------------------

private:
	static bool TryDijkstraPath(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, const FTransform& ComponentTransform, TArray<FVector>& outPoints);

	// Notre propre implémentation de Dijkstra
	static bool FindPathDijkstra(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath);
    	
	// Fonction helper pour reconstruire le chemin depuis notre Dijkstra
	static bool ReconstructPathDijkstra(const TMap<int32, FMeshDijkstraNode>& NodeMap, int32 StartVID, int32 EndVID, TArray<int32>& OutPath);
        

#pragma endregion Dijkstra
	

};
