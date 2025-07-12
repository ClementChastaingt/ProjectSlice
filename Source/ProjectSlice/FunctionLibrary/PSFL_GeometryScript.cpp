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

// Remplacer la fonction ComputeGeodesicPath pour utiliser une approche différente
// void UPSFL_GeometryScript::ComputeGeodesicPath(UMeshComponent* meshComp, const FVector& startPoint, const FVector& endPoint, TArray<FVector>& outPoints)
// {
//     outPoints.Reset();
//     
//     if (!IsValid(meshComp))
//     {
//         UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPath - Invalid mesh component"));
//         return;
//     }
//     
//     // Convertir le mesh en DynamicMesh3
//     FDynamicMesh3 Mesh;
//     if (!ConvertMeshComponentToDynamicMesh(meshComp, Mesh))
//     {
//         UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPath - Failed to convert mesh"));
//         return;
//     }
//     
//     UE_LOG(LogTemp, Log, TEXT("DynamicMesh - Vertices: %d, Triangles: %d"), Mesh.VertexCount(), Mesh.TriangleCount());
//     
//     // Transformer les points en coordonnées locales
//     FTransform ComponentTransform = meshComp->GetComponentTransform();
//     FVector3d LocalStart = ComponentTransform.InverseTransformPosition(startPoint);
//     FVector3d LocalEnd = ComponentTransform.InverseTransformPosition(endPoint);
//     
//     // Créer l'arbre spatial
//     FDynamicMeshAABBTree3 Spatial(&Mesh);
//     
//     // Projeter les points sur la surface
//     FVector3d ProjectedStart, ProjectedEnd;
//     int32 StartTriangleID, EndTriangleID;
//     
//     if (!ProjectPointToMeshSurface(Mesh, Spatial, LocalStart, ProjectedStart, StartTriangleID) ||
//         !ProjectPointToMeshSurface(Mesh, Spatial, LocalEnd, ProjectedEnd, EndTriangleID))
//     {
//         UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPath - Failed to project points to mesh surface"));
//         return;
//     }
//     
//     // Essayer plusieurs combinaisons de vertices des triangles
//     FIndex3i StartTriangle = Mesh.GetTriangle(StartTriangleID);
//     FIndex3i EndTriangle = Mesh.GetTriangle(EndTriangleID);
//     
//     TArray<int32> StartCandidates = {StartTriangle.A, StartTriangle.B, StartTriangle.C};
//     TArray<int32> EndCandidates = {EndTriangle.A, EndTriangle.B, EndTriangle.C};
//     
//     // Essayer notre A* avec différentes combinaisons
//     TArray<int32> BestPath;
//     float BestPathLength = TNumericLimits<float>::Max();
//     
//     for (int32 StartCandidate : StartCandidates)
//     {
//         for (int32 EndCandidate : EndCandidates)
//         {
//             if (StartCandidate == EndCandidate) continue;
//             
//             TArray<int32> CurrentPath;
//             if (FindPathAStar(Mesh, StartCandidate, EndCandidate, CurrentPath))
//             {
//                 // Calculer la longueur du chemin
//                 float PathLength = 0.0f;
//                 for (int32 i = 0; i < CurrentPath.Num() - 1; i++)
//                 {
//                     FVector3d Pos1 = Mesh.GetVertex(CurrentPath[i]);
//                     FVector3d Pos2 = Mesh.GetVertex(CurrentPath[i + 1]);
//                     PathLength += FVector3d::Dist(Pos1, Pos2);
//                 }
//                 
//                 if (PathLength < BestPathLength)
//                 {
//                     BestPathLength = PathLength;
//                     BestPath = CurrentPath;
//                 }
//                 
//                 UE_LOG(LogTemp, Log, TEXT("Found path from %d to %d with length %f"), StartCandidate, EndCandidate, PathLength);
//             }
//         }
//     }
//     
//     if (BestPath.Num() > 0)
//     {
//         // Convertir le chemin en coordonnées mondiales
//         outPoints.Reserve(BestPath.Num());
//         for (int32 VID : BestPath)
//         {
//             FVector3d LocalPos = Mesh.GetVertex(VID);
//             FVector WorldPos = ComponentTransform.TransformPosition(LocalPos);
//             outPoints.Add(WorldPos);
//         }
//         
//         UE_LOG(LogTemp, Log, TEXT("ComputeGeodesicPath - A* found path with %d points, length %f"), outPoints.Num(), BestPathLength);
//         return;
//     }
//     
//     // Si aucun chemin trouvé, utiliser les vertices les plus proches de chaque triangle
//     int32 StartVID = FindClosestVertexInTriangle(Mesh, ProjectedStart, StartTriangle);
//     int32 EndVID = FindClosestVertexInTriangle(Mesh, ProjectedEnd, EndTriangle);
//     
//     // Essayer un pathfinding simple BFS
//     TArray<int32> SimplePath;
//     if (FindPathBFS(Mesh, StartVID, EndVID, SimplePath))
//     {
//         outPoints.Reserve(SimplePath.Num());
//         for (int32 VID : SimplePath)
//         {
//             FVector3d LocalPos = Mesh.GetVertex(VID);
//             FVector WorldPos = ComponentTransform.TransformPosition(LocalPos);
//             outPoints.Add(WorldPos);
//         }
//         
//         UE_LOG(LogTemp, Log, TEXT("ComputeGeodesicPath - BFS found path with %d points"), outPoints.Num());
//         return;
//     }
//     
//     // Fallback final: ligne droite interpolée
//     UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPath - All pathfinding attempts failed, using straight line"));
//     const int32 NumIntermediatePoints = 10;
//     outPoints.Reserve(NumIntermediatePoints + 2);
//     
//     outPoints.Add(startPoint);
//     for (int32 i = 1; i <= NumIntermediatePoints; ++i)
//     {
//         float Alpha = (float)i / (float)(NumIntermediatePoints + 1);
//         FVector IntermediatePoint = FMath::Lerp(startPoint, endPoint, Alpha);
//         outPoints.Add(IntermediatePoint);
//     }
//     outPoints.Add(endPoint);
//     
//     UE_LOG(LogTemp, Log, TEXT("ComputeGeodesicPath - Generated fallback with %d points"), outPoints.Num());
// }

