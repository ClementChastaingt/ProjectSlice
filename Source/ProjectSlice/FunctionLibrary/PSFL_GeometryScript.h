// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"

//Path
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Parameterization/MeshDijkstra.h"

#include "PSFL_GeometryScript.generated.h"

class UProceduralMeshComponent;

using namespace UE::Geometry;

/**
 * 
 */


#pragma region Path
//------------------

// Structure pour identifier un path unique
struct FPathCacheKey
{
	FVector StartPoint = FVector::ZeroVector;
	FVector EndPoint = FVector::ZeroVector;
	TWeakObjectPtr<UMeshComponent> MeshComponent = nullptr;

	// Constructeur par défaut - maintenant défini inline
	FPathCacheKey()
		: StartPoint(FVector::ZeroVector)
		, EndPoint(FVector::ZeroVector)
		, MeshComponent(nullptr)
	{}
    
	FPathCacheKey(const FVector& InStartPoint, const FVector& InEndPoint, UMeshComponent* InMeshComp)
		: StartPoint(InStartPoint), EndPoint(InEndPoint), MeshComponent(InMeshComp){}
    
	bool operator==(const FPathCacheKey& Other) const
	{
		const float Tolerance = 1e-3f; // Tolérance pour la comparaison des positions
		return FVector::DistSquared(StartPoint, Other.StartPoint) < Tolerance * Tolerance &&
			   FVector::DistSquared(EndPoint, Other.EndPoint) < Tolerance * Tolerance &&
			   MeshComponent == Other.MeshComponent;
	}
};

// Structure pour stocker les résultats du cache
struct FPathCacheEntry
{
	TArray<FVector> Points;
	double LastAccessTime;
    
	FPathCacheEntry()
		: LastAccessTime(0.0)
	{}
    
	FPathCacheEntry(const TArray<FVector>& InPoints)
		: Points(InPoints), LastAccessTime(FPlatformTime::Seconds())
	{}
};

// Nouvelle structure pour inclure la vélocité dans le cache
struct FPathCacheKeyWithVelocity
{
	FVector StartPoint = FVector::ZeroVector;
	FVector EndPoint = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float VelocityInfluence = 0.5f;
	TWeakObjectPtr<UMeshComponent> MeshComponent = nullptr;

	FPathCacheKeyWithVelocity() = default;
    
	FPathCacheKeyWithVelocity(const FVector& InStartPoint, const FVector& InEndPoint, const FVector& InVelocity, float InVelocityInfluence, UMeshComponent* InMeshComp)
		: StartPoint(InStartPoint), EndPoint(InEndPoint), Velocity(InVelocity), VelocityInfluence(InVelocityInfluence), MeshComponent(InMeshComp) {}
    
	bool operator==(const FPathCacheKeyWithVelocity& Other) const
	{
		const float Tolerance = 1e-3f;
		return FVector::DistSquared(StartPoint, Other.StartPoint) < Tolerance * Tolerance &&
			   FVector::DistSquared(EndPoint, Other.EndPoint) < Tolerance * Tolerance &&
			   FVector::DistSquared(Velocity, Other.Velocity) < Tolerance * Tolerance &&
			   FMath::Abs(VelocityInfluence - Other.VelocityInfluence) < Tolerance &&
			   MeshComponent == Other.MeshComponent;
	}
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

// Hash function pour FPathCacheKey
inline uint32 GetTypeHash(const FPathCacheKey& Key)
{
	uint32 Hash = 0;
	Hash = HashCombine(Hash, GetTypeHash(Key.StartPoint));
	Hash = HashCombine(Hash, GetTypeHash(Key.EndPoint));
	Hash = HashCombine(Hash, GetTypeHash(Key.MeshComponent.Get()));
	return Hash;
}

//------------------
#pragma endregion Path

#pragma region Hull
//------------------

struct FHullWidthOutData
{
	FVector OutCenter3D;
	FVector2d OutHullLeft;
	FVector2d OutHullRight;
	FFrame3d OutProjectionFrame;
	
	FHullWidthOutData() 
		: OutCenter3D(FVector::ZeroVector)
		, OutHullLeft(FVector2d::ZeroVector)
		, OutHullRight(FVector2d::ZeroVector)
		, OutProjectionFrame(FFrame3d())
	{
	}
	
