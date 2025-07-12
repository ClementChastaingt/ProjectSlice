// Fill out your copyright notice in the Description page of Project Settings.

#include "PSFL_GeometryScript.h"

#include "PSFL_CustomProcMesh.h"
#include "Components/BaseDynamicMeshSceneProxy.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Parameterization/MeshDijkstra.h"

using namespace UE::Geometry;

class UProceduralMeshComponent;

// bool UPSFL_GeometryScript::ProjectPointToMeshSurface(const FDynamicMesh3& Mesh, const FDynamicMeshAABBTree3& Spatial, 
// 	const FVector3d& Point, FVector3d& OutProjectedPoint, int32& OutTriangleID)
// {
// 	// D'abord essayer de trouver le triangle le plus proche
// 	double MaxDistance = Mesh.GetBounds().DiagonalLength() * 2.0;
// 	OutTriangleID = Spatial.FindNearestTriangle(Point, MaxDistance);
//     
// 	if (OutTriangleID == FDynamicMesh3::InvalidID)
// 	{
// 		// Fallback : recherche brutale
// 		OutTriangleID = FindNearestTriangleBruteForce(Mesh, Point);
// 	}
//     
// 	if (OutTriangleID == FDynamicMesh3::InvalidID)
// 	{
// 		UE_LOG(LogTemp, Warning, TEXT("Could not find any triangle for point projection"));
// 		return false;
// 	}
//     
// 	// Projeter le point sur le triangle trouvé
// 	OutProjectedPoint = GetClosestPointOnTriangle(Mesh, OutTriangleID, Point);
//     
// 	UE_LOG(LogTemp, Log, TEXT("Projected point %s to %s on triangle %d (distance: %f)"), 
// 		   *Point.ToString(), *OutProjectedPoint.ToString(), OutTriangleID, 
// 		   FVector3d::Distance(Point, OutProjectedPoint));
//     
// 	return true;
// }

bool UPSFL_GeometryScript::ProjectPointToMeshSurface(const FDynamicMesh3& Mesh, const FDynamicMeshAABBTree3& Spatial, 
	const FVector3d& Point, FVector3d& OutProjectedPoint, int32& OutTriangleID)
{
	UE_LOG(LogTemp, Log, TEXT("ProjectPointToMeshSurface - Input Point: %s"), *Point.ToString());
    
	// Utiliser uniquement la recherche brute force pour éviter les problèmes d'arbre spatial
	OutTriangleID = FindNearestTriangleBruteForce(Mesh, Point);
    
	if (OutTriangleID == FDynamicMesh3::InvalidID)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find any triangle for point projection"));
		return false;
	}
    
	// Projeter le point sur le triangle trouvé
	OutProjectedPoint = GetClosestPointOnTriangle(Mesh, OutTriangleID, Point);
    
	double ProjectionDistance = FVector3d::Distance(Point, OutProjectedPoint);
    
	UE_LOG(LogTemp, Log, TEXT("Projected point %s to %s on triangle %d (distance: %f)"), 
		   *Point.ToString(), *OutProjectedPoint.ToString(), OutTriangleID, ProjectionDistance);
    
	return true;
}


