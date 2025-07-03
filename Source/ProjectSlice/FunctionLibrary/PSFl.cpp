#include "PSFl.h"

#include "KismetTraceUtils.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "StaticMeshAttributes.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "ProjectSlice/Components/PC/PS_PlayerCameraComponent.h"

class UProceduralMeshComponent;
struct FProcMeshTangent;
class AProjectSliceCharacter;

#pragma region Utilities

bool UPSFl::FindClosestPointOnActor(const AActor* actorToTest, const FVector& fromWorldLocation, FVector& outClosestPoint, const bool bDebug)
{
	if (!IsValid(actorToTest)) return false;

	bool bFoundPoint = false;
	//float bestDist = TNumericLimits<float>().Max();
    
	TArray<UActorComponent*> outComps;
	actorToTest->GetComponents(UMeshComponent::StaticClass(), outComps, true);
    
	for (UActorComponent* currentComp : outComps)
	{
		if (UMeshComponent* meshComp = Cast<UMeshComponent>(currentComp))
		{
			FVector currentPoint;
			const float currentDist = meshComp->GetClosestPointOnCollision(fromWorldLocation, currentPoint);
			
			if (currentDist >= 0/* && currentDist < bestDist*/)
			{
				/*bestDist = currentDist;*/
				outClosestPoint = currentPoint;
				bFoundPoint = true;
			}
			
			if (bDebug) DrawDebugPoint(actorToTest->GetWorld(), outClosestPoint, 10.f, FColor::Emerald, true, 1.0f, 100);
		}
	}
    
	return bFoundPoint;
}

FVector UPSFl::FindNearestSurfacePoint(const UPrimitiveComponent* targetComponent, const FVector& insideLocation, const float sweepDistance)
{
    if (!IsValid(targetComponent)) return FVector::ZeroVector;
	
	static const FVector directions[] = {
		// Cardinal Directions 
		FVector(1.0f, 0.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f),
		FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f),
		FVector(0.0f, 0.0f, 1.0f), FVector(0.0f, 0.0f, -1.0f),

		// Diagonal Directions (pre-normalized)
		FVector(0.70710678f, 0.70710678f, 0.0f),
		FVector(-0.70710678f, 0.70710678f, 0.0f),
		FVector(0.70710678f, -0.70710678f, 0.0f),
		FVector(-0.70710678f, -0.70710678f, 0.0f)
	};
	
    static const FName TraceTag = TEXT("FindNearestSurface");
    FCollisionQueryParams Params(TraceTag, false);
    Params.AddIgnoredComponent(targetComponent);
    
    FVector closestHit = FVector::ZeroVector;
    float minDistanceSquared = FLT_MAX;
	
    const UWorld* World = targetComponent->GetWorld();
    if (!World) return FVector::ZeroVector;
	
    constexpr float EarlyExitThreshold = 1.0f; // Distance très courte en unités Unreal
    constexpr float EarlyExitThresholdSquared = EarlyExitThreshold * EarlyExitThreshold;
    
    for (const FVector& dir : directions)
    {
        FHitResult Hit;
        const FVector end = insideLocation + dir * sweepDistance;
        
        if (World->LineTraceSingleByChannel(Hit, insideLocation, end, ECC_Visibility, Params))
        {
            const float distSquared = FVector::DistSquared(Hit.ImpactPoint, insideLocation);
            
            if (distSquared < minDistanceSquared)
            {
                minDistanceSquared = distSquared;
                closestHit = Hit.ImpactPoint;
            	
                if (distSquared < EarlyExitThresholdSquared)
                {
                    break;
                }
            }
        }
    }
    DrawDebugPoint(targetComponent->GetWorld(), closestHit, 10.f, FColor::Turquoise, true, 1.0f, 100);
	
    return (minDistanceSquared < FLT_MAX) ? closestHit : FVector::ZeroVector;
}


FVector UPSFl::ClampVelocity(FVector currentVelocity, const FVector& targetVelocity, const float maxVelocity, const bool bDebug )
{
	FVector clampedVel = currentVelocity;
	if(targetVelocity.Length() > maxVelocity)
	{
		currentVelocity.Normalize();
		clampedVel = currentVelocity * maxVelocity;
		
		if(bDebug)
			UE_LOG(LogTemp, Warning, TEXT("%S :: Clamp Max Velocity"), __FUNCTION__);
	}
	return clampedVel;
}

