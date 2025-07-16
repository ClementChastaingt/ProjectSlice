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
	if(_bDebug) UE_LOG(LogTemp, Log, TEXT("ProjectPointToMeshSurface - Input Point: %s"), *Point.ToString());
    
	// Utiliser uniquement la recherche brute force pour éviter les problèmes d'arbre spatial
	OutTriangleID = FindNearestTriangleBruteForce(Mesh, Point);
    
	if (OutTriangleID == FDynamicMesh3::InvalidID)
	{
		if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("Could not find any triangle for point projection"));
		return false;
	}
    
	// Projeter le point sur le triangle trouvé
	OutProjectedPoint = GetClosestPointOnTriangle(Mesh, OutTriangleID, Point);
    
	double ProjectionDistance = FVector3d::Distance(Point, OutProjectedPoint);
    
	if(_bDebug) UE_LOG(LogTemp, Log, TEXT("Projected point %s to %s on triangle %d (distance: %f)"), 
		   *Point.ToString(), *OutProjectedPoint.ToString(), OutTriangleID, ProjectionDistance);
    
	return true;
}

// Version simplifiée et corrigée de ComputeGeodesicPath
void UPSFL_GeometryScript::ComputeGeodesicPath(UMeshComponent* meshComp, const FVector& startPoint, const FVector& endPoint, TArray<FVector>& outPoints, const bool bDebug, const bool bDebugPoint)
{
	// Vérifier les paramètres d'entrée
	if (!meshComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPath: MeshComponent is null"));
		return;
	}

	// Créer la clé de cache
	FPathCacheKey CacheKey(startPoint, endPoint, meshComp);

	// Vérifier si le résultat existe déjà dans le cache
	if (TryGetCachedPath(CacheKey, outPoints))
	{
		if (bDebug)
		{
			UE_LOG(LogTemp, Log, TEXT("ComputeGeodesicPath: Using cached path with %d points"), outPoints.Num());
		}
		return;
	}

	// Si pas dans le cache, calculer le path normalement
	TArray<FVector> CalculatedPoints;

	// Init var
    outPoints.Reset();

	_bDebug = bDebug;
	_bDebugPoint = bDebugPoint;
	
    // Convertir le mesh en DynamicMesh3
    FDynamicMesh3 Mesh;
    if (!ConvertMeshComponentToDynamicMesh(meshComp, Mesh))
    {
        if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPath - Failed to convert mesh"));
        return;
    }
    
    // Obtenir le transform du composant
    FTransform ComponentTransform = meshComp->GetComponentTransform();
    
    if(_bDebug) UE_LOG(LogTemp, Log, TEXT("DynamicMesh - Vertices: %d, Triangles: %d"), 
           Mesh.VertexCount(), Mesh.TriangleCount());
    
    // Analyser la connectivité du mesh
    AnalyzeMeshConnectivity(Mesh);
    
    // Transformer les points en coordonnées locales
    FVector3d LocalStart = ComponentTransform.InverseTransformPosition(startPoint);
    FVector3d LocalEnd = ComponentTransform.InverseTransformPosition(endPoint);
    
    if(_bDebug) UE_LOG(LogTemp, Log, TEXT("LocalStart: %s, LocalEnd: %s"), 
           *LocalStart.ToString(), *LocalEnd.ToString());
    
    // Créer l'arbre spatial
    FDynamicMeshAABBTree3 Spatial(&Mesh);
    
    // Projeter les points sur la surface
    FVector3d ProjectedStart, ProjectedEnd;
    int32 StartTriangleID, EndTriangleID;
    
    if (!ProjectPointToMeshSurface(Mesh, Spatial, LocalStart, ProjectedStart, StartTriangleID) ||
        !ProjectPointToMeshSurface(Mesh, Spatial, LocalEnd, ProjectedEnd, EndTriangleID))
    {
        if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPath - Failed to project points to mesh surface"));
        return;
    }
    
    if(_bDebug) UE_LOG(LogTemp, Log, TEXT("ProjectedStart: %s, ProjectedEnd: %s"), 
           *ProjectedStart.ToString(), *ProjectedEnd.ToString());
    
    // Si les points projetés sont très proches, créer un chemin direct
    float ProjectedDistance = FVector3d::Dist(ProjectedStart, ProjectedEnd);
    if (ProjectedDistance < 1.0f)
    {
        if(_bDebug) UE_LOG(LogTemp, Log, TEXT("Projected points are very close (%f), creating direct path"), ProjectedDistance);
        
        // Créer un chemin direct avec quelques points intermédiaires
        const int32 NumPoints = 8;
        outPoints.Reserve(NumPoints);
        
        for (int32 i = 0; i < NumPoints; i++)
        {
            float Alpha = (float)i / (float)(NumPoints - 1);
            FVector3d IntermediateLocal = FMath::Lerp(ProjectedStart, ProjectedEnd, Alpha);
            FVector WorldPos = ComponentTransform.TransformPosition(IntermediateLocal);
            outPoints.Add(WorldPos);
        }
        
        if(_bDebug) UE_LOG(LogTemp, Log, TEXT("ComputeGeodesicPath - Created direct path with %d points"), outPoints.Num());

    	//Stockage en cache du résultat
    	StoreCachedPath(CacheKey, outPoints);
    	
        return;
    }
    
    // Obtenir les triangles et leurs vertices
    FIndex3i StartTriangle = Mesh.GetTriangle(StartTriangleID);
    FIndex3i EndTriangle = Mesh.GetTriangle(EndTriangleID);
    
    TArray<int32> StartCandidates = {StartTriangle.A, StartTriangle.B, StartTriangle.C};
    TArray<int32> EndCandidates = {EndTriangle.A, EndTriangle.B, EndTriangle.C};
    
    // Priorise les candidats par distance aux points projetés
    StartCandidates.Sort([&](int32 A, int32 B) {
        FVector3d PosA = Mesh.GetVertex(A);
        FVector3d PosB = Mesh.GetVertex(B);
        float DistA = FVector3d::Dist(PosA, ProjectedStart);
        float DistB = FVector3d::Dist(PosB, ProjectedStart);
        return DistA < DistB;
    });
    
    EndCandidates.Sort([&](int32 A, int32 B) {
        FVector3d PosA = Mesh.GetVertex(A);
        FVector3d PosB = Mesh.GetVertex(B);
        float DistA = FVector3d::Dist(PosA, ProjectedEnd);
        float DistB = FVector3d::Dist(PosB, ProjectedEnd);
        return DistA < DistB;
    });
    
    TArray<int32> BestPath;
    float BestPathLength = TNumericLimits<float>::Max();
    bool bFoundPath = false;
    
    // Essayer toutes les combinaisons de vertices
    for (int32 i = 0; i < StartCandidates.Num() && !bFoundPath; i++)
    {
        for (int32 j = 0; j < EndCandidates.Num() && !bFoundPath; j++)
        {
            int32 StartCandidate = StartCandidates[i];
            int32 EndCandidate = EndCandidates[j];
            
            if (StartCandidate == EndCandidate) continue;
            
            if(_bDebug) UE_LOG(LogTemp, Log, TEXT("Trying path from vertex %d to vertex %d"), StartCandidate, EndCandidate);
            
            TArray<int32> CurrentPath;
            bool bPathFound = false;
            
            // Essayer d'abord un chemin connecté
            if (AreVerticesInSameComponent(Mesh, StartCandidate, EndCandidate))
            {
                bPathFound = FindPathDijkstra(Mesh, StartCandidate, EndCandidate, CurrentPath);
            }
            else
            {
                bPathFound = FindPathBetweenComponents(Mesh, StartCandidate, EndCandidate, CurrentPath);
            }
            
            if (bPathFound && CurrentPath.Num() > 0)
            {
                float PathLength = CalculatePathLength(Mesh, CurrentPath);
                if(_bDebug) UE_LOG(LogTemp, Log, TEXT("Found path with length %f (%d vertices)"), PathLength, CurrentPath.Num());
                
                if (PathLength < BestPathLength)
                {
                    BestPathLength = PathLength;
                    BestPath = CurrentPath;
                    bFoundPath = true;
                }
            }
        }
    }
    
    if (BestPath.Num() > 0)
    {
        // Convertir le chemin en coordonnées mondiales
        outPoints.Reserve(BestPath.Num());
    	int i = 0;
        for (int32 VID : BestPath)
        {
            FVector3d LocalPos = Mesh.GetVertex(VID);
            FVector WorldPos = ComponentTransform.TransformPosition(LocalPos);

        	float alpha = static_cast<float>(i) / static_cast<float>(BestPath.Num());
        	FVector intermediatePoint = FMath::Lerp(startPoint, endPoint, alpha);
        	WorldPos.Z = intermediatePoint.Z;
        	
            outPoints.Add(WorldPos);

        	if (_bDebugPoint && meshComp->GetWorld()) DrawDebugPoint(meshComp->GetWorld(), WorldPos, 10.f + (i * 5), FColor::Magenta, true, 0.5f, 20.0f);

        	i++;
        }
        
    	if(_bDebug) UE_LOG(LogTemp, Log, TEXT("ComputeGeodesicPath - SUCCESS! Found path with %d points, length %f"), outPoints.Num(), BestPathLength);

    	//Stockage en cache du résultat
    	StoreCachedPath(CacheKey, outPoints);
    	
        return;
    }
    
    // Fallback: ligne droite
    if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPath - All pathfinding attempts failed, using straight line"));
    constexpr int32 numIntermediatePoints = 10;
    outPoints.Reserve(numIntermediatePoints + 2);
    
    outPoints.Add(startPoint);
    for (int32 i = 1; i <= numIntermediatePoints; ++i)
    {
        float alpha = static_cast<float>(i) / static_cast<float>(numIntermediatePoints + 1);
        FVector IntermediatePoint = FMath::Lerp(startPoint, endPoint, alpha);
        outPoints.Add(IntermediatePoint);

    	if (_bDebugPoint && meshComp->GetWorld()) DrawDebugPoint(meshComp->GetWorld(), IntermediatePoint, 10.f, FColor::Magenta, true, 0.5f, 20.0f);
    }
    outPoints.Add(endPoint);
    
     //Stockage en cache du résultat
     StoreCachedPath(CacheKey, outPoints);
	
	
}