// Version améliorée de ComputeGeodesicPath avec fallback plus intelligent
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
    
    // Analyser la connectivité du mesh
    AnalyzeMeshConnectivity(Mesh);
    
    // Transformer les points en coordonnées locales
    FTransform ComponentTransform = meshComp->GetComponentTransform();
    FVector3d LocalStart = ComponentTransform.InverseTransformPosition(startPoint);
    FVector3d LocalEnd = ComponentTransform.InverseTransformPosition(endPoint);
    
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
    
    // Obtenir les triangles et leurs vertices
    FIndex3i StartTriangle = Mesh.GetTriangle(StartTriangleID);
    FIndex3i EndTriangle = Mesh.GetTriangle(EndTriangleID);
    
    TArray<int32> StartCandidates = {StartTriangle.A, StartTriangle.B, StartTriangle.C};
    TArray<int32> EndCandidates = {EndTriangle.A, EndTriangle.B, EndTriangle.C};
    
    // Priorise les candidats par distance
    StartCandidates.Sort([&](int32 A, int32 B) {
        float DistA = FVector3d::Dist(Mesh.GetVertex(A), ProjectedStart);
        float DistB = FVector3d::Dist(Mesh.GetVertex(B), ProjectedStart);
        return DistA < DistB;
    });
    
    EndCandidates.Sort([&](int32 A, int32 B) {
        float DistA = FVector3d::Dist(Mesh.GetVertex(A), ProjectedEnd);
        float DistB = FVector3d::Dist(Mesh.GetVertex(B), ProjectedEnd);
        return DistA < DistB;
    });
    
    TArray<int32> BestPath;
    float BestPathLength = TNumericLimits<float>::Max();
    bool bFoundConnectedPath = false;
    
    // Essayer d'abord des chemins dans le même composant
    for (int32 i = 0; i < StartCandidates.Num() && !bFoundConnectedPath; i++)
    {
        for (int32 j = 0; j < EndCandidates.Num() && !bFoundConnectedPath; j++)
        {
            int32 StartCandidate = StartCandidates[i];
            int32 EndCandidate = EndCandidates[j];
            
            if (StartCandidate == EndCandidate) continue;
            
            // Vérifier s'ils sont dans le même composant
            if (AreVerticesInSameComponent(Mesh, StartCandidate, EndCandidate))
            {
                UE_LOG(LogTemp, Log, TEXT("Trying connected path from vertex %d to vertex %d"), StartCandidate, EndCandidate);
                
                TArray<int32> CurrentPath;
                if (FindPathDijkstra(Mesh, StartCandidate, EndCandidate, CurrentPath))
                {
                    float PathLength = CalculatePathLength(Mesh, CurrentPath);
                    UE_LOG(LogTemp, Log, TEXT("SUCCESS: Found connected path with length %f (%d vertices)"), PathLength, CurrentPath.Num());
                    
                    if (PathLength < BestPathLength)
                    {
                        BestPathLength = PathLength;
                        BestPath = CurrentPath;
                        bFoundConnectedPath = true;
                    }
                }
            }
        }
    }
    
    // Si aucun chemin connecté trouvé, essayer un chemin avec pont
    if (!bFoundConnectedPath)
    {
        UE_LOG(LogTemp, Warning, TEXT("No connected path found, trying bridged path"));
        
        for (int32 i = 0; i < StartCandidates.Num(); i++)
        {
            for (int32 j = 0; j < EndCandidates.Num(); j++)
            {
                int32 StartCandidate = StartCandidates[i];
                int32 EndCandidate = EndCandidates[j];
                
                if (StartCandidate == EndCandidate) continue;
                
                TArray<int32> CurrentPath;
                if (FindPathBetweenComponents(Mesh, StartCandidate, EndCandidate, CurrentPath))
                {
                    float PathLength = CalculatePathLength(Mesh, CurrentPath);
                    UE_LOG(LogTemp, Log, TEXT("Found bridged path with length %f (%d vertices)"), PathLength, CurrentPath.Num());
                    
                    if (PathLength < BestPathLength)
                    {
                        BestPathLength = PathLength;
                        BestPath = CurrentPath;
                    }
                }
                
                // Prendre le premier chemin trouvé pour les composants déconnectés
                if (BestPath.Num() > 0) break;
            }
            if (BestPath.Num() > 0) break;
        }
    }
    
    if (BestPath.Num() > 0)
    {
        // Convertir le chemin en coordonnées mondiales
        outPoints.Reserve(BestPath.Num());
        for (int32 VID : BestPath)
        {
            FVector3d LocalPos = Mesh.GetVertex(VID);
            FVector WorldPos = ComponentTransform.TransformPosition(LocalPos);
            outPoints.Add(WorldPos);
        }
        
        UE_LOG(LogTemp, Log, TEXT("ComputeGeodesicPath - SUCCESS! Found path with %d points, length %f"), 
               outPoints.Num(), BestPathLength);
        return;
    }
    
    // Fallback: ligne droite
    UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPath - All pathfinding attempts failed, using straight line"));
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
}