FVector UPSFl::ClampVelocity(FVector& startVelocity, FVector currentVelocity, const FVector& targetVelocity, const float maxVelocity, const bool bDebug )
{
	FVector clampedVel = targetVelocity;
	if(targetVelocity.Length() > maxVelocity)
	{
		currentVelocity.Normalize();
		clampedVel = currentVelocity * maxVelocity;

		//Clamp base velocity too
		if(startVelocity.Length() > maxVelocity)
			startVelocity = clampedVel;

		if(bDebug)
			UE_LOG(LogTemp, Warning, TEXT("%S :: Clamp Max Velocity"), __FUNCTION__);
	}
	return clampedVel;
}

FVector UPSFl::GetWorldInputDirection(const UPS_PlayerCameraComponent* cameraInstance, const FVector2D moveInput)
{
	FVector worldInputDirection = cameraInstance->GetRightVector() * moveInput.X + cameraInstance->GetForwardVector() * moveInput.Y;
	worldInputDirection.Z = 0;
	worldInputDirection.Normalize();
	return worldInputDirection;
}

FVector UPSFl::GetScreenCenterWorldLocation(const APlayerController* const PlayerController)
{
	if (!PlayerController) return FVector::ZeroVector;

	// Get viewport size (screen resolution)
	int32 ScreenWidth, ScreenHeight;
	PlayerController->GetViewportSize(ScreenWidth, ScreenHeight);

	// Calculate screen center
	float ScreenCenterX = ScreenWidth * 0.5f;
	float ScreenCenterY = ScreenHeight * 0.5f;

	// World location and direction
	FVector WorldLocation;
	FVector WorldDirection;

	// Convert screen position to world space
	if (PlayerController->DeprojectScreenPositionToWorld(ScreenCenterX, ScreenCenterY, WorldLocation, WorldDirection))
	{
		return WorldLocation; // The world position at the screen center
	}

	return FVector::ZeroVector; // Return zero vector if conversion fails
}

FVector UPSFl::GetWorldPointInFrontOfCamera(const APlayerController* const PlayerController, const float Distance)
{
	if (!IsValid(PlayerController)) return FVector::ZeroVector;

	// Get viewport size (screen resolution)
	int32 ScreenWidth, ScreenHeight;
	PlayerController->GetViewportSize(ScreenWidth, ScreenHeight);

	// Calculate screen center
	float ScreenCenterX = ScreenWidth * 0.5f;
	float ScreenCenterY = ScreenHeight * 0.5f;

	// World location and direction
	FVector WorldLocation;
	FVector WorldDirection;

	// Convert screen position to world space
	if (PlayerController->DeprojectScreenPositionToWorld(ScreenCenterX, ScreenCenterY, WorldLocation, WorldDirection))
	{
		return WorldLocation + (WorldDirection * Distance); // The world position at the screen center
	}

	return FVector::ZeroVector; // Return zero vector if conversion fails
}

float UPSFl::GetObjectUnifiedMass(UPrimitiveComponent* const comp, const bool bDebug)
{
	if(!IsValid(comp)) return 1.0f;
	
	UPhysicalMaterial* physMat = comp->BodyInstance.GetSimplePhysicalMaterial();

	const float output = ((comp->GetMass() * comp->GetMassScale() * (IsValid(physMat) ? physMat->Density : 1.0f)) / comp->GetComponentScale().Length());
	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: %s output %f"),__FUNCTION__, *comp->GetName(),output);
	
	return output ;

}