void UPSFL_GeometryScript::ComputeGeodesicPathWithVelocity(UMeshComponent* meshComp, const FVector& startPoint, const FVector& endPoint, const FVector& velocity, TArray<FVector>& outPoints, float velocityInfluence, const bool bDebug, const bool bDebugPoint)
{
    // Vérifier les paramètres d'entrée
    if (!meshComp)
    {
        UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPathWithVelocity: MeshComponent is null"));
        return;
    }

	if (velocity.IsNearlyZero())
	{
		ComputeGeodesicPath(meshComp, startPoint, endPoint, outPoints, bDebug, bDebugPoint);
		return;
	}

    // Normaliser la vélocité si elle n'est pas nulle
    FVector3d normalizedVelocity = velocity;
    if (normalizedVelocity.Length() > SMALL_NUMBER)
    {
        normalizedVelocity.Normalize();
    }

    // Créer une clé de cache modifiée pour inclure la vélocité
    FPathCacheKey CacheKey(startPoint, endPoint, meshComp);
    // Note: vous pourriez vouloir modifier FPathCacheKey pour inclure la vélocité

    // Initialiser les variables
    outPoints.Reset();
    _bDebug = bDebug;
    _bDebugPoint = bDebugPoint;

    // Convertir le mesh en DynamicMesh3
    FDynamicMesh3 Mesh;
    if (!ConvertMeshComponentToDynamicMesh(meshComp, Mesh))
    {
        if (_bDebug) UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPathWithVelocity - Failed to convert mesh"));
        return;
    }

    // Obtenir le transform du composant
    FTransform ComponentTransform = meshComp->GetComponentTransform();

    // Transformer les points et la vélocité en coordonnées locales
    FVector3d LocalStart = ComponentTransform.InverseTransformPosition(startPoint);
    FVector3d LocalEnd = ComponentTransform.InverseTransformPosition(endPoint);
    FVector3d LocalVelocity = ComponentTransform.InverseTransformVector(normalizedVelocity);

    // Créer l'arbre spatial
    FDynamicMeshAABBTree3 Spatial(&Mesh);

    // Projeter les points sur la surface
    FVector3d ProjectedStart, ProjectedEnd;
    int32 StartTriangleID, EndTriangleID;

    if (!ProjectPointToMeshSurface(Mesh, Spatial, LocalStart, ProjectedStart, StartTriangleID) ||
        !ProjectPointToMeshSurface(Mesh, Spatial, LocalEnd, ProjectedEnd, EndTriangleID))
    {
        if (_bDebug) UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPathWithVelocity - Failed to project points to mesh surface"));
        return;
    }

    // Trouver les vertices les plus proches
    int32 StartVertexID = FindNearestVertex(Mesh, ProjectedStart);
    int32 EndVertexID = FindNearestVertex(Mesh, ProjectedEnd);

    if (StartVertexID == FDynamicMesh3::InvalidID || EndVertexID == FDynamicMesh3::InvalidID)
    {
        if (_bDebug) UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPathWithVelocity - Failed to find nearest vertices"));
        return;
    }

    // Utiliser l'algorithme de Dijkstra modifié avec vélocité
    TArray<int32> Path;
    if (FindPathDijkstraWithVelocity(Mesh, StartVertexID, EndVertexID, LocalVelocity, velocityInfluence, Path))
    {
        // Convertir le chemin en points world
        outPoints.Reserve(Path.Num() + 2);
        outPoints.Add(startPoint);
    	
    	int i = 0;
        for (int32 VertexID : Path)
        {
            FVector3d LocalPos = Mesh.GetVertex(VertexID);
            FVector WorldPos = ComponentTransform.TransformPosition(LocalPos);

        	float alpha = static_cast<float>(i) / static_cast<float>(Path.Num());
        	FVector intermediatePoint = FMath::Lerp(startPoint, endPoint, alpha);
        	WorldPos.Z = intermediatePoint.Z;
        	
            outPoints.Add(WorldPos);

        	i++;
        }

        outPoints.Add(endPoint);

        if (_bDebug)
        {
            UE_LOG(LogTemp, Log, TEXT("ComputeGeodesicPathWithVelocity - Path found with %d points"), outPoints.Num());
        }
    }
    else
    {
        // Fallback vers la méthode standard
        if (_bDebug) UE_LOG(LogTemp, Warning, TEXT("ComputeGeodesicPathWithVelocity - Falling back to standard geodesic path"));
        ComputeGeodesicPath(meshComp, startPoint, endPoint, outPoints, bDebug, bDebugPoint);
    }
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
        if(_bDebug) UE_LOG(LogTemp, Log, TEXT("ReconstructPath - Start and End are the same vertex: %d"), StartVID);
        return true;
    }
    
    // Vérifier que les vertices existent
    if (!Mesh.IsVertex(StartVID) || !Mesh.IsVertex(EndVID))
    {
        if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("ReconstructPath - Invalid vertices: Start=%d, End=%d"), StartVID, EndVID);
        return false;
    }
    
    // Vérifier que Dijkstra a calculé la distance vers EndVID
    if (!Dijkstra.HasDistance(EndVID))
    {
        if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("ReconstructPath - No path found from %d to %d"), StartVID, EndVID);
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
            if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("ReconstructPath - Failed to find predecessor for vertex %d"), CurrentVID);
            return false;
        }
        
        CurrentVID = PreviousVID;
        Iterations++;
    }
    
    if (Iterations >= MaxIterations)
    {
        if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("ReconstructPath - Maximum iterations reached, possible infinite loop"));
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
    
    if(_bDebug) UE_LOG(LogTemp, Log, TEXT("ReconstructPath - Found path with %d vertices: Start=%d, End=%d"), 
           OutPath.Num(), StartVID, EndVID);
    
    // Loguer le chemin complet pour déboguer
    FString PathStr = TEXT("Path: ");
    for (int32 i = 0; i < OutPath.Num(); i++)
    {
        PathStr += FString::Printf(TEXT("%d"), OutPath[i]);
        if (i < OutPath.Num() - 1) PathStr += TEXT(" -> ");
    }
    if(_bDebug) UE_LOG(LogTemp, Log, TEXT("%s"), *PathStr);
    
    return OutPath.Num() > 1;
}

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
			if(_bDebug) UE_LOG(LogTemp, Log, TEXT("AreVerticesInSameComponent - Found connection after exploring %d nodes"), NodesExplored);
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
    
	if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("AreVerticesInSameComponent - No connection found after exploring %d nodes"), NodesExplored);
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
		if(_bDebug) UE_LOG(LogTemp, Log, TEXT("Found connected component %d with %d vertices"), 
			   ConnectedComponents.Num(), Component.Num());
	}
    
	if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("Mesh has %d connected components"), ConnectedComponents.Num());
    
	// Afficher les détails des composants
	for (int32 i = 0; i < ConnectedComponents.Num(); i++)
	{
		if(_bDebug) UE_LOG(LogTemp, Log, TEXT("Component %d: %d vertices"), i, ConnectedComponents[i].Num());
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
        if(_bDebug) UE_LOG(LogTemp, Error, TEXT("Could not find components for vertices"));
        return false;
    }
    
    // Vérifier si les vertices sont dans le même composant
    if (StartComponent.Contains(EndVID))
    {
        if(_bDebug) UE_LOG(LogTemp, Log, TEXT("Vertices are in the same component, using direct path"));
        return FindPathDijkstra(Mesh, StartVID, EndVID, OutPath);
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
    
    if(_bDebug) UE_LOG(LogTemp, Log, TEXT("Closest bridge: vertex %d to vertex %d (distance: %f)"), 
           ClosestStartVID, ClosestEndVID, MinDistance);
    
    // Si la distance est nulle, c'est que c'est le même vertex, utiliser une approche différente
    if (MinDistance < 0.001f)
    {
        if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("Bridge vertices are identical, creating surface path"));
        return CreateSurfacePath(Mesh, StartVID, EndVID, OutPath);
    }
    
    // Créer le chemin en trois parties
    TArray<int32> PathToStart, PathFromEnd;
    
    // Partie 1: Du vertex de départ au vertex le plus proche dans son composant
    if (StartVID != ClosestStartVID)
    {
        if (!FindPathDijkstra(Mesh, StartVID, ClosestStartVID, PathToStart))
        {
            if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("Could not find path within start component"));
            PathToStart.Add(StartVID);
            PathToStart.Add(ClosestStartVID);
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
            if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("Could not find path within end component"));
            PathFromEnd.Add(ClosestEndVID);
            PathFromEnd.Add(EndVID);
        }
    }
    else
    {
        PathFromEnd.Add(EndVID);
    }
    
    // Combiner les chemins
    OutPath.Reset();
    OutPath.Append(PathToStart);
    
    // Ajouter le pont seulement si les vertices sont différents
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
    
    if(_bDebug) UE_LOG(LogTemp, Log, TEXT("Created bridged path with %d vertices"), OutPath.Num());
    return OutPath.Num() > 0;
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

