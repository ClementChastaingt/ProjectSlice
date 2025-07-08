// Fill out your copyright notice in the Description page of Project Settings.

#include "PSFL_GeometryScript.h"

#include "PSFL_CustomProcMesh.h"
#include "Components/BaseDynamicMeshSceneProxy.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Parameterization/MeshDijkstra.h"

using namespace UE::Geometry;

class UProceduralMeshComponent;

void UPSFL_GeometryScript::ComputeGeodesicPath(UProceduralMeshComponent* meshComp, const FVector& startPoint, const FVector& endPoint,TArray<FVector>& outPoints)
{
	FDynamicMesh3 dynMesh;
	if (!ConvertProceduralMeshToDynamicMesh(meshComp, dynMesh))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to convert procedural mesh to dynamic mesh."));
		return;
	}

	FTransform LocalToWorld = meshComp->GetComponentTransform();
	FVector LocalStart = LocalToWorld.InverseTransformPosition(startPoint);
	FVector LocalEnd = LocalToWorld.InverseTransformPosition(endPoint);

	FDynamicMeshAABBTree3 Spatial(&dynMesh);

	FAxisAlignedBox3d Bounds = dynMesh.GetBounds();
	FVector3d MeshSize = Bounds.Max - Bounds.Min;
	double MaxDistance = MeshSize.Length() * 0.5;
		
	int32 StartTri = Spatial.FindNearestTriangle(LocalStart, MaxDistance);
	int32 EndTri = Spatial.FindNearestTriangle(LocalEnd, MaxDistance);

	// Vérifier si les triangles ont été trouvés
	if (StartTri == FDynamicMesh3::InvalidID || EndTri == FDynamicMesh3::InvalidID)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to find start or end triangle."));
		return;
	}
	
	FVector3d StartProjected = GetClosestPointOnTriangle(dynMesh, StartTri, LocalStart);
	FVector3d EndProjected = GetClosestPointOnTriangle(dynMesh, EndTri, LocalEnd);

	int32 startVID = FindNearestVertex(dynMesh, StartProjected);
	int32 endVID = FindNearestVertex(dynMesh, EndProjected);

	TMeshDijkstra Dijkstra(&dynMesh);

	// Créer un tableau de points de départ avec le vertex de départ
	TArray<TMeshDijkstra<FDynamicMesh3>::FSeedPoint> SeedPoints;
	SeedPoints.Add(TMeshDijkstra<FDynamicMesh3>::FSeedPoint(startVID, 0.0));

	// Calculer les distances avec une distance maximale
	double ComputeMaxDistance = TNumericLimits<double>::Max();
	Dijkstra.ComputeToMaxDistance(SeedPoints, ComputeMaxDistance);
	
	// Obtenir le chemin le plus court
	TArray<int32> PathVerts;
	if (!ReconstructPath(Dijkstra, dynMesh, startVID, endVID, PathVerts))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to reconstruct path."));
		return;
	}
	
	outPoints.Empty();
	for (int32 VID : PathVerts)
	{
		FVector3d Pos = dynMesh.GetVertex(VID);
		outPoints.Add(LocalToWorld.TransformPosition((FVector)Pos));
	}

	
	UE_LOG(LogTemp, Log, TEXT("Start vertex: %d, End vertex: %d"), startVID, endVID);
	UE_LOG(LogTemp, Log, TEXT("Start distance: %f, End distance: %f"), 
		   Dijkstra.GetDistance(startVID), Dijkstra.GetDistance(endVID));

	UE_LOG(LogTemp, Log, TEXT("Geodesic path computed with %d points."), PathVerts.Num());
	
}