	FHullWidthOutData(FVector InCenter3D, FVector2d InHullLeft, FVector2d InHullRight, FFrame3d InProjectionFrame)
		: OutCenter3D(InCenter3D)
		, OutHullLeft(InHullLeft)
		, OutHullRight(InHullRight)
		, OutProjectionFrame(InProjectionFrame)
	{
	}

};

//------------------

#pragma endregion Hull

UCLASS()
class PROJECTSLICE_API UPSFL_GeometryScript : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

#pragma region Projection
	//------------------

	
public:
	static void ComputeGeodesicPath(UMeshComponent* meshComp, const FVector& startPoint, const FVector& endPoint, TArray<FVector>& outPoints,const bool bDebug = false, const bool bDebugPoint = false);

	// Version avec vélocité
	static void ComputeGeodesicPathWithVelocity(UMeshComponent* meshComp, const FVector& startPoint, const FVector& endPoint, const FVector& velocity, TArray<FVector>& outPoints, float velocityInfluence = 0.5f, const bool bDebug = false, const bool bDebugPoint = false);

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

	static inline bool _bDebugPoint = false;

#pragma region Cache
	//------------------

public:
	// Fonction pour nettoyer le cache
	UFUNCTION(BlueprintCallable, Category = "Geometry Script")
	static void ClearPathCache();
	
	// Fonction pour définir la taille maximale du cache
	UFUNCTION(BlueprintCallable, Category = "Geometry Script")
	static void SetMaxCacheSize(int32 MaxSize);

private:
	// Cache statique pour stocker les chemins calculés
	static TMap<FPathCacheKey, FPathCacheEntry> PathCache;
	static int32 MaxCacheSize;
	static constexpr double CacheExpirationTime = 300.0; // 5 minutes
	
	// Fonctions de gestion du cache
	static bool TryGetCachedPath(const FPathCacheKey& Key, TArray<FVector>& OutPoints);
	static void StoreCachedPath(const FPathCacheKey& Key, const TArray<FVector>& Points);
	static void CleanupExpiredCache();

	//------------------
#pragma endregion Cache

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

	// Version modifiée de l'algorithme de Dijkstra avec vélocité
	static bool FindPathDijkstraWithVelocity(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, const FVector3d& velocity, float velocityInfluence, TArray<int32>& OutPath);
        

#pragma endregion Dijkstra

//TODO :: Actually Velocity Biased func are WIP 
#pragma region Velocity
	//------------------

private:
	// Calculer le coût d'un edge en tenant compte de la vélocité
	static float CalculateVelocityBiasedCost(const FDynamicMesh3& Mesh, int32 EdgeID, const FVector3d& velocity, float velocityInfluence = 0.5f);
    
	// Calculer le coût entre deux vertices en tenant compte de la vélocité
	static float CalculateVelocityBiasedVertexCost(const FDynamicMesh3& Mesh, int32 FromVertexID, int32 ToVertexID, const FVector3d& velocity, float velocityInfluence = 0.5f);

#pragma endregion Velocity
	
	//------------------
#pragma endregion Projection

#pragma region HullBounds
	//------------------
	
public:
	// Computes the 2D bounds of a Static or Procedural Mesh projected along a direction.
    // Returns 4 corners of the convex projection hull that surrounds the mesh
    // Used to scale a visual triangle from muzzle to projected edges
	static float ComputeProjectedHullWidth(UMeshComponent* MeshComponent, const FVector& ViewDirection, const FVector& SightHitPoint, FHullWidthOutData& OutDatas, bool bDebug);
	
	static FRotator ComputeAdjustedAimDeltaRotator(const FVector& MuzzleLoc, const FVector& HullCenter3D, const FVector& ImpactPoint, const FFrame3d& ProjectionFrame);
	
	static FRotator ComputeAdjustedAimLookAt(const FVector& MuzzleLoc, const FVector& HullCenter, const FVector& ImpactPoint, const FTransform& ReferenceFrame);

	static FRotator ComputeAdjustedAimLookAt_Relative(const FVector& MuzzleWorldLoc, const FVector& HullCenterWorld,
		const FVector& ImpactWorld, const FTransform& MuzzleTransform);

	//------------------
#pragma endregion HullBounds
};