// Nouvelle fonction pour créer un chemin sur la surface quand les composants se touchent
bool UPSFL_GeometryScript::CreateSurfacePath(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, TArray<int32>& OutPath)
{
	OutPath.Reset();
    
	FVector3d StartPos = Mesh.GetVertex(StartVID);
	FVector3d EndPos = Mesh.GetVertex(EndVID);
    
	if(_bDebug) UE_LOG(LogTemp, Log, TEXT("CreateSurfacePath: From %d (%s) to %d (%s)"), 
		   StartVID, *StartPos.ToString(), EndVID, *EndPos.ToString());
    
	// Créer des points intermédiaires le long de la surface
	const int32 NumIntermediatePoints = 5;
	OutPath.Add(StartVID);
    
	for (int32 i = 1; i < NumIntermediatePoints; i++)
	{
		float Alpha = (float)i / (float)NumIntermediatePoints;
		FVector3d IntermediatePos = FMath::Lerp(StartPos, EndPos, Alpha);
        
		// Trouver le vertex le plus proche de cette position intermédiaire
		int32 ClosestVID = FindNearestVertex(const_cast<FDynamicMesh3&>(Mesh), IntermediatePos);
		if (ClosestVID != FDynamicMesh3::InvalidID && !OutPath.Contains(ClosestVID))
		{
			OutPath.Add(ClosestVID);
		}
	}
    
	if (StartVID != EndVID)
	{
		OutPath.Add(EndVID);
	}
    
	if(_bDebug) UE_LOG(LogTemp, Log, TEXT("Created surface path with %d vertices"), OutPath.Num());
	return OutPath.Num() > 1;
}