void UPSFL_GeometryScript::ComputeGeodesicPath(UMeshComponent* meshComp, const FVector& startPoint, const FVector& endPoint, TArray<FVector>& outPoints)
{
    outPoints.Reset();
    
    if (!IsValid(meshComp))
    {
        UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPath - Invalid mesh component"));
        return;
    }
    
    // Convertir le mesh en DynamicMesh3
    FDynamicMesh3 Mesh;
    if (!ConvertMeshComponentToDynamicMesh(meshComp, Mesh))
    {
        UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPath - Failed to convert mesh"));
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("DynamicMesh - Vertices: %d, Triangles: %d"), Mesh.VertexCount(), Mesh.TriangleCount());
    
    // Transformer les points en coordonnées locales
    FTransform ComponentTransform = meshComp->GetComponentTransform();
    FVector3d LocalStart = ComponentTransform.InverseTransformPosition(startPoint);
    FVector3d LocalEnd = ComponentTransform.InverseTransformPosition(endPoint);
    
    UE_LOG(LogTemp, Log, TEXT("Original - World Start: %s, World End: %s"), *startPoint.ToString(), *endPoint.ToString());
    UE_LOG(LogTemp, Log, TEXT("Transformed - Local Start: %s, Local End: %s"), *LocalStart.ToString(), *LocalEnd.ToString());
    
    // Créer l'arbre spatial
    FDynamicMeshAABBTree3 Spatial(&Mesh);
    
    // Projeter les points sur la surface
    FVector3d ProjectedStart, ProjectedEnd;
    int32 StartTriangleID, EndTriangleID;
    
    if (!ProjectPointToMeshSurface(Mesh, Spatial, LocalStart, ProjectedStart, StartTriangleID) ||
        !ProjectPointToMeshSurface(Mesh, Spatial, LocalEnd, ProjectedEnd, EndTriangleID))
    {
        UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPath - Failed to project points to mesh surface"));
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("Projected - Start: %s (Tri: %d), End: %s (Tri: %d)"), 
           *ProjectedStart.ToString(), StartTriangleID, *ProjectedEnd.ToString(), EndTriangleID);
    
    // Trouver les vertices les plus proches
    int32 StartVID = FindNearestVertex(Mesh, ProjectedStart);
    int32 EndVID = FindNearestVertex(Mesh, ProjectedEnd);
    
    UE_LOG(LogTemp, Log, TEXT("Nearest vertices - Start: %d, End: %d"), StartVID, EndVID);
    
    if (StartVID == FDynamicMesh3::InvalidID || EndVID == FDynamicMesh3::InvalidID)
    {
        UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPath - Invalid vertex IDs"));
        return;
    }
    
    // Vérifier si les vertices sont identiques
    if (StartVID == EndVID)
    {
        UE_LOG(LogTemp, Log, TEXT("ComputeGeodesicPath - Start and end vertices are the same"));
        outPoints.Add(ComponentTransform.TransformPosition(Mesh.GetVertex(StartVID)));
        return;
    }
    
    // Vérifier la connectivité avant de lancer Dijkstra
    bool bIsConnected = false;
    
    // Faire un BFS simple pour vérifier la connectivité
    TSet<int32> Visited;
    TQueue<int32> Queue;
    Queue.Enqueue(StartVID);
    Visited.Add(StartVID);

	UE_LOG(LogTemp, Log, TEXT("Starting connectivity check from vertex %d to vertex %d"), StartVID, EndVID);
	UE_LOG(LogTemp, Log, TEXT("StartVID position: %s"), *Mesh.GetVertex(StartVID).ToString());
	UE_LOG(LogTemp, Log, TEXT("EndVID position: %s"), *Mesh.GetVertex(EndVID).ToString());
    
    int32 MaxSearchDepth = 1000; // Limiter la recherche
    int32 SearchDepth = 0;
    
    while (!Queue.IsEmpty() && SearchDepth < MaxSearchDepth)
    {
        int32 CurrentVID;
        Queue.Dequeue(CurrentVID);
        SearchDepth++;
        
        if (CurrentVID == EndVID)
        {
            bIsConnected = true;
        	UE_LOG(LogTemp, Log, TEXT("Found connection at depth %d"), SearchDepth);
            break;
        }
        
        // Ajouter les voisins
    	int32 NeighborCount = 0;
        for (int32 NeighborVID : Mesh.VtxVerticesItr(CurrentVID))
        {
        	NeighborCount++;
            if (!Visited.Contains(NeighborVID))
            {
                Visited.Add(NeighborVID);
                Queue.Enqueue(NeighborVID);
            }
        }

    	if (SearchDepth < 10) // Log seulement les premiers pour éviter le spam
    	{
    		UE_LOG(LogTemp, Log, TEXT("Vertex %d has %d neighbors"), CurrentVID, NeighborCount);
    	}
    }
    
    UE_LOG(LogTemp, Log, TEXT("Connectivity check - Connected: %s, Visited vertices: %d"), 
           bIsConnected ? TEXT("Yes") : TEXT("No"), Visited.Num());

	// Si pas connecté, afficher les informations sur les vertices
    if (!bIsConnected)
    {
        UE_LOG(LogTemp, Warning, TEXT("Vertices %d and %d are not connected - mesh may have disconnected components"), StartVID, EndVID);

    	UE_LOG(LogTemp, Warning, TEXT("Detailed analysis:"));
    	UE_LOG(LogTemp, Warning, TEXT("Total mesh vertices: %d"), Mesh.VertexCount());
    	UE_LOG(LogTemp, Warning, TEXT("StartVID %d neighbors:"), StartVID);

    	int32 StartNeighborCount = 0;
    	for (int32 NeighborVID : Mesh.VtxVerticesItr(StartVID))
    	{
    		StartNeighborCount++;
    		UE_LOG(LogTemp, Warning, TEXT("  Neighbor %d: %s"), NeighborVID, *Mesh.GetVertex(NeighborVID).ToString());
    	}
        
    	UE_LOG(LogTemp, Warning, TEXT("EndVID %d neighbors:"), EndVID);
    	int32 EndNeighborCount = 0;
    	for (int32 NeighborVID : Mesh.VtxVerticesItr(EndVID))
    	{
    		EndNeighborCount++;
    		UE_LOG(LogTemp, Warning, TEXT("  Neighbor %d: %s"), NeighborVID, *Mesh.GetVertex(NeighborVID).ToString());
    	}
        
    	UE_LOG(LogTemp, Warning, TEXT("StartVID has %d neighbors, EndVID has %d neighbors"), StartNeighborCount, EndNeighborCount);
    	
        // CORRECTION: Fallback avec une ligne droite interpolée
        // Utiliser les points originaux en coordonnées monde, pas les points projetés
        const int32 NumIntermediatePoints = 10; // Nombre de points intermédiaires
        outPoints.Reserve(NumIntermediatePoints + 2);
        
        // Ajouter le point de départ
        outPoints.Add(startPoint);
        
        // Ajouter des points intermédiaires
        for (int32 i = 1; i <= NumIntermediatePoints; ++i)
        {
            float Alpha = (float)i / (float)(NumIntermediatePoints + 1);
            FVector IntermediatePoint = FMath::Lerp(startPoint, endPoint, Alpha);
            outPoints.Add(IntermediatePoint);
        }
        
        // Ajouter le point d'arrivée
        outPoints.Add(endPoint);
        
        UE_LOG(LogTemp, Log, TEXT("ComputeGeodesicPath - Generated straight line fallback with %d points"), outPoints.Num());
        return;
    }
    
    // Maintenant lancer Dijkstra avec la bonne syntaxe
    TMeshDijkstra<FDynamicMesh3> Dijkstra(&Mesh);
    
    // Créer le tableau de seed points
    TArray<TMeshDijkstra<FDynamicMesh3>::FSeedPoint> SeedPoints;
    SeedPoints.Add(TMeshDijkstra<FDynamicMesh3>::FSeedPoint(StartVID, 0.0));
    
    // Lancer le calcul vers le vertex cible
    Dijkstra.ComputeToTargetPoint(SeedPoints, EndVID);
    
    // Vérifier si le calcul a réussi
    if (!Dijkstra.HasDistance(EndVID))
    {
        UE_LOG(LogTemp, Warning, TEXT("Dijkstra failed to find path from %d to %d"), StartVID, EndVID);
        
        // Fallback: créer une ligne droite interpolée
        const int32 NumIntermediatePoints = 10;
        outPoints.Reserve(NumIntermediatePoints + 2);
        
        outPoints.Add(startPoint);
        for (int32 i = 1; i <= NumIntermediatePoints; ++i)
        {
            float Alpha = (float)i / (float)(NumIntermediatePoints + 1);
            FVector IntermediatePoint = FMath::Lerp(startPoint, endPoint, Alpha);
            outPoints.Add(IntermediatePoint);
        }
        outPoints.Add(endPoint);
        
        UE_LOG(LogTemp, Log, TEXT("ComputeGeodesicPath - Generated Dijkstra fallback with %d points"), outPoints.Num());
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("Dijkstra completed - Distance to end: %f"), Dijkstra.GetDistance(EndVID));
    
    // Reconstruire le chemin
    TArray<int32> Path;
    if (!ReconstructPath(Dijkstra, Mesh, StartVID, EndVID, Path))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to reconstruct path"));
        
        // Fallback: créer une ligne droite interpolée
        const int32 NumIntermediatePoints = 10;
        outPoints.Reserve(NumIntermediatePoints + 2);
        
        outPoints.Add(startPoint);
        for (int32 i = 1; i <= NumIntermediatePoints; ++i)
        {
            float Alpha = (float)i / (float)(NumIntermediatePoints + 1);
            FVector IntermediatePoint = FMath::Lerp(startPoint, endPoint, Alpha);
            outPoints.Add(IntermediatePoint);
        }
        outPoints.Add(endPoint);
        
        UE_LOG(LogTemp, Log, TEXT("ComputeGeodesicPath - Generated reconstruction fallback with %d points"), outPoints.Num());
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("Path reconstructed with %d vertices"), Path.Num());
    
    // Convertir le chemin en coordonnées mondiales
    outPoints.Reserve(Path.Num());
    for (int32 VID : Path)
    {
        FVector3d LocalPos = Mesh.GetVertex(VID);
        FVector WorldPos = ComponentTransform.TransformPosition(LocalPos);
        outPoints.Add(WorldPos);
    }
    
    UE_LOG(LogTemp, Log, TEXT("ComputeGeodesicPath completed - Generated %d points"), outPoints.Num());
}

