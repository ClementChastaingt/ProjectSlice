// Fill out your copyright notice in the Description page of Project Settings.

#include "PSFL_GeometryScript.h"

#include "PSFL_CustomProcMesh.h"
#include "Components/BaseDynamicMeshSceneProxy.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Parameterization/MeshDijkstra.h"

using namespace UE::Geometry;

class UProceduralMeshComponent;

bool UPSFL_GeometryScript::ProjectPointToMeshSurface(const FDynamicMesh3& Mesh, const FDynamicMeshAABBTree3& Spatial, 
	const FVector3d& Point, FVector3d& OutProjectedPoint, int32& OutTriangleID)
{
	// D'abord essayer de trouver le triangle le plus proche
	double MaxDistance = Mesh.GetBounds().DiagonalLength() * 2.0;
	OutTriangleID = Spatial.FindNearestTriangle(Point, MaxDistance);
    
	if (OutTriangleID == FDynamicMesh3::InvalidID)
	{
		// Fallback : recherche brutale
		OutTriangleID = FindNearestTriangleBruteForce(Mesh, Point);
	}
    
	if (OutTriangleID == FDynamicMesh3::InvalidID)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find any triangle for point projection"));
		return false;
	}
    
	// Projeter le point sur le triangle trouvé
	OutProjectedPoint = GetClosestPointOnTriangle(Mesh, OutTriangleID, Point);
    
	UE_LOG(LogTemp, Log, TEXT("Projected point %s to %s on triangle %d (distance: %f)"), 
		   *Point.ToString(), *OutProjectedPoint.ToString(), OutTriangleID, 
		   FVector3d::Distance(Point, OutProjectedPoint));
    
	return true;
}


void UPSFL_GeometryScript::ComputeGeodesicPath(UMeshComponent* meshComp, const FVector& startPoint, const FVector& endPoint, TArray<FVector>& outPoints)
{
    if (!meshComp)
    {
        UE_LOG(LogTemp, Warning, TEXT("MeshComponent is null."));
        return;
    }

    FDynamicMesh3 dynMesh;
    if (!ConvertMeshComponentToDynamicMesh(meshComp, dynMesh))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to convert procedural mesh to dynamic mesh."));
        return;
    }

    if (dynMesh.VertexCount() == 0 || dynMesh.TriangleCount() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("DynamicMesh is empty"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("DynamicMesh - Vertices: %d, Triangles: %d"), dynMesh.VertexCount(), dynMesh.TriangleCount());

    // Obtenir les transformations
    FTransform LocalToWorld = meshComp->GetComponentTransform();
    FVector3d LocalStart = (FVector3d)LocalToWorld.InverseTransformPosition(startPoint);
    FVector3d LocalEnd = (FVector3d)LocalToWorld.InverseTransformPosition(endPoint);

    UE_LOG(LogTemp, Log, TEXT("Original - World Start: %s, World End: %s"), 
           *startPoint.ToString(), *endPoint.ToString());
    UE_LOG(LogTemp, Log, TEXT("Transformed - Local Start: %s, Local End: %s"), 
           *LocalStart.ToString(), *LocalEnd.ToString());

    // Construire l'arbre spatial
    FDynamicMeshAABBTree3 Spatial(&dynMesh);

    // Projeter les points directement sur la surface du mesh
    FVector3d ProjectedStart, ProjectedEnd;
    int32 StartTri, EndTri;
    
    if (!ProjectPointToMeshSurface(dynMesh, Spatial, LocalStart, ProjectedStart, StartTri))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to project start point to mesh surface"));
        return;
    }
    
    if (!ProjectPointToMeshSurface(dynMesh, Spatial, LocalEnd, ProjectedEnd, EndTri))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to project end point to mesh surface"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Projected - Start: %s (Tri: %d), End: %s (Tri: %d)"), 
           *ProjectedStart.ToString(), StartTri, *ProjectedEnd.ToString(), EndTri);

    // Trouver les vertices les plus proches des points projetés
    int32 startVID = FindNearestVertex(dynMesh, (FVector)ProjectedStart);
    int32 endVID = FindNearestVertex(dynMesh, (FVector)ProjectedEnd);

    UE_LOG(LogTemp, Log, TEXT("Nearest vertices - Start: %d, End: %d"), startVID, endVID);
    
    if (startVID == -1 || endVID == -1)
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid vertex IDs found"));
        return;
    }

    // Si les vertices sont identiques, retourner un chemin simple
    if (startVID == endVID)
    {
        UE_LOG(LogTemp, Log, TEXT("Start and end vertices are the same, returning direct path"));
        outPoints.Empty();
        outPoints.Add(LocalToWorld.TransformPosition((FVector)ProjectedStart));
        outPoints.Add(LocalToWorld.TransformPosition((FVector)ProjectedEnd));
        return;
    }

    // Calculer le chemin géodésique avec Dijkstra
    TMeshDijkstra<FDynamicMesh3> Dijkstra(&dynMesh);

    // Créer un tableau de points de départ
    TArray<TMeshDijkstra<FDynamicMesh3>::FSeedPoint> SeedPoints;
    SeedPoints.Add(TMeshDijkstra<FDynamicMesh3>::FSeedPoint(startVID, 0.0));

    // Calculer les distances
    double ComputeMaxDistance = TNumericLimits<double>::Max();
    Dijkstra.ComputeToMaxDistance(SeedPoints, ComputeMaxDistance);
    
    // Vérifier que le vertex de destination est atteignable
    if (!Dijkstra.HasDistance(endVID))
    {
        UE_LOG(LogTemp, Warning, TEXT("End vertex %d is not reachable from start vertex %d"), endVID, startVID);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Dijkstra computed - Distance to end: %f"), Dijkstra.GetDistance(endVID));

    // Reconstruire le chemin
    TArray<int32> PathVerts;
    if (!ReconstructPath(Dijkstra, dynMesh, startVID, endVID, PathVerts))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to reconstruct path"));
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("Path reconstructed with %d vertices"), PathVerts.Num());

    // Convertir le chemin en positions world
    outPoints.Empty();
    outPoints.Reserve(PathVerts.Num() + 2); // +2 pour les points de départ et d'arrivée projetés
    
    // Ajouter le point de départ projeté
    outPoints.Add(LocalToWorld.TransformPosition((FVector)ProjectedStart));
    
    // Ajouter les vertices du chemin (en excluant le premier et dernier s'ils sont identiques aux points projetés)
    for (int32 i = 0; i < PathVerts.Num(); ++i)
    {
        int32 VID = PathVerts[i];
        FVector3d VertexPos = dynMesh.GetVertex(VID);
        
        // Éviter les doublons avec les points de départ/arrivée
        bool bSkip = false;
        if (i == 0 && FVector3d::DistSquared(VertexPos, ProjectedStart) < 1e-6)
        {
            bSkip = true;
        }
        else if (i == PathVerts.Num() - 1 && FVector3d::DistSquared(VertexPos, ProjectedEnd) < 1e-6)
        {
            bSkip = true;
        }
        
        if (!bSkip)
        {
            outPoints.Add(LocalToWorld.TransformPosition((FVector)VertexPos));
        }
    }
    
    // Ajouter le point d'arrivée projeté
    outPoints.Add(LocalToWorld.TransformPosition((FVector)ProjectedEnd));

    UE_LOG(LogTemp, Log, TEXT("Final geodesic path contains %d points"), outPoints.Num());
}