#pragma region Cache
//------------------

// Initialisation des variables statiques du cache
TMap<FPathCacheKey, FPathCacheEntry> UPSFL_GeometryScript::PathCache;
int32 UPSFL_GeometryScript::MaxCacheSize = 100;

bool UPSFL_GeometryScript::TryGetCachedPath(const FPathCacheKey& Key, TArray<FVector>& OutPoints)
{
    // Nettoyer le cache expiré périodiquement
    CleanupExpiredCache();
    
    if (FPathCacheEntry* Entry = PathCache.Find(Key))
    {
        // Vérifier si l'entrée n'est pas expirée
        double CurrentTime = FPlatformTime::Seconds();
        if (CurrentTime - Entry->LastAccessTime < CacheExpirationTime)
        {
            Entry->LastAccessTime = CurrentTime; // Mettre à jour le temps d'accès
            OutPoints = Entry->Points;
            return true;
        }
        else
        {
            // L'entrée est expirée, la supprimer
            PathCache.Remove(Key);
        }
    }
    
    return false;
}

void UPSFL_GeometryScript::StoreCachedPath(const FPathCacheKey& Key, const TArray<FVector>& Points)
{
    // Vérifier si le cache est plein
    if (PathCache.Num() >= MaxCacheSize)
    {
        // Supprimer l'entrée la plus ancienne
        FPathCacheKey OldestKey = FPathCacheKey();
        double OldestTime = FPlatformTime::Seconds();
        
        for (const auto& Pair : PathCache)
        {
            if (Pair.Value.LastAccessTime < OldestTime)
            {
                OldestTime = Pair.Value.LastAccessTime;
                OldestKey = Pair.Key;
            }
        }
        
        PathCache.Remove(OldestKey);
    }
    
    // Ajouter la nouvelle entrée
    PathCache.Add(Key, FPathCacheEntry(Points));
}