bool UPSFL_GeometryScript::ReconstructPath(const TMeshDijkstra<FDynamicMesh3>& Dijkstra, const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath)
{
    OutPath.Reset();
    
    if (StartVID == EndVID)
    {
        OutPath.Add(StartVID);
        UE_LOG(LogTemp, Log, TEXT("ReconstructPath - Start and End are the same vertex: %d"), StartVID);
        return true;
    }
    
    // Vérifier que les vertices existent
    if (!Mesh.IsVertex(StartVID) || !Mesh.IsVertex(EndVID))
    {
        UE_LOG(LogTemp, Warning, TEXT("ReconstructPath - Invalid vertices: Start=%d, End=%d"), StartVID, EndVID);
        return false;
    }
    
    // Vérifier que Dijkstra a calculé la distance vers EndVID
    if (!Dijkstra.HasDistance(EndVID))
    {
        UE_LOG(LogTemp, Warning, TEXT("ReconstructPath - No path found from %d to %d"), StartVID, EndVID);
        return false;
    }
    
    // Reconstruire le chemin en remontant les prédécesseurs
    TArray<int32> ReversePath;
    int32 CurrentVID = EndVID;
    
    // Sécurité pour éviter les boucles infinies
    int32 MaxIterations = Mesh.VertexCount();
    int32 Iterations = 0;
    
    while (CurrentVID != StartVID && Iterations < MaxIterations)
    {
        ReversePath.Add(CurrentVID);
        
        // Obtenir le vertex précédent
        int32 PreviousVID = FDynamicMesh3::InvalidID;
        
        // Parcourir les voisins pour trouver celui qui mène au chemin le plus court
        double MinDist = TNumericLimits<double>::Max();
        
        for (int32 NeighborVID : Mesh.VtxVerticesItr(CurrentVID))
        {
            if (Dijkstra.HasDistance(NeighborVID))
            {
                double NeighborDist = Dijkstra.GetDistance(NeighborVID);
                double EdgeLength = FVector3d::Distance(Mesh.GetVertex(CurrentVID), Mesh.GetVertex(NeighborVID));
                
                // Vérifier si ce voisin est sur le chemin optimal
                if (FMath::IsNearlyEqual(NeighborDist + EdgeLength, Dijkstra.GetDistance(CurrentVID), 1e-6))
                {
                    if (NeighborDist < MinDist)
                    {
                        MinDist = NeighborDist;
                        PreviousVID = NeighborVID;
                    }
                }
            }
        }
        
        if (PreviousVID == FDynamicMesh3::InvalidID)
        {
            UE_LOG(LogTemp, Warning, TEXT("ReconstructPath - Failed to find predecessor for vertex %d"), CurrentVID);
            return false;
        }
        
        CurrentVID = PreviousVID;
        Iterations++;
    }
    
    if (Iterations >= MaxIterations)
    {
        UE_LOG(LogTemp, Warning, TEXT("ReconstructPath - Maximum iterations reached, possible infinite loop"));
        return false;
    }
    
    // Ajouter le vertex de départ
    ReversePath.Add(StartVID);
    
    // Inverser le chemin pour avoir l'ordre correct
    OutPath.Reserve(ReversePath.Num());
    for (int32 i = ReversePath.Num() - 1; i >= 0; i--)
    {
        OutPath.Add(ReversePath[i]);
    }
    
    UE_LOG(LogTemp, Log, TEXT("ReconstructPath - Found path with %d vertices: Start=%d, End=%d"), 
           OutPath.Num(), StartVID, EndVID);
    
    // Loguer le chemin complet pour déboguer
    FString PathStr = TEXT("Path: ");
    for (int32 i = 0; i < OutPath.Num(); i++)
    {
        PathStr += FString::Printf(TEXT("%d"), OutPath[i]);
        if (i < OutPath.Num() - 1) PathStr += TEXT(" -> ");
    }
    UE_LOG(LogTemp, Log, TEXT("%s"), *PathStr);
    
    return OutPath.Num() > 1;
}