UStaticMesh* UPSFl::CreateMeshFromProcMesh(UProceduralMeshComponent* procMesh)
{
	 // if (!procMesh) return nullptr;
  //
  //   // Create a new Static Mesh
  //   UStaticMesh* NewStaticMesh = NewObject<UStaticMesh>(procMesh->GetOuter(), UStaticMesh::StaticClass(), NAME_None, RF_Public | RF_Standalone);
  //   if (!NewStaticMesh) return nullptr;
  //
  //   // Create Mesh Description
  //   FMeshDescription MeshDescription;
  //   FStaticMeshAttributes Attributes(MeshDescription);
  //   Attributes.Register();

    // Get the Static Mesh Body Setup
    // NewStaticMesh->GetMeshDescription(0) = MeshDescription;
    // FMeshDescription* MeshDesc = NewStaticMesh->GetMeshDescription(0);
    // if (!MeshDesc) return nullptr;
    //
    // // Create Mesh Section Data
    // for (int32 SectionIndex = 0; SectionIndex < procMesh->GetNumSections(); SectionIndex++)
    // {
    //     FProcMeshSection* Section = procMesh->GetProcMeshSection(SectionIndex);
    //     if (!Section) continue;
    //
    //     TArray<FVector>& Vertices = Section->ProcVertexBuffer;
    //     TArray<int32>& Triangles = Section->ProcIndexBuffer;
    //     TArray<FVector>& Normals = Section->ProcNormalBuffer;
    //     TArray<FVector2D>& UVs = Section->ProcUVBuffer;
    //     TArray<FProcMeshTangent>& Tangents = Section->ProcTangentBuffer;
    //
    //     // Create a new polygon group for this section
    //     FPolygonGroupID PolygonGroup = MeshDesc->CreatePolygonGroup();
    //     
    //     // Add Vertices
    //     TArray<FVertexID> VertexIDs;
    //     for (const FVector& Vertex : Vertices)
    //     {
    //         FVertexID VertexID = MeshDesc->CreateVertex();
    //         MeshDesc->GetVertexPositions()[VertexID] = Vertex;
    //         VertexIDs.Add(VertexID);
    //     }
    //
    //     // Add Triangles
    //     for (int32 i = 0; i < Triangles.Num(); i += 3)
    //     {
    //         TArray<FVertexInstanceID> VertexInstances;
    //         for (int32 j = 0; j < 3; j++)
    //         {
    //             FVertexInstanceID VertexInstance = MeshDesc->CreateVertexInstance(VertexIDs[Triangles[i + j]]);
    //             VertexInstances.Add(VertexInstance);
    //
    //             // Assign normals, UVs, and tangents
    //             MeshDesc->GetVertexInstanceNormals()[VertexInstance] = Normals[i + j];
    //             MeshDesc->GetVertexInstanceUVs()[VertexInstance].SetNum(1);
    //             MeshDesc->GetVertexInstanceUVs()[VertexInstance][0] = UVs[i + j];
    //             MeshDesc->GetVertexInstanceTangents()[VertexInstance] = Tangents[i + j].TangentX;
    //         }
    //
    //         MeshDesc->CreatePolygon(PolygonGroup, VertexInstances);
    //     }
    // }

    // Commit the Mesh Description and build the Static Mesh
    // NewStaticMesh->CommitMeshDescription(0);
    // NewStaticMesh->PostEditChange();

	// return NewStaticMesh;
	return nullptr;
}

#pragma endregion Utilities