void UPSFL_GeometryScript::CleanupExpiredCache()
{
    double CurrentTime = FPlatformTime::Seconds();
    
    // Collecter les clés expirées
    TArray<FPathCacheKey> ExpiredKeys;
    for (const auto& Pair : PathCache)
    {
        if (CurrentTime - Pair.Value.LastAccessTime >= CacheExpirationTime)
        {
            ExpiredKeys.Add(Pair.Key);
        }
    }
    
    // Supprimer les entrées expirées
    for (const FPathCacheKey& ExpiredKey : ExpiredKeys)
    {
        PathCache.Remove(ExpiredKey);
    }
}

void UPSFL_GeometryScript::ClearPathCache()
{
	PathCache.Empty();
	UE_LOG(LogTemp, Log, TEXT("Path cache cleared"));
}

void UPSFL_GeometryScript::SetMaxCacheSize(int32 MaxSize)
{
	MaxCacheSize = FMath::Max(1, MaxSize);
    
	// Si le cache actuel est plus grand que la nouvelle limite, le réduire
	while (PathCache.Num() > MaxCacheSize)
	{
		// Supprimer l'entrée la plus ancienne
		FPathCacheKey OldestKey = FPathCacheKey();
		double OldestTime = FPlatformTime::Seconds();
        
		for (const auto& Pair : PathCache)
		{
			if (Pair.Value.LastAccessTime < OldestTime)
			{
				OldestTime = Pair.Value.LastAccessTime;
				OldestKey = Pair.Key;
			}
		}
        
		PathCache.Remove(OldestKey);
	}
}

//------------------
#pragma endregion Cache

#pragma region Triangle
//------------------

FVector3d UPSFL_GeometryScript::GetClosestPointOnTriangle(const FDynamicMesh3& Mesh, int32 TriangleID, const FVector3d& Point)
{
    if (!Mesh.IsTriangle(TriangleID))
    {
        if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("GetClosestPointOnTriangle - Triangle %d is not valid"), TriangleID);
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
        if(_bDebug) UE_LOG(LogTemp, VeryVerbose, TEXT("GetClosestPointOnTriangle - Triangle %d: Point closest to vertex A"), TriangleID);
        return A;
    }

    // Vérifier si le point est dans la région de B
    FVector3d BP = Point - B;
    double d3 = FVector3d::DotProduct(AB, BP);
    double d4 = FVector3d::DotProduct(AC, BP);
    if (d3 >= 0.0 && d4 <= d3)
    {
        if(_bDebug) UE_LOG(LogTemp, VeryVerbose, TEXT("GetClosestPointOnTriangle - Triangle %d: Point closest to vertex B"), TriangleID);
        return B;
    }

    // Vérifier si le point est dans la région de l'arête AB
    double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0)
    {
        double v = d1 / (d1 - d3);
        FVector3d result = A + v * AB;
        if(_bDebug) UE_LOG(LogTemp, VeryVerbose, TEXT("GetClosestPointOnTriangle - Triangle %d: Point closest to edge AB"), TriangleID);
        return result;
    }

    // Vérifier si le point est dans la région de C
    FVector3d CP = Point - C;
    double d5 = FVector3d::DotProduct(AB, CP);
    double d6 = FVector3d::DotProduct(AC, CP);
    if (d6 >= 0.0 && d5 <= d6)
    {
        if(_bDebug) UE_LOG(LogTemp, VeryVerbose, TEXT("GetClosestPointOnTriangle - Triangle %d: Point closest to vertex C"), TriangleID);
        return C;
    }

    // Vérifier si le point est dans la région de l'arête AC
    double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0)
    {
        double w = d2 / (d2 - d6);
        FVector3d result = A + w * AC;
        if(_bDebug) UE_LOG(LogTemp, VeryVerbose, TEXT("GetClosestPointOnTriangle - Triangle %d: Point closest to edge AC"), TriangleID);
        return result;
    }

    // Vérifier si le point est dans la région de l'arête BC
    double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0)
    {
        double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        FVector3d result = B + w * (C - B);
        if(_bDebug) UE_LOG(LogTemp, VeryVerbose, TEXT("GetClosestPointOnTriangle - Triangle %d: Point closest to edge BC"), TriangleID);
        return result;
    }

    // Le point est à l'intérieur du triangle
    double denom = 1.0 / (va + vb + vc);
    double v = vb * denom;
    double w = vc * denom;
    FVector3d result = A + AB * v + AC * w;
    
    if(_bDebug) UE_LOG(LogTemp, VeryVerbose, TEXT("GetClosestPointOnTriangle - Triangle %d: Point inside triangle"), TriangleID);
    
    double Distance = FVector3d::Distance(Point, result);
    if(_bDebug) UE_LOG(LogTemp, Log, TEXT("GetClosestPointOnTriangle - Triangle %d: Input=%s -> Output=%s (distance=%f)"), 
           TriangleID, *Point.ToString(), *result.ToString(), Distance);
    
    return result;
}