FVector3d UPSFL_GeometryScript::GetClosestPointOnTriangle(const FDynamicMesh3& Mesh, int32 TriangleID, const FVector3d& Point)
{
    if (!Mesh.IsTriangle(TriangleID))
    {
        UE_LOG(LogTemp, Warning, TEXT("GetClosestPointOnTriangle - Triangle %d is not valid"), TriangleID);
        return Point;
    }

    FIndex3i Triangle = Mesh.GetTriangle(TriangleID);
    FVector3d A = Mesh.GetVertex(Triangle.A);
    FVector3d B = Mesh.GetVertex(Triangle.B);
    FVector3d C = Mesh.GetVertex(Triangle.C);

    // Implémentation manuelle de l'algorithme de projection point-triangle
    FVector3d AB = B - A;
    FVector3d AC = C - A;
    FVector3d AP = Point - A;

    // Calculer les produits scalaires
    double d1 = FVector3d::DotProduct(AB, AP);
    double d2 = FVector3d::DotProduct(AC, AP);
    
    // Vérifier si le point est dans la région de A
    if (d1 <= 0.0 && d2 <= 0.0)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("GetClosestPointOnTriangle - Triangle %d: Point closest to vertex A"), TriangleID);
        return A;
    }

    // Vérifier si le point est dans la région de B
    FVector3d BP = Point - B;
    double d3 = FVector3d::DotProduct(AB, BP);
    double d4 = FVector3d::DotProduct(AC, BP);
    if (d3 >= 0.0 && d4 <= d3)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("GetClosestPointOnTriangle - Triangle %d: Point closest to vertex B"), TriangleID);
        return B;
    }

    // Vérifier si le point est dans la région de l'arête AB
    double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0)
    {
        double v = d1 / (d1 - d3);
        FVector3d result = A + v * AB;
        UE_LOG(LogTemp, VeryVerbose, TEXT("GetClosestPointOnTriangle - Triangle %d: Point closest to edge AB"), TriangleID);
        return result;
    }

    // Vérifier si le point est dans la région de C
    FVector3d CP = Point - C;
    double d5 = FVector3d::DotProduct(AB, CP);
    double d6 = FVector3d::DotProduct(AC, CP);
    if (d6 >= 0.0 && d5 <= d6)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("GetClosestPointOnTriangle - Triangle %d: Point closest to vertex C"), TriangleID);
        return C;
    }

    // Vérifier si le point est dans la région de l'arête AC
    double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0)
    {
        double w = d2 / (d2 - d6);
        FVector3d result = A + w * AC;
        UE_LOG(LogTemp, VeryVerbose, TEXT("GetClosestPointOnTriangle - Triangle %d: Point closest to edge AC"), TriangleID);
        return result;
    }

    // Vérifier si le point est dans la région de l'arête BC
    double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0)
    {
        double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        FVector3d result = B + w * (C - B);
        UE_LOG(LogTemp, VeryVerbose, TEXT("GetClosestPointOnTriangle - Triangle %d: Point closest to edge BC"), TriangleID);
        return result;
    }

    // Le point est à l'intérieur du triangle
    double denom = 1.0 / (va + vb + vc);
    double v = vb * denom;
    double w = vc * denom;
    FVector3d result = A + AB * v + AC * w;
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("GetClosestPointOnTriangle - Triangle %d: Point inside triangle"), TriangleID);
    
    double Distance = FVector3d::Distance(Point, result);
    UE_LOG(LogTemp, Log, TEXT("GetClosestPointOnTriangle - Triangle %d: Input=%s -> Output=%s (distance=%f)"), 
           TriangleID, *Point.ToString(), *result.ToString(), Distance);
    
    return result;
}