bool UPSFL_GeometryScript::ReconstructPath(const TMeshDijkstra<FDynamicMesh3>& Dijkstra, 
	const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath)
{
	
	OutPath.Empty();
    
    // Vérifier si le vertex de destination est atteignable
    if (!Dijkstra.HasDistance(EndVID))
    {
        UE_LOG(LogTemp, Warning, TEXT("End vertex %d is not reachable"), EndVID);
        return false;
    }
    
    if (StartVID == EndVID)
    {
        OutPath.Add(StartVID);
        return true;
    }
    
    // Reconstruire le chemin en remontant depuis la destination
    TArray<int32> ReversePath;
    int32 CurrentVID = EndVID;
    ReversePath.Add(CurrentVID);
    
    const double TOLERANCE = 1e-6;
    int32 MaxIterations = Mesh.MaxVertexID() * 2; // Sécurité contre les boucles infinies
    int32 Iterations = 0;
    
    while (CurrentVID != StartVID && Iterations < MaxIterations)
    {
        int32 NextVID = -1;
        double CurrentDistance = Dijkstra.GetDistance(CurrentVID);
        double MinDistance = TNumericLimits<double>::Max();
        
        UE_LOG(LogTemp, VeryVerbose, TEXT("Current vertex %d, distance: %f"), CurrentVID, CurrentDistance);
        
        // Parcourir tous les voisins du vertex actuel
        bool bFoundNext = false;
        for (int32 NeighborVID : Mesh.VtxVerticesItr(CurrentVID))
        {
            if (Dijkstra.HasDistance(NeighborVID))
            {
                double NeighborDistance = Dijkstra.GetDistance(NeighborVID);
                double EdgeLength = FVector3d::Distance(Mesh.GetVertex(CurrentVID), Mesh.GetVertex(NeighborVID));
                
                // Vérifier si ce voisin peut être sur le chemin optimal
                double ExpectedDistance = NeighborDistance + EdgeLength;
                if (FMath::Abs(ExpectedDistance - CurrentDistance) < TOLERANCE && NeighborDistance < MinDistance)
                {
                    MinDistance = NeighborDistance;
                    NextVID = NeighborVID;
                    bFoundNext = true;
                }
            }
        }
        
        if (!bFoundNext || NextVID == -1)
        {
            UE_LOG(LogTemp, Warning, TEXT("No path found from vertex %d"), CurrentVID);
            return false;
        }
        
        CurrentVID = NextVID;
        ReversePath.Add(CurrentVID);
        Iterations++;
    }
    
    if (Iterations >= MaxIterations)
    {
        UE_LOG(LogTemp, Warning, TEXT("Path reconstruction exceeded maximum iterations"));
        return false;
    }
    
    // Inverser le chemin pour qu'il aille du début à la fin
    OutPath.Reserve(ReversePath.Num());
    for (int32 i = ReversePath.Num() - 1; i >= 0; --i)
    {
        OutPath.Add(ReversePath[i]);
    }
    
    UE_LOG(LogTemp, Log, TEXT("Path reconstructed: %d vertices"), OutPath.Num());
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

int32 UPSFL_GeometryScript::FindNearestTriangleBruteForce(const FDynamicMesh3& Mesh, const FVector3d& Point)
{
	int32 NearestTriID = FDynamicMesh3::InvalidID;
	double MinDistSqr = TNumericLimits<double>::Max();

	for (int32 TriID : Mesh.TriangleIndicesItr())
	{
		FVector3d ClosestPoint = GetClosestPointOnTriangle(Mesh, TriID, Point);
		double DistSqr = FVector3d::DistSquared(Point, ClosestPoint);

		if (DistSqr < MinDistSqr)
		{
			MinDistSqr = DistSqr;
			NearestTriID = TriID;
		}
	}

	return NearestTriID;
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

bool UPSFL_GeometryScript::ConvertStaticMeshToDynamicMesh(UStaticMeshComponent* StaticMeshComp, FDynamicMesh3& OutMesh, int32 LODIndex)
{
	if (!StaticMeshComp || !StaticMeshComp->GetStaticMesh())
	{
		return false;
	}

	UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();
	
	// Vérifier si le LOD demandé existe
	if (LODIndex >= StaticMesh->GetNumLODs())
	{
		LODIndex = 0;
	}

	const FStaticMeshLODResources& LODResource = StaticMesh->GetRenderData()->LODResources[LODIndex];
	
	OutMesh.Clear();

	// Copier les vertices
	const FPositionVertexBuffer& PositionBuffer = LODResource.VertexBuffers.PositionVertexBuffer;
	TMap<int32, int32> IndexMap;
	
	for (uint32 VertexIndex = 0; VertexIndex < PositionBuffer.GetNumVertices(); ++VertexIndex)
	{
		FVector3f Position = PositionBuffer.VertexPosition(VertexIndex);
		int32 NewVID = OutMesh.AppendVertex(FVector3d(Position));
		IndexMap.Add(VertexIndex, NewVID);
	}

	// Copier les triangles
	const FRawStaticIndexBuffer& IndexBuffer = LODResource.IndexBuffer;
	const int32 NumTriangles = IndexBuffer.GetNumIndices() / 3;
	
	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
	{
		const int32 BaseIndex = TriangleIndex * 3;
		const int32 Idx0 = IndexMap[IndexBuffer.GetIndex(BaseIndex)];
		const int32 Idx1 = IndexMap[IndexBuffer.GetIndex(BaseIndex + 1)];
		const int32 Idx2 = IndexMap[IndexBuffer.GetIndex(BaseIndex + 2)];
		
		OutMesh.AppendTriangle(Idx0, Idx1, Idx2);
	}

	return true;
}

bool UPSFL_GeometryScript::ConvertMeshComponentToDynamicMesh(UMeshComponent* MeshComp, FDynamicMesh3& OutMesh, int32 SectionOrLODIndex)
{
	if (!MeshComp)
	{
		return false;
	}

	// Essayer de convertir en tant que UProceduralMeshComponent
	if (UProceduralMeshComponent* ProcMeshComp = Cast<UProceduralMeshComponent>(MeshComp))
	{
		bool bSuccess = ConvertProceduralMeshToDynamicMesh(ProcMeshComp, OutMesh, SectionOrLODIndex);
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("Successfully converted ProceduralMesh - Vertices: %d, Triangles: %d"), 
				   OutMesh.VertexCount(), OutMesh.TriangleCount());
		}
		return bSuccess;
	}
	
	// Essayer de convertir en tant que UStaticMeshComponent
	if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(MeshComp))
	{
		bool bSuccess = ConvertStaticMeshToDynamicMesh(StaticMeshComp, OutMesh, SectionOrLODIndex);
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("Successfully converted StaticMesh - Vertices: %d, Triangles: %d"), OutMesh.VertexCount(), OutMesh.TriangleCount());
		}
		return bSuccess;
	}

	UE_LOG(LogTemp, Warning, TEXT("Unsupported mesh component type: %s"), *MeshComp->GetClass()->GetName());
	return false;
}