int32 UPSFL_GeometryScript::FindNearestTriangleBruteForce(const FDynamicMesh3& Mesh, const FVector3d& Point)
{
    int32 NearestTriID = FDynamicMesh3::InvalidID;
    double MinDistSqr = TNumericLimits<double>::Max();
    int32 ValidTriangleCount = 0;
    
    if(_bDebug) UE_LOG(LogTemp, Log, TEXT("FindNearestTriangleBruteForce - Point: %s, Total triangles: %d"), 
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
            if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("Triangle %d returned invalid closest point"), TriID);
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
            
            if(_bDebug) UE_LOG(LogTemp, Log, TEXT("Triangle %d: A=%s B=%s C=%s"), 
                   TriID, *A.ToString(), *B.ToString(), *C.ToString());
            if(_bDebug) UE_LOG(LogTemp, Log, TEXT("Triangle %d: Closest=%s, Distance=%f"), 
                   TriID, *ClosestPoint.ToString(), FMath::Sqrt(DistSqr));
        }
        DebugCount++;

        if (DistSqr < MinDistSqr)
        {
            MinDistSqr = DistSqr;
            NearestTriID = TriID;
            
            if(_bDebug) UE_LOG(LogTemp, Log, TEXT("New nearest triangle found: %d (distance: %f)"), 
                   TriID, FMath::Sqrt(MinDistSqr));
        }
    }

    if(_bDebug) UE_LOG(LogTemp, Log, TEXT("FindNearestTriangleBruteForce FINAL - Checked %d triangles, nearest: %d (distance: %f)"), 
           ValidTriangleCount, NearestTriID, FMath::Sqrt(MinDistSqr));

    return NearestTriID;
}

int32 UPSFL_GeometryScript::FindNearestVertex(FDynamicMesh3& Mesh, const FVector3d& Point)
{
	int32 NearestVID = FDynamicMesh3::InvalidID;
	double MinDistance = TNumericLimits<double>::Max();

	if(_bDebug) UE_LOG(LogTemp, Log, TEXT("FindNearestVertex - Looking for nearest vertex to: %s"), *Point.ToString());

	for (int32 VID : Mesh.VertexIndicesItr())
	{
		FVector3d VertexPos = Mesh.GetVertex(VID);
		double Distance = FVector3d::DistSquared(Point, VertexPos);
        
		if (Distance < MinDistance)
		{
			MinDistance = Distance;
			NearestVID = VID;
			if(_bDebug) UE_LOG(LogTemp, Log, TEXT("New nearest vertex: %d at %s (distance: %f)"), VID, *VertexPos.ToString(), FMath::Sqrt(Distance));
		}
	}

	if(_bDebug) UE_LOG(LogTemp, Log, TEXT("FindNearestVertex - Final result: vertex %d at distance %f"), NearestVID, FMath::Sqrt(MinDistance));
	return NearestVID;
}

//------------------

#pragma endregion Triangle

#pragma region Vertex
//------------------

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
		if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("FindMinDistanceVertex - No vertices with finite distance remaining (total unvisited: %d)"), UnvisitedVertices.Num());
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
			if(_bDebug) UE_LOG(LogTemp, Log, TEXT("Successfully converted ProceduralMesh - Vertices: %d, Triangles: %d"), 
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
			if(_bDebug) UE_LOG(LogTemp, Log, TEXT("Successfully converted StaticMesh - Vertices: %d, Triangles: %d"), OutMesh.VertexCount(), OutMesh.TriangleCount());
		}
		return bSuccess;
	}

	if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("Unsupported mesh component type: %s"), *MeshComp->GetClass()->GetName());
	return false;
}

//------------------
#pragma endregion MeshConverter

#pragma region Djikstra
//------------------