int32 UPSFL_GeometryScript::FindNearestVertex(FDynamicMesh3& Mesh, const FVector3d& Point)
{
	int32 NearestVID = FDynamicMesh3::InvalidID;
	double MinDistance = TNumericLimits<double>::Max();

	UE_LOG(LogTemp, Log, TEXT("FindNearestVertex - Looking for nearest vertex to: %s"), *Point.ToString());

	for (int32 VID : Mesh.VertexIndicesItr())
	{
		FVector3d VertexPos = Mesh.GetVertex(VID);
		double Distance = FVector3d::DistSquared(Point, VertexPos);
        
		if (Distance < MinDistance)
		{
			MinDistance = Distance;
			NearestVID = VID;
			UE_LOG(LogTemp, Log, TEXT("New nearest vertex: %d at %s (distance: %f)"), VID, *VertexPos.ToString(), FMath::Sqrt(Distance));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("FindNearestVertex - Final result: vertex %d at distance %f"), NearestVID, FMath::Sqrt(MinDistance));
	return NearestVID;
}


int32 UPSFL_GeometryScript::FindNearestTriangleBruteForce(const FDynamicMesh3& Mesh, const FVector3d& Point)
{
    int32 NearestTriID = FDynamicMesh3::InvalidID;
    double MinDistSqr = TNumericLimits<double>::Max();
    int32 ValidTriangleCount = 0;
    
    UE_LOG(LogTemp, Log, TEXT("FindNearestTriangleBruteForce - Point: %s, Total triangles: %d"), 
           *Point.ToString(), Mesh.TriangleCount());

    // Loguer les premiers triangles pour débogage
    int32 DebugCount = 0;
    
    for (int32 TriID : Mesh.TriangleIndicesItr())
    {
        if (!Mesh.IsTriangle(TriID))
        {
            continue;
        }
        
        ValidTriangleCount++;
        
        // Utiliser notre fonction personnalisée
        FVector3d ClosestPoint = GetClosestPointOnTriangle(Mesh, TriID, Point);
        
        // Vérifier que le point est valide
        if (!FMath::IsFinite(ClosestPoint.X) || !FMath::IsFinite(ClosestPoint.Y) || !FMath::IsFinite(ClosestPoint.Z))
        {
            UE_LOG(LogTemp, Warning, TEXT("Triangle %d returned invalid closest point"), TriID);
            continue;
        }
        
        double DistSqr = FVector3d::DistSquared(Point, ClosestPoint);
        
        // Loguer les premiers triangles pour débogage
        if (DebugCount < 10)
        {
            FIndex3i Triangle = Mesh.GetTriangle(TriID);
            FVector3d A = Mesh.GetVertex(Triangle.A);
            FVector3d B = Mesh.GetVertex(Triangle.B);
            FVector3d C = Mesh.GetVertex(Triangle.C);
            
            UE_LOG(LogTemp, Log, TEXT("Triangle %d: A=%s B=%s C=%s"), 
                   TriID, *A.ToString(), *B.ToString(), *C.ToString());
            UE_LOG(LogTemp, Log, TEXT("Triangle %d: Closest=%s, Distance=%f"), 
                   TriID, *ClosestPoint.ToString(), FMath::Sqrt(DistSqr));
        }
        DebugCount++;

        if (DistSqr < MinDistSqr)
        {
            MinDistSqr = DistSqr;
            NearestTriID = TriID;
            
            UE_LOG(LogTemp, Log, TEXT("New nearest triangle found: %d (distance: %f)"), 
                   TriID, FMath::Sqrt(MinDistSqr));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("FindNearestTriangleBruteForce FINAL - Checked %d triangles, nearest: %d (distance: %f)"), 
           ValidTriangleCount, NearestTriID, FMath::Sqrt(MinDistSqr));

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