// Fonction helper pour vérifier la connectivité rapidement
bool UPSFL_GeometryScript::AreVerticesConnected(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID)
{
    if (StartVID == EndVID) return true;
    
    TSet<int32> Visited;
    TQueue<int32> Queue;
    Queue.Enqueue(StartVID);
    Visited.Add(StartVID);
    
    int32 MaxSearchDepth = 100; // Limite plus petite pour test rapide
    int32 SearchDepth = 0;
    
    while (!Queue.IsEmpty() && SearchDepth < MaxSearchDepth)
    {
        int32 CurrentVID;
        Queue.Dequeue(CurrentVID);
        SearchDepth++;
        
        if (CurrentVID == EndVID)
        {
            return true;
        }
        
        for (int32 NeighborVID : Mesh.VtxVerticesItr(CurrentVID))
        {
            if (!Visited.Contains(NeighborVID))
            {
                Visited.Add(NeighborVID);
                Queue.Enqueue(NeighborVID);
            }
        }
    }
    
    return false;
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

#pragma region Triangle
//------------------

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

//------------------

#pragma endregion Triangle

#pragma region Vertex
//------------------

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

// Nouvelle fonction helper pour trouver le vertex le plus proche dans un triangle
int32 UPSFL_GeometryScript::FindClosestVertexInTriangle(const FDynamicMesh3& Mesh, const FVector3d& Point, const FIndex3i& Triangle)
{
	FVector3d VertexA = Mesh.GetVertex(Triangle.A);
	FVector3d VertexB = Mesh.GetVertex(Triangle.B);
	FVector3d VertexC = Mesh.GetVertex(Triangle.C);
    
	double DistA = FVector3d::DistSquared(Point, VertexA);
	double DistB = FVector3d::DistSquared(Point, VertexB);
	double DistC = FVector3d::DistSquared(Point, VertexC);
    
	if (DistA <= DistB && DistA <= DistC)
	{
		UE_LOG(LogTemp, Log, TEXT("Closest vertex in triangle: %d (A) at distance %f"), Triangle.A, FMath::Sqrt(DistA));
		return Triangle.A;
	}
	else if (DistB <= DistC)
	{
		UE_LOG(LogTemp, Log, TEXT("Closest vertex in triangle: %d (B) at distance %f"), Triangle.B, FMath::Sqrt(DistB));
		return Triangle.B;
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Closest vertex in triangle: %d (C) at distance %f"), Triangle.C, FMath::Sqrt(DistC));
		return Triangle.C;
	}
}

// Version améliorée de FindMinDistanceVertex avec plus de debugging
int32 UPSFL_GeometryScript::FindMinDistanceVertex(const TMap<int32, FMeshDijkstraNode>& NodeMap, const TSet<int32>& UnvisitedVertices)
{
	int32 MinVertex = FDynamicMesh3::InvalidID;
	float MinDistance = TNumericLimits<float>::Max();
	int32 FiniteDistanceCount = 0;
    
	for (int32 VID : UnvisitedVertices)
	{
		if (NodeMap[VID].Distance < TNumericLimits<float>::Max())
		{
			FiniteDistanceCount++;
		}
        
		if (NodeMap[VID].Distance < MinDistance)
		{
			MinDistance = NodeMap[VID].Distance;
			MinVertex = VID;
		}
	}
    
	if (FiniteDistanceCount == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("FindMinDistanceVertex - No vertices with finite distance remaining (total unvisited: %d)"), UnvisitedVertices.Num());
	}
    
	return MinVertex;
}

// Fonction helper pour obtenir le composant d'un vertex
TArray<int32> UPSFL_GeometryScript::GetVertexComponent(const FDynamicMesh3& Mesh, int32 VertexID)
{
	TArray<int32> Component;
	TSet<int32> Visited;
	TQueue<int32> Queue;
    
	Queue.Enqueue(VertexID);
	Visited.Add(VertexID);
    
	while (!Queue.IsEmpty())
	{
		int32 CurrentVID;
		Queue.Dequeue(CurrentVID);
		Component.Add(CurrentVID);
        
		for (int32 NeighborVID : Mesh.VtxVerticesItr(CurrentVID))
		{
			if (!Visited.Contains(NeighborVID))
			{
				Visited.Add(NeighborVID);
				Queue.Enqueue(NeighborVID);
			}
		}
	}
    
	return Component;
}