// Fonction helper pour essayer Dijkstra
bool UPSFL_GeometryScript::TryDijkstraPath(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, const FTransform& ComponentTransform, TArray<FVector>& outPoints)
{
	// Vérifier la connectivité rapidement
	if (!AreVerticesConnected(Mesh, StartVID, EndVID))
	{
		if(_bDebug) UE_LOG(LogTemp, Log, TEXT("TryDijkstraPath - Vertices %d and %d are not connected"), StartVID, EndVID);
		return false;
	}
    
	// Lancer Dijkstra
	TMeshDijkstra<FDynamicMesh3> Dijkstra(&Mesh);
	TArray<TMeshDijkstra<FDynamicMesh3>::FSeedPoint> SeedPoints;
	SeedPoints.Add(TMeshDijkstra<FDynamicMesh3>::FSeedPoint(StartVID, 0.0));
    
	Dijkstra.ComputeToTargetPoint(SeedPoints, EndVID);
    
	if (!Dijkstra.HasDistance(EndVID))
	{
		if(_bDebug) UE_LOG(LogTemp, Log, TEXT("TryDijkstraPath - Dijkstra failed to find path"));
		return false;
	}
    
	// Reconstruire le chemin
	TArray<int32> Path;
	if (!ReconstructPath(Dijkstra, Mesh, StartVID, EndVID, Path))
	{
		if(_bDebug) UE_LOG(LogTemp, Log, TEXT("TryDijkstraPath - Failed to reconstruct path"));
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
    
	if(_bDebug) UE_LOG(LogTemp, Log, TEXT("TryDijkstraPath - Success! Generated %d points"), outPoints.Num());
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
        if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("FindPathDijkstra - Invalid vertex IDs: Start=%d, End=%d"), StartVID, EndVID);
        return false;
    }
    
    // D'abord, vérifier si les vertices sont dans le même composant connecté
    if (!AreVerticesInSameComponent(Mesh, StartVID, EndVID))
    {
        if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("FindPathDijkstra - Vertices %d and %d are not in the same connected component"), StartVID, EndVID);
        return false;
    }
    
    if(_bDebug) UE_LOG(LogTemp, Log, TEXT("FindPathDijkstra - Starting from vertex %d to %d"), StartVID, EndVID);
    
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
    
    if(_bDebug) UE_LOG(LogTemp, Log, TEXT("FindPathDijkstra - Initialized %d vertices"), NodeMap.Num());
    
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
            if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("FindPathDijkstra - No valid vertex found at iteration %d"), Iterations);
            break;
        }
        
        // Vérifier si la distance est infinie
        if (NodeMap[CurrentVID].Distance == TNumericLimits<float>::Max())
        {
            if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("FindPathDijkstra - Reached infinite distance at iteration %d, no more connected vertices"), Iterations);
            break;
        }
        
        // Marquer comme visité
        NodeMap[CurrentVID].bVisited = true;
        UnvisitedVertices.Remove(CurrentVID);
        
        // Si nous avons atteint la destination
        if (CurrentVID == EndVID)
        {
            if(_bDebug) UE_LOG(LogTemp, Log, TEXT("FindPathDijkstra - Reached destination in %d iterations"), Iterations);
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
                    if(_bDebug) UE_LOG(LogTemp, Log, TEXT("Updated vertex %d: distance=%f, parent=%d"), 
                           NeighborVID, NewDistance, CurrentVID);
                }
            }
        }
        
        // Log périodique avec plus d'informations
        if (Iterations % 50 == 0)
        {
            if(_bDebug) UE_LOG(LogTemp, Log, TEXT("FindPathDijkstra - Iteration %d: vertex %d (dist=%f), neighbors updated=%d, finite vertices=%d, remaining=%d"), 
                   Iterations, CurrentVID, NodeMap[CurrentVID].Distance, NeighborsUpdated, FiniteDistanceVertices, UnvisitedVertices.Num());
        }
    }
    
    if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("FindPathDijkstra - Failed to find path after %d iterations"), Iterations);
    return false;
}

bool UPSFL_GeometryScript::ReconstructPathDijkstra(const TMap<int32, FMeshDijkstraNode>& NodeMap, int32 StartVID, int32 EndVID, TArray<int32>& OutPath)
{
    OutPath.Reset();
    
    // Vérifier que nous avons bien une solution
    if (!NodeMap.Contains(EndVID) || NodeMap[EndVID].Distance == TNumericLimits<float>::Max())
    {
        if(_bDebug) UE_LOG(LogTemp, Warning, TEXT("ReconstructPathDijkstra - No valid path to end vertex"));
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
            if(_bDebug) UE_LOG(LogTemp, Error, TEXT("ReconstructPathDijkstra - Infinite loop detected"));
            return false;
        }
    }
    
    // Inverser le chemin pour qu'il aille du début à la fin
    OutPath.Reserve(ReversePath.Num());
    for (int32 i = ReversePath.Num() - 1; i >= 0; i--)
    {
        OutPath.Add(ReversePath[i]);
    }
    
    if(_bDebug) UE_LOG(LogTemp, Log, TEXT("ReconstructPathDijkstra - Reconstructed path with %d vertices"), OutPath.Num());
    
    // Calculer la distance totale pour debug
    float TotalDistance = NodeMap[EndVID].Distance;
    if(_bDebug) UE_LOG(LogTemp, Log, TEXT("ReconstructPathDijkstra - Total path distance: %f"), TotalDistance);
    
    return OutPath.Num() > 0;
}