bool UPSFL_GeometryScript::ReconstructPath(const TMeshDijkstra<FDynamicMesh3>& Dijkstra, 
	const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath)
{
	
	OutPath.Empty();
    
	// Vérifier si le vertex de destination est atteignable
	if (!Dijkstra.HasDistance(EndVID))
	{
		return false;
	}
    
	// Reconstruire le chemin en remontant depuis la destination
	TArray<int32> ReversePath;
	int32 CurrentVID = EndVID;
	ReversePath.Add(CurrentVID);
    
	while (CurrentVID != StartVID)
	{
		int32 NextVID = -1;
		double CurrentDistance = Dijkstra.GetDistance(CurrentVID);
		double MinDistance = TNumericLimits<double>::Max();
        
		// Parcourir tous les voisins du vertex actuel
		for (int32 NeighborVID : Mesh.VtxVerticesItr(CurrentVID))
		{
			if (Dijkstra.HasDistance(NeighborVID))
			{
				double NeighborDistance = Dijkstra.GetDistance(NeighborVID);
				// Calculer la distance de l'arête
				double EdgeLength = FVector3d::Distance(Mesh.GetVertex(CurrentVID), Mesh.GetVertex(NeighborVID));
                
				// Vérifier si ce voisin peut être sur le chemin optimal
				if (FMath::IsNearlyEqual(NeighborDistance + EdgeLength, CurrentDistance, 1e-6) && 
					NeighborDistance < MinDistance)
				{
					MinDistance = NeighborDistance;
					NextVID = NeighborVID;
				}
			}
		}
        
		if (NextVID == -1)
		{
			// Pas de chemin trouvé
			return false;
		}
        
		CurrentVID = NextVID;
		ReversePath.Add(CurrentVID);
        
		// Sécurité pour éviter les boucles infinies
		if (ReversePath.Num() > Mesh.MaxVertexID())
		{
			return false;
		}
	}
    
	// Inverser le chemin pour qu'il aille du début à la fin
	OutPath.Reserve(ReversePath.Num());
	for (int32 i = ReversePath.Num() - 1; i >= 0; --i)
	{
		OutPath.Add(ReversePath[i]);
	}
    
	return true;
}



FVector3d UPSFL_GeometryScript::GetClosestPointOnTriangle(const FDynamicMesh3& Mesh, int32 TriangleID, const FVector3d& Point)
{
	if (!Mesh.IsTriangle(TriangleID))
	{
		return Point;
	}

	FIndex3i Triangle = Mesh.GetTriangle(TriangleID);
	FVector3d A = Mesh.GetVertex(Triangle.A);
	FVector3d B = Mesh.GetVertex(Triangle.B);
	FVector3d C = Mesh.GetVertex(Triangle.C);

	// Utiliser la classe DistPoint3Triangle3 pour calculer le point le plus proche
	FDistPoint3Triangle3d DistanceQuery(Point, FTriangle3d(A, B, C));
	
	return DistanceQuery.ClosestTrianglePoint;
}


int32 UPSFL_GeometryScript::FindNearestVertex(FDynamicMesh3& Mesh, const FVector& Point)
{
	double MinDistSqr = FLT_MAX;
	int32 NearestVID = -1;

	for (int32 VID : Mesh.VertexIndicesItr())
	{
		FVector3d Pos = Mesh.GetVertex(VID);
		double DistSqr = FVector3d::DistSquared(Point, Pos);

		if (DistSqr < MinDistSqr)
		{
			MinDistSqr = DistSqr;
			NearestVID = VID;
		}
	}

	return NearestVID;
}

bool UPSFL_GeometryScript::ConvertProceduralMeshToDynamicMesh(UProceduralMeshComponent* ProcMesh, FDynamicMesh3& OutMesh, int32 SectionIndex)
{
	if (!ProcMesh || !ProcMesh->GetProcMeshSection(SectionIndex))
		return false;

	const FProcMeshSection* Section = ProcMesh->GetProcMeshSection(SectionIndex);
	if (!Section)
		return false;

	OutMesh.Clear();

	TMap<int32, int32> IndexMap; // Map from original index to dynamic mesh index

	for (int32 i = 0; i < Section->ProcVertexBuffer.Num(); ++i)
	{
		const FProcMeshVertex& Vertex = Section->ProcVertexBuffer[i];
		int32 NewVID = OutMesh.AppendVertex((FVector3d)Vertex.Position);
		IndexMap.Add(i, NewVID);
	}

	for (int32 i = 0; i + 2 < Section->ProcIndexBuffer.Num(); i += 3)
	{
		int32 Idx0 = IndexMap[Section->ProcIndexBuffer[i]];
		int32 Idx1 = IndexMap[Section->ProcIndexBuffer[i + 1]];
		int32 Idx2 = IndexMap[Section->ProcIndexBuffer[i + 2]];

		OutMesh.AppendTriangle(Idx0, Idx1, Idx2);
	}

	return true;
}