//------------------

#pragma endregion Vertex

#pragma region MeshConverter
//------------------

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

//------------------
#pragma endregion MeshConverter

#pragma region PathStar
//------------------


bool UPSFL_GeometryScript::FindPathAStar(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath)
{
    if (StartVID == EndVID)
    {
        OutPath.Add(StartVID);
        return true;
    }
    
    // Vérifier que les vertices existent
    if (!Mesh.IsVertex(StartVID) || !Mesh.IsVertex(EndVID))
    {
        UE_LOG(LogTemp, Warning, TEXT("FindPathAStar - Invalid vertex IDs: Start=%d, End=%d"), StartVID, EndVID);
        return false;
    }
    
    FVector3d StartPos = Mesh.GetVertex(StartVID);
    FVector3d EndPos = Mesh.GetVertex(EndVID);
    
    // Listes ouvertes et fermées
    TArray<FMeshPathNode> OpenList;
    TSet<int32> ClosedSet;
    TMap<int32, int32> OpenSetMap; // VertexID -> Index dans OpenList
    
    // Commencer avec le vertex de départ
    FMeshPathNode StartNode(StartVID, 0.0f, FVector3d::Dist(StartPos, EndPos), FDynamicMesh3::InvalidID);
    OpenList.Add(StartNode);
    OpenSetMap.Add(StartVID, 0);
    
    UE_LOG(LogTemp, Log, TEXT("FindPathAStar - Starting A* from vertex %d to %d"), StartVID, EndVID);
    
    int32 MaxIterations = 1000;
    int32 Iterations = 0;
    
    while (OpenList.Num() > 0 && Iterations < MaxIterations)
    {
        Iterations++;
        
        // Trouver le nœud avec le plus petit FCost
        int32 BestIndex = 0;
        float BestFCost = OpenList[0].FCost;
        
        for (int32 i = 1; i < OpenList.Num(); i++)
        {
            if (OpenList[i].FCost < BestFCost)
            {
                BestFCost = OpenList[i].FCost;
                BestIndex = i;
            }
        }
        
        // Prendre le meilleur nœud
        FMeshPathNode CurrentNode = OpenList[BestIndex];
        
        // Le retirer de la liste ouverte
        OpenList.RemoveAtSwap(BestIndex);
        OpenSetMap.Remove(CurrentNode.VertexID);
        
        // L'ajouter à la liste fermée
        ClosedSet.Add(CurrentNode.VertexID);
        
        // Vérifier si nous avons atteint l'objectif
        if (CurrentNode.VertexID == EndVID)
        {
            UE_LOG(LogTemp, Log, TEXT("FindPathAStar - Path found in %d iterations"), Iterations);
            
            // Reconstruire le chemin
            OutPath.Reset();
            int32 CurrentVID = EndVID;
            
            // Créer une map pour retrouver les parents
            TMap<int32, int32> ParentMap;
            for (int32 VID : ClosedSet)
            {
                // Retrouver le parent de ce vertex
                if (VID == StartVID)
                {
                    ParentMap.Add(VID, FDynamicMesh3::InvalidID);
                }
                else
                {
                    // Chercher dans nos données (on devrait stocker ça mieux)
                    // Pour l'instant, reconstruire depuis CurrentNode
                    if (VID == CurrentNode.VertexID)
                    {
                        ParentMap.Add(VID, CurrentNode.ParentID);
                    }
                }
            }
            
            // Reconstruire le chemin (on va implémenter une version plus simple)
            return ReconstructPathAStar(Mesh, StartVID, EndVID, ClosedSet, OutPath);
        }
        
        // Explorer les voisins
        FVector3d CurrentPos = Mesh.GetVertex(CurrentNode.VertexID);
        
        for (int32 NeighborVID : Mesh.VtxVerticesItr(CurrentNode.VertexID))
        {
            // Ignorer si déjà dans la liste fermée
            if (ClosedSet.Contains(NeighborVID))
            {
                continue;
            }
            
            FVector3d NeighborPos = Mesh.GetVertex(NeighborVID);
            float EdgeCost = FVector3d::Dist(CurrentPos, NeighborPos);
            float NewGCost = CurrentNode.GCost + EdgeCost;
            float HCost = FVector3d::Dist(NeighborPos, EndPos);
            
            // Vérifier si ce voisin est déjà dans la liste ouverte
            if (OpenSetMap.Contains(NeighborVID))
            {
                int32 ExistingIndex = OpenSetMap[NeighborVID];
                if (NewGCost < OpenList[ExistingIndex].GCost)
                {
                    // Mettre à jour le nœud existant
                    OpenList[ExistingIndex].GCost = NewGCost;
                    OpenList[ExistingIndex].FCost = NewGCost + HCost;
                    OpenList[ExistingIndex].ParentID = CurrentNode.VertexID;
                }
            }
            else
            {
                // Ajouter à la liste ouverte
                FMeshPathNode NewNode(NeighborVID, NewGCost, HCost, CurrentNode.VertexID);
                OpenList.Add(NewNode);
                OpenSetMap.Add(NeighborVID, OpenList.Num() - 1);
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("FindPathAStar - No path found after %d iterations"), Iterations);
    return false;
}

// Version simplifiée de reconstruction de chemin
bool UPSFL_GeometryScript::ReconstructPathAStar(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, const TSet<int32>& VisitedVertices, TArray<int32>& OutPath)
{
	// Pour cette version simple, on va faire un BFS inverse depuis EndVID vers StartVID
	// en utilisant seulement les vertices visités
    
	TMap<int32, int32> ParentMap;
	TQueue<int32> Queue;
	TSet<int32> Visited;
    
	Queue.Enqueue(EndVID);
	Visited.Add(EndVID);
	ParentMap.Add(EndVID, FDynamicMesh3::InvalidID);
    
	bool bFoundPath = false;
    
	while (!Queue.IsEmpty())
	{
		int32 CurrentVID;
		Queue.Dequeue(CurrentVID);
        
		if (CurrentVID == StartVID)
		{
			bFoundPath = true;
			break;
		}
        
		for (int32 NeighborVID : Mesh.VtxVerticesItr(CurrentVID))
		{
			if (VisitedVertices.Contains(NeighborVID) && !Visited.Contains(NeighborVID))
			{
				Visited.Add(NeighborVID);
				ParentMap.Add(NeighborVID, CurrentVID);
				Queue.Enqueue(NeighborVID);
			}
		}
	}
    
	if (!bFoundPath)
	{
		return false;
	}
    
	// Reconstruire le chemin
	OutPath.Reset();
	int32 CurrentVID = StartVID;
    
	while (CurrentVID != FDynamicMesh3::InvalidID)
	{
		OutPath.Add(CurrentVID);
		CurrentVID = ParentMap.Contains(CurrentVID) ? ParentMap[CurrentVID] : FDynamicMesh3::InvalidID;
        
		if (CurrentVID == EndVID)
		{
			OutPath.Add(EndVID);
			break;
		}
	}
    
	return OutPath.Num() > 1;
}

// Pathfinding BFS simple comme backup
bool UPSFL_GeometryScript::FindPathBFS(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath)
{
	if (StartVID == EndVID)
	{
		OutPath.Add(StartVID);
		return true;
	}
    
	TMap<int32, int32> ParentMap;
	TQueue<int32> Queue;
	TSet<int32> Visited;
    
	Queue.Enqueue(StartVID);
	Visited.Add(StartVID);
	ParentMap.Add(StartVID, FDynamicMesh3::InvalidID);
    
	bool bFoundPath = false;
	int32 MaxIterations = 1000;
	int32 Iterations = 0;
    
	while (!Queue.IsEmpty() && Iterations < MaxIterations)
	{
		Iterations++;
        
		int32 CurrentVID;
		Queue.Dequeue(CurrentVID);
        
		if (CurrentVID == EndVID)
		{
			bFoundPath = true;
			break;
		}
        
		for (int32 NeighborVID : Mesh.VtxVerticesItr(CurrentVID))
		{
			if (!Visited.Contains(NeighborVID))
			{
				Visited.Add(NeighborVID);
				ParentMap.Add(NeighborVID, CurrentVID);
				Queue.Enqueue(NeighborVID);
			}
		}
	}
    
	if (!bFoundPath)
	{
		UE_LOG(LogTemp, Warning, TEXT("FindPathBFS - No path found after %d iterations"), Iterations);
		return false;
	}
    
	// Reconstruire le chemin
	OutPath.Reset();
	int32 CurrentVID = EndVID;
    
	while (CurrentVID != FDynamicMesh3::InvalidID)
	{
		OutPath.Add(CurrentVID);
		CurrentVID = ParentMap.Contains(CurrentVID) ? ParentMap[CurrentVID] : FDynamicMesh3::InvalidID;
	}
    
	// Inverser le chemin pour qu'il aille du début à la fin
	Algo::Reverse(OutPath);
    
	UE_LOG(LogTemp, Log, TEXT("FindPathBFS - Found path with %d vertices in %d iterations"), OutPath.Num(), Iterations);
	return OutPath.Num() > 1;
}

//------------------
#pragma endregion PathStar

#pragma region Djikstra
//------------------


// Fonction helper pour essayer Dijkstra
bool UPSFL_GeometryScript::TryDijkstraPath(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, const FTransform& ComponentTransform, TArray<FVector>& outPoints)
{
	// Vérifier la connectivité rapidement
	if (!AreVerticesConnected(Mesh, StartVID, EndVID))
	{
		UE_LOG(LogTemp, Log, TEXT("TryDijkstraPath - Vertices %d and %d are not connected"), StartVID, EndVID);
		return false;
	}
    
	// Lancer Dijkstra
	TMeshDijkstra<FDynamicMesh3> Dijkstra(&Mesh);
	TArray<TMeshDijkstra<FDynamicMesh3>::FSeedPoint> SeedPoints;
	SeedPoints.Add(TMeshDijkstra<FDynamicMesh3>::FSeedPoint(StartVID, 0.0));
    
	Dijkstra.ComputeToTargetPoint(SeedPoints, EndVID);
    
	if (!Dijkstra.HasDistance(EndVID))
	{
		UE_LOG(LogTemp, Log, TEXT("TryDijkstraPath - Dijkstra failed to find path"));
		return false;
	}
    
	// Reconstruire le chemin
	TArray<int32> Path;
	if (!ReconstructPath(Dijkstra, Mesh, StartVID, EndVID, Path))
	{
		UE_LOG(LogTemp, Log, TEXT("TryDijkstraPath - Failed to reconstruct path"));
		return false;
	}
    
	// Convertir en coordonnées mondiales
	outPoints.Reset();
	outPoints.Reserve(Path.Num());
	for (int32 VID : Path)
	{
		FVector3d LocalPos = Mesh.GetVertex(VID);
		FVector WorldPos = ComponentTransform.TransformPosition(LocalPos);
		outPoints.Add(WorldPos);
	}
    
	UE_LOG(LogTemp, Log, TEXT("TryDijkstraPath - Success! Generated %d points"), outPoints.Num());
	return true;
}

// Notre propre implémentation de l'algorithme de Dijkstra avec détection de connectivité
bool UPSFL_GeometryScript::FindPathDijkstra(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath)
{
    if (StartVID == EndVID)
    {
        OutPath.Add(StartVID);
        return true;
    }
    
    // Vérifier que les vertices existent
    if (!Mesh.IsVertex(StartVID) || !Mesh.IsVertex(EndVID))
    {
        UE_LOG(LogTemp, Warning, TEXT("FindPathDijkstra - Invalid vertex IDs: Start=%d, End=%d"), StartVID, EndVID);
        return false;
    }
    
    // D'abord, vérifier si les vertices sont dans le même composant connecté
    if (!AreVerticesInSameComponent(Mesh, StartVID, EndVID))
    {
        UE_LOG(LogTemp, Warning, TEXT("FindPathDijkstra - Vertices %d and %d are not in the same connected component"), StartVID, EndVID);
        return false;
    }
    
    UE_LOG(LogTemp, Log, TEXT("FindPathDijkstra - Starting from vertex %d to %d"), StartVID, EndVID);
    
    // Initialiser les structures de données
    TMap<int32, FMeshDijkstraNode> NodeMap;
    TSet<int32> UnvisitedVertices;
    
    // Initialiser tous les vertices
    for (int32 VID : Mesh.VertexIndicesItr())
    {
        NodeMap.Add(VID, FMeshDijkstraNode(VID));
        UnvisitedVertices.Add(VID);
    }
    
    // Distance du vertex de départ = 0
    NodeMap[StartVID].Distance = 0.0f;
    
    UE_LOG(LogTemp, Log, TEXT("FindPathDijkstra - Initialized %d vertices"), NodeMap.Num());
    
    int32 MaxIterations = 10000;
    int32 Iterations = 0;
    int32 FiniteDistanceVertices = 1; // Commencer avec le vertex de départ
    
    while (UnvisitedVertices.Num() > 0 && Iterations < MaxIterations)
    {
        Iterations++;
        
        // Trouver le vertex non visité avec la distance minimale
        int32 CurrentVID = FindMinDistanceVertex(NodeMap, UnvisitedVertices);
        
        if (CurrentVID == FDynamicMesh3::InvalidID)
        {
            UE_LOG(LogTemp, Warning, TEXT("FindPathDijkstra - No valid vertex found at iteration %d"), Iterations);
            break;
        }
        
        // Vérifier si la distance est infinie
        if (NodeMap[CurrentVID].Distance == TNumericLimits<float>::Max())
        {
            UE_LOG(LogTemp, Warning, TEXT("FindPathDijkstra - Reached infinite distance at iteration %d, no more connected vertices"), Iterations);
            break;
        }
        
        // Marquer comme visité
        NodeMap[CurrentVID].bVisited = true;
        UnvisitedVertices.Remove(CurrentVID);
        
        // Si nous avons atteint la destination
        if (CurrentVID == EndVID)
        {
            UE_LOG(LogTemp, Log, TEXT("FindPathDijkstra - Reached destination in %d iterations"), Iterations);
            return ReconstructPathDijkstra(NodeMap, StartVID, EndVID, OutPath);
        }
        
        // Examiner tous les voisins
        FVector3d CurrentPos = Mesh.GetVertex(CurrentVID);
        int32 NeighborsUpdated = 0;
        
        for (int32 NeighborVID : Mesh.VtxVerticesItr(CurrentVID))
        {
            if (NodeMap[NeighborVID].bVisited)
            {
                continue;
            }
            
            // Calculer la distance vers ce voisin
            FVector3d NeighborPos = Mesh.GetVertex(NeighborVID);
            float EdgeDistance = FVector3d::Dist(CurrentPos, NeighborPos);
            float NewDistance = NodeMap[CurrentVID].Distance + EdgeDistance;
            
            // Si nous avons trouvé un chemin plus court
            if (NewDistance < NodeMap[NeighborVID].Distance)
            {
                // Si c'était la première fois qu'on atteignait ce vertex
                if (NodeMap[NeighborVID].Distance == TNumericLimits<float>::Max())
                {
                    FiniteDistanceVertices++;
                }
                
                NodeMap[NeighborVID].Distance = NewDistance;
                NodeMap[NeighborVID].ParentID = CurrentVID;
                NeighborsUpdated++;
                
                if (Iterations < 20) // Debug pour les premières itérations
                {
                    UE_LOG(LogTemp, Log, TEXT("Updated vertex %d: distance=%f, parent=%d"), 
                           NeighborVID, NewDistance, CurrentVID);
                }
            }
        }
        
        // Log périodique avec plus d'informations
        if (Iterations % 50 == 0)
        {
            UE_LOG(LogTemp, Log, TEXT("FindPathDijkstra - Iteration %d: vertex %d (dist=%f), neighbors updated=%d, finite vertices=%d, remaining=%d"), 
                   Iterations, CurrentVID, NodeMap[CurrentVID].Distance, NeighborsUpdated, FiniteDistanceVertices, UnvisitedVertices.Num());
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("FindPathDijkstra - Failed to find path after %d iterations"), Iterations);
    return false;
}

bool UPSFL_GeometryScript::ReconstructPathDijkstra(const TMap<int32, FMeshDijkstraNode>& NodeMap, int32 StartVID, int32 EndVID, TArray<int32>& OutPath)
{
    OutPath.Reset();
    
    // Vérifier que nous avons bien une solution
    if (!NodeMap.Contains(EndVID) || NodeMap[EndVID].Distance == TNumericLimits<float>::Max())
    {
        UE_LOG(LogTemp, Warning, TEXT("ReconstructPathDijkstra - No valid path to end vertex"));
        return false;
    }
    
    // Reconstruire le chemin en remontant depuis la fin
    TArray<int32> ReversePath;
    int32 CurrentVID = EndVID;
    
    while (CurrentVID != FDynamicMesh3::InvalidID)
    {
        ReversePath.Add(CurrentVID);
        
        if (CurrentVID == StartVID)
        {
            break;
        }
        
        CurrentVID = NodeMap[CurrentVID].ParentID;
        
        // Sécurité pour éviter les boucles infinies
        if (ReversePath.Num() > 10000)
        {
            UE_LOG(LogTemp, Error, TEXT("ReconstructPathDijkstra - Infinite loop detected"));
            return false;
        }
    }
    
    // Inverser le chemin pour qu'il aille du début à la fin
    OutPath.Reserve(ReversePath.Num());
    for (int32 i = ReversePath.Num() - 1; i >= 0; i--)
    {
        OutPath.Add(ReversePath[i]);
    }
    
    UE_LOG(LogTemp, Log, TEXT("ReconstructPathDijkstra - Reconstructed path with %d vertices"), OutPath.Num());
    
    // Calculer la distance totale pour debug
    float TotalDistance = NodeMap[EndVID].Distance;
    UE_LOG(LogTemp, Log, TEXT("ReconstructPathDijkstra - Total path distance: %f"), TotalDistance);
    
    return OutPath.Num() > 0;
}

//------------------
#pragma endregion Djikstra

// Nouvelle fonction pour vérifier si deux vertices sont dans le même composant connecté
bool UPSFL_GeometryScript::AreVerticesInSameComponent(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID)
{
	if (StartVID == EndVID) return true;
    
	// BFS limité pour vérifier la connectivité
	TSet<int32> Visited;
	TQueue<int32> Queue;
    
	Queue.Enqueue(StartVID);
	Visited.Add(StartVID);
    
	int32 MaxSearchNodes = 1000; // Limiter la recherche
	int32 NodesExplored = 0;
    
	while (!Queue.IsEmpty() && NodesExplored < MaxSearchNodes)
	{
		int32 CurrentVID;
		Queue.Dequeue(CurrentVID);
		NodesExplored++;
        
		if (CurrentVID == EndVID)
		{
			UE_LOG(LogTemp, Log, TEXT("AreVerticesInSameComponent - Found connection after exploring %d nodes"), NodesExplored);
			return true;
		}
        
		for (int32 NeighborVID : Mesh.VtxVerticesItr(CurrentVID))
		{
			if (!Visited.Contains(NeighborVID))
			{
				Visited.Add(NeighborVID);
				Queue.Enqueue(NeighborVID);
			}
		}
	}
    
	UE_LOG(LogTemp, Warning, TEXT("AreVerticesInSameComponent - No connection found after exploring %d nodes"), NodesExplored);
	return false;
}

// Fonction pour analyser la connectivité du mesh
void UPSFL_GeometryScript::AnalyzeMeshConnectivity(const FDynamicMesh3& Mesh)
{
	TSet<int32> UnvisitedVertices;
	for (int32 VID : Mesh.VertexIndicesItr())
	{
		UnvisitedVertices.Add(VID);
	}
    
	TArray<TArray<int32>> ConnectedComponents;
    
	while (UnvisitedVertices.Num() > 0)
	{
		// Prendre un vertex non visité et explorer son composant
		int32 StartVID = *UnvisitedVertices.CreateIterator();
        
		TArray<int32> Component;
		TQueue<int32> Queue;
        
		Queue.Enqueue(StartVID);
		UnvisitedVertices.Remove(StartVID);
        
		while (!Queue.IsEmpty())
		{
			int32 CurrentVID;
			Queue.Dequeue(CurrentVID);
			Component.Add(CurrentVID);
            
			// Examiner tous les voisins
			for (int32 NeighborVID : Mesh.VtxVerticesItr(CurrentVID))
			{
				if (UnvisitedVertices.Contains(NeighborVID))
				{
					UnvisitedVertices.Remove(NeighborVID);
					Queue.Enqueue(NeighborVID);
				}
			}
		}
        
		ConnectedComponents.Add(Component);
		UE_LOG(LogTemp, Log, TEXT("Found connected component %d with %d vertices"), 
			   ConnectedComponents.Num(), Component.Num());
	}
    
	UE_LOG(LogTemp, Warning, TEXT("Mesh has %d connected components"), ConnectedComponents.Num());
    
	// Afficher les détails des composants
	for (int32 i = 0; i < ConnectedComponents.Num(); i++)
	{
		UE_LOG(LogTemp, Log, TEXT("Component %d: %d vertices"), i, ConnectedComponents[i].Num());
	}
}

// Fonction pour trouver le chemin entre deux composants différents
bool UPSFL_GeometryScript::FindPathBetweenComponents(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath)
{
    // Trouver les composants de chaque vertex
    TArray<int32> StartComponent = GetVertexComponent(Mesh, StartVID);
    TArray<int32> EndComponent = GetVertexComponent(Mesh, EndVID);
    
    if (StartComponent.Num() == 0 || EndComponent.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not find components for vertices"));
        return false;
    }
    
    // Trouver les vertices les plus proches entre les deux composants
    int32 ClosestStartVID = FDynamicMesh3::InvalidID;
    int32 ClosestEndVID = FDynamicMesh3::InvalidID;
    float MinDistance = TNumericLimits<float>::Max();
    
    for (int32 StartComponentVID : StartComponent)
    {
        FVector3d StartPos = Mesh.GetVertex(StartComponentVID);
        
        for (int32 EndComponentVID : EndComponent)
        {
            FVector3d EndPos = Mesh.GetVertex(EndComponentVID);
            float Distance = FVector3d::Dist(StartPos, EndPos);
            
            if (Distance < MinDistance)
            {
                MinDistance = Distance;
                ClosestStartVID = StartComponentVID;
                ClosestEndVID = EndComponentVID;
            }
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Closest bridge: vertex %d to vertex %d (distance: %f)"), 
           ClosestStartVID, ClosestEndVID, MinDistance);
    
    // Créer le chemin en trois parties
    TArray<int32> PathToStart, PathFromEnd;
    
    // Partie 1: Du vertex de départ au vertex le plus proche dans son composant
    if (StartVID != ClosestStartVID)
    {
        if (!FindPathDijkstra(Mesh, StartVID, ClosestStartVID, PathToStart))
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not find path within start component"));
            return false;
        }
    }
    else
    {
        PathToStart.Add(StartVID);
    }
    
    // Partie 3: Du vertex le plus proche dans le composant de fin au vertex de fin
    if (ClosestEndVID != EndVID)
    {
        if (!FindPathDijkstra(Mesh, ClosestEndVID, EndVID, PathFromEnd))
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not find path within end component"));
            return false;
        }
    }
    else
    {
        PathFromEnd.Add(EndVID);
    }
    
    // Combiner les chemins
    OutPath.Reset();
    OutPath.Append(PathToStart);
    
    // Ajouter le pont (ligne droite)
    if (ClosestStartVID != ClosestEndVID)
    {
        OutPath.Add(ClosestEndVID);
    }
    
    // Ajouter le chemin de fin (sans le premier vertex pour éviter les doublons)
    if (PathFromEnd.Num() > 1)
    {
        for (int32 i = 1; i < PathFromEnd.Num(); i++)
        {
            OutPath.Add(PathFromEnd[i]);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Created bridged path with %d vertices"), OutPath.Num());
    return true;
}

// Fonction helper pour calculer la longueur d'un chemin
float UPSFL_GeometryScript::CalculatePathLength(const FDynamicMesh3& Mesh, const TArray<int32>& Path)
{
	float Length = 0.0f;
	for (int32 i = 0; i < Path.Num() - 1; i++)
	{
		FVector3d Pos1 = Mesh.GetVertex(Path[i]);
		FVector3d Pos2 = Mesh.GetVertex(Path[i + 1]);
		Length += FVector3d::Dist(Pos1, Pos2);
	}
	return Length;
}