bool UPSFL_GeometryScript::FindPathDijkstraWithVelocity(const FDynamicMesh3& Mesh, int32 StartVID, int32 EndVID, const FVector3d& velocity, float velocityInfluence, TArray<int32>& OutPath)
{
   if (!Mesh.IsVertex(StartVID) || !Mesh.IsVertex(EndVID))
    {
        return false;
    }

    if (StartVID == EndVID)
    {
        OutPath = {StartVID};
        return true;
    }

    // Initialiser les structures de données pour Dijkstra
    TMap<int32, FMeshDijkstraNode> NodeMap;
    TSet<int32> UnvisitedVertices;

    // Initialiser tous les vertices
    for (int32 VertexID : Mesh.VertexIndicesItr())
    {
        NodeMap.Add(VertexID, FMeshDijkstraNode(VertexID));
        UnvisitedVertices.Add(VertexID);
    }

    // Définir la distance du point de départ à 0
    NodeMap[StartVID].Distance = 0.0f;

    // Algorithme de Dijkstra principal
    while (UnvisitedVertices.Num() > 0)
    {
        // Trouver le vertex non visité avec la distance minimale
        int32 CurrentVertex = FindMinDistanceVertex(NodeMap, UnvisitedVertices);
        
        if (CurrentVertex == FDynamicMesh3::InvalidID || NodeMap[CurrentVertex].Distance == TNumericLimits<float>::Max())
        {
            break; // Pas de chemin possible
        }

        // Marquer comme visité
        UnvisitedVertices.Remove(CurrentVertex);
        NodeMap[CurrentVertex].bVisited = true;

        // Si nous avons atteint la destination, nous pouvons arrêter
        if (CurrentVertex == EndVID)
        {
            break;
        }

        // Examiner tous les voisins
        for (int32 EdgeID : Mesh.VtxEdgesItr(CurrentVertex))
        {
            // Utiliser GetEdge pour obtenir les vertices de l'edge
            FDynamicMesh3::FEdge Edge = Mesh.GetEdge(EdgeID);
            
            // Déterminer quel vertex est le voisin (l'autre vertex de l'edge)
            int32 NeighborVertex;
            if (Edge.Vert.A == CurrentVertex)
            {
                NeighborVertex = Edge.Vert.B;
            }
            else if (Edge.Vert.B == CurrentVertex)
            {
                NeighborVertex = Edge.Vert.A;
            }
            else
            {
                // Cet edge ne devrait pas être dans la liste du CurrentVertex
                continue;
            }
            
            if (NodeMap[NeighborVertex].bVisited)
            {
                continue;
            }

            // Calculer le coût avec biais de vélocité
            float EdgeCost = CalculateVelocityBiasedVertexCost(Mesh, CurrentVertex, NeighborVertex, velocity, velocityInfluence);
            float NewDistance = NodeMap[CurrentVertex].Distance + EdgeCost;

            // Mettre à jour si nous avons trouvé un meilleur chemin
            if (NewDistance < NodeMap[NeighborVertex].Distance)
            {
                NodeMap[NeighborVertex].Distance = NewDistance;
                NodeMap[NeighborVertex].ParentID = CurrentVertex;
            }
        }
    }

    // Reconstruire le chemin
    return ReconstructPathDijkstra(NodeMap, StartVID, EndVID, OutPath);
}

//------------------
#pragma endregion Djikstra

#pragma region Velocity
//------------------

float UPSFL_GeometryScript::CalculateVelocityBiasedCost(const FDynamicMesh3& Mesh, int32 EdgeID, const FVector3d& velocity, float velocityInfluence)
{
	if (!Mesh.IsEdge(EdgeID))
	{
		return TNumericLimits<float>::Max();
	}

	// Obtenir l'edge
	FDynamicMesh3::FEdge Edge = Mesh.GetEdge(EdgeID);
    
	// Calculer la direction de l'edge
	FVector3d Vertex1Pos = Mesh.GetVertex(Edge.Vert.A);
	FVector3d Vertex2Pos = Mesh.GetVertex(Edge.Vert.B);
	FVector3d EdgeDirection = (Vertex2Pos - Vertex1Pos).GetSafeNormal();
    
	// Calculer la distance de base de l'edge
	float BaseDistance = FVector3d::Dist(Vertex1Pos, Vertex2Pos);

	// Si la vélocité est nulle, retourner la distance de base
	if (velocity.Length() < SMALL_NUMBER)
	{
		return BaseDistance;
	}

	// Calculer l'alignement entre la direction de l'edge et la vélocité
	float DotProduct = FVector3d::DotProduct(EdgeDirection, velocity);
    
	// Convertir le dot product en facteur de coût
	// DotProduct = 1 (même direction) -> facteur de réduction
	// DotProduct = -1 (direction opposée) -> facteur d'augmentation
	// DotProduct = 0 (perpendiculaire) -> facteur neutre
    
	float AlignmentFactor = 1.0f - (DotProduct * velocityInfluence);
    
	// S'assurer que le facteur reste dans des limites raisonnables
	AlignmentFactor = FMath::Clamp(AlignmentFactor, 0.1f, 2.0f);
    
	return BaseDistance * AlignmentFactor;
}

float UPSFL_GeometryScript::CalculateVelocityBiasedVertexCost(const FDynamicMesh3& Mesh, int32 FromVertexID, int32 ToVertexID, const FVector3d& velocity, float velocityInfluence)
{
	// Calculer la distance euclidienne de base
	FVector3d FromPos = Mesh.GetVertex(FromVertexID);
	FVector3d ToPos = Mesh.GetVertex(ToVertexID);
	FVector3d EdgeDirection = (ToPos - FromPos).GetSafeNormal();
	float BaseDistance = FVector3d::Dist(FromPos, ToPos);

	// Si la vélocité est nulle, retourner la distance de base
	if (velocity.Length() < SMALL_NUMBER)
	{
		return BaseDistance;
	}

	// Calculer l'alignement entre la direction de l'edge et la vélocité
	float DotProduct = FVector3d::DotProduct(EdgeDirection, velocity);
    
	// Convertir le dot product en facteur de coût
	// DotProduct = 1 (même direction) -> facteur de réduction
	// DotProduct = -1 (direction opposée) -> facteur d'augmentation
	// DotProduct = 0 (perpendiculaire) -> facteur neutre
    
	float AlignmentFactor = 1.0f - (DotProduct * velocityInfluence);
    
	// S'assurer que le facteur reste dans des limites raisonnables
	AlignmentFactor = FMath::Clamp(AlignmentFactor, 0.1f, 2.0f);
    
	return BaseDistance * AlignmentFactor;
}

//------------------
#pragma endregion Velocity
	












