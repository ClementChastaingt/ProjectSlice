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

// Notre propre implémentation de pathfinding A* sur mesh
struct FMeshPathNode
{
	int32 VertexID;
	float GCost;     // Distance depuis le début
	float HCost;     // Distance heuristique vers la fin
	float FCost;     // GCost + HCost
	int32 ParentID;  // Vertex parent dans le chemin
    
	FMeshPathNode()
		: VertexID(FDynamicMesh3::InvalidID)
		, GCost(0.0f)
		, HCost(0.0f)
		, FCost(0.0f)
		, ParentID(FDynamicMesh3::InvalidID)
	{}
    
	FMeshPathNode(int32 InVertexID, float InGCost, float InHCost, int32 InParentID)
		: VertexID(InVertexID)
		, GCost(InGCost)
		, HCost(InHCost)
		, FCost(InGCost + InHCost)
		, ParentID(InParentID)
	{}
};

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
	static void ComputeGeodesicPath(UMeshComponent* meshComp, const FVector& startPoint, const FVector& endPoint,TArray<FVector>& outPoints);

private:
	static bool ProjectPointToMeshSurface(const FDynamicMesh3& Mesh, const FDynamicMeshAABBTree3& Spatial, const FVector3d& Point, FVector3d& OutProjectedPoint, int32& OutTriangleID);
	
	static bool AreVerticesConnected(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID);

	static bool ReconstructPath(const TMeshDijkstra<FDynamicMesh3>& Dijkstra, const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath);
	
	static bool AreVerticesInSameComponent(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID);
    
	static void AnalyzeMeshConnectivity(const FDynamicMesh3& Mesh);

	static bool FindPathBetweenComponents(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath);

	static float CalculatePathLength(const FDynamicMesh3& Mesh, const TArray<int32>& Path);

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
	
	static int32 FindClosestVertexInTriangle(const FDynamicMesh3& Mesh, const FVector3d& Point, const FIndex3i& Triangle);

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

#pragma region PathStar
	//------------------

private:
	static bool FindPathAStar(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath);
	
	static bool ReconstructPathAStar(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, const TSet<int32>& VisitedVertices, TArray<int32>& OutPath);
	
	static bool FindPathBFS(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath);

	//------------------
#pragma endregion PathStar

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