FCollisionQueryParams UPSFl::CustomConfigureCollisionParams(FName TraceTag, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, bool bIgnoreSelf, const UObject* WorldContextObject)
{
	FCollisionQueryParams Params(TraceTag, SCENE_QUERY_STAT_ONLY(KismetTraceUtils), bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		const AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			const UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	return Params;
}

void UPSFl::SweepConeMultiByChannel(
		UWorld* World,
		FVector ConeApex,
		FVector ConeDirection,
		float ConeAngleDegrees,
		float ConeLength,
		float StepInterval,
		TArray<FHitResult>& OutHits,
		ECollisionChannel TraceChannel,
		const TArray<AActor*>& ActorsToIgnore,
		bool bDebug)
{
	if (!World) return;
    
    //Normaliser && check validity
    ConeDirection.Normalize();
    if (ConeDirection.IsNearlyZero() || ConeLength <= 0.0f || StepInterval <= 0.0f) return;
    
    //Pré-calculate const var
    const float ConeAngleRadians = FMath::DegreesToRadians(ConeAngleDegrees);
    const float StepSize = ConeLength / StepInterval;
    const int32 NumSteps = FMath::FloorToInt(StepInterval);
    
    //Reserve space for avoid reallocation
    OutHits.Reserve(NumSteps * 8); // Average estimated hits per step
    
    // Re-use collision params
    static const FName SphereTraceMultiName(TEXT("SweepTraceCone"));
    const FCollisionQueryParams QueryParams = CustomConfigureCollisionParams(
        SphereTraceMultiName, false, ActorsToIgnore, true, World);
    
    // Re-use table for avoid multiple allocation
    TArray<FHitResult> SphereHitResults;
    SphereHitResults.Reserve(16);
    
    //Pre-calculate const var
    const FQuat Identity = FQuat::Identity;

	// Sweep multiple spheres along the cone's length
    for (int32 Step = 0; Step < NumSteps; ++Step)
    {
    	// Calculate the distance along the cone
        const float Distance = StepSize * Step;
        
        // Skip spheres with radius 0 on begin
        if (Distance < KINDA_SMALL_NUMBER) continue;
        
        const float Radius = Distance * ConeAngleRadians;
        const FVector SphereCenter = ConeApex + (ConeDirection * Distance);
        
        //Make shape
        const FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
        
        //Reset Tab
        SphereHitResults.Reset();
        
        if (World->SweepMultiByChannel(SphereHitResults, SphereCenter, SphereCenter, 
            Identity, TraceChannel, Sphere, QueryParams))
        {
            OutHits.Append(SphereHitResults);
        }
        
        // Debug
        if (bDebug)
        {
            DrawDebugSphere(World, SphereCenter, Radius, 8, FColor::Red, false, 1.0f);
        }
    }
	
    if (bDebug)
    {
        DrawDebugCone(World, ConeApex, ConeDirection, ConeLength, 
            ConeAngleRadians, ConeAngleRadians, 8, FColor::Green, false, 5.0f);
        
        const FVector ConeEnd = ConeApex + (ConeDirection * ConeLength);
        const float EndRadius = ConeLength * ConeAngleRadians;
        DrawDebugSphere(World, ConeEnd, EndRadius, 8, FColor::Green, false, 1.0f);
    }
}

void UPSFl::SweepConeMultiByChannel(
		UWorld* World,
		FVector ConeApex,
		FVector ConeDirection,
		float ConeAngleDegrees,
		float ConeLength,
		float StepInterval,
		TArray<UPrimitiveComponent*>& OutHitComponents,
		ECollisionChannel TraceChannel,
		const TArray<AActor*>& ActorsToIgnore,
		bool bDebug)
{	
	 if (!World) return;
    
	//Normaliser && check validity
    ConeDirection.Normalize();
    if (ConeDirection.IsNearlyZero() || ConeLength <= 0.0f || StepInterval <= 0.0f) return;

	//Pré-calculate const var
    const float ConeAngleRadians = FMath::DegreesToRadians(ConeAngleDegrees);
    const float StepSize = ConeLength / StepInterval;
    const int32 NumSteps = FMath::FloorToInt(StepInterval);
    
    // Use TSet for avoid doubles
    TSet<UPrimitiveComponent*> UniqueComponents;
    UniqueComponents.Reserve(NumSteps * 4); // Estimation

	// Re-use collision params
    static const FName SphereTraceMultiName(TEXT("SweepTraceCone"));
    const FCollisionQueryParams QueryParams = CustomConfigureCollisionParams(
        SphereTraceMultiName, false, ActorsToIgnore, true, World);

	// Re-use table for avoid multiple allocation
    TArray<FHitResult> SphereHitResults;
    SphereHitResults.Reserve(16);

	//Pre-calculate const var
    const FQuat Identity = FQuat::Identity;

	// Sweep multiple spheres along the cone's length
    for (int32 Step = 0; Step < NumSteps; ++Step)
    {
    	// Calculate the distance along the cone
        const float Distance = StepSize * Step;

    	// Skip spheres with radius 0 on begin
        if (Distance < KINDA_SMALL_NUMBER) continue;
        
        const float Radius = Distance * ConeAngleRadians;
        const FVector SphereCenter = ConeApex + (ConeDirection * Distance);

    	//Make shape
        const FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);

    	//Reset Tab
        SphereHitResults.Reset();
        
        if (World->SweepMultiByChannel(SphereHitResults, SphereCenter, SphereCenter, 
            Identity, TraceChannel, Sphere, QueryParams))
        {
            for (const FHitResult& Hit : SphereHitResults)
            {
                if (UPrimitiveComponent* HitComp = Hit.GetComponent())
                {
                    if (IsValid(HitComp))
                    {
                        UniqueComponents.Add(HitComp);

                    	// Debug
                        if (bDebug)
                        {
                            DrawDebugPoint(World, Hit.ImpactPoint, 10.0f, 
                                FColor::Purple, false, 2.0f, 5.0f);
                        }
                    }
                }
            }
        }
    }
    
    //Convert Set to Array
    OutHitComponents.Reset();
    OutHitComponents.Reserve(UniqueComponents.Num());
    for (UPrimitiveComponent* Comp : UniqueComponents)
    {
        OutHitComponents.Add(Comp);
    }
    
    // Debug
    if (bDebug)
    {
        DrawDebugCone(World, ConeApex, ConeDirection, ConeLength, 
            ConeAngleRadians, ConeAngleRadians, 8, FColor::Green, false, 5.0f);
    }
}

#pragma region Cooldown
//------------------

void UPSFl::StartCooldown(UWorld* World, float coolDownDuration,UPARAM(ref) FTimerHandle& timerHandler)
{
	if (!World) return;
	
	FTimerDelegate timerDelegate;
	World->GetTimerManager().SetTimer(timerHandler, timerDelegate, coolDownDuration, false);
}

//------------------
#pragma endregion Cooldown




