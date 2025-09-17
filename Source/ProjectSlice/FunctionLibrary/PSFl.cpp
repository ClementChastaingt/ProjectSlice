#include "PSFl.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "ProjectSlice/Components/PC/PS_PlayerCameraComponent.h"

#include "DynamicMesh/MeshIndexUtil.h"
#include "MeshQueries.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Parameterization/MeshDijkstra.h"

class UProceduralMeshComponent;
struct FProcMeshTangent;
class AProjectSliceCharacter;

#pragma region Utilities
//------------------

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

float UPSFl::GetObjectUnifiedMass(UPrimitiveComponent* const comp, const bool bDebug)
{
	if(!IsValid(comp) || comp->GetMobility() != EComponentMobility::Movable) return 1.0f;
	
	UPhysicalMaterial* physMat = comp->BodyInstance.GetSimplePhysicalMaterial();

	const float output = ((comp->GetMass() * comp->GetMassScale() * (IsValid(physMat) ? physMat->Density : 1.0f)) / comp->GetComponentScale().Length());
	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: %s output %f"),__FUNCTION__, *comp->GetName(),output);
	
	return output ;

}

void UPSFl::SetDilatedRealTimeTimer(UWorld* World, FTimerHandle& InOutHandle, const FTimerDelegate& InDelegate,
	float InRate, bool bLoop, float InFirstDelay, float CustomDilation)
{
	
}

//------------------
#pragma endregion Utilities

#pragma region Time
//------------------

//Equivalent Delay node RealTimed
class FDelayRealTimeAction : public FPendingLatentAction
{
public:
	double TargetTime;
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;

	FDelayRealTimeAction(float Duration, const FLatentActionInfo& LatentInfo, double StartTime)
		: TargetTime(StartTime + Duration)
		, ExecutionFunction(LatentInfo.ExecutionFunction)
		, OutputLink(LatentInfo.Linkage)
		, CallbackTarget(LatentInfo.CallbackTarget)
	{}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		UWorld* World = GWorld; // ou via Response.GetWorld()
		double Now = World ? World->GetRealTimeSeconds() : 0.0;

		Response.FinishAndTriggerIf(Now >= TargetTime, ExecutionFunction, OutputLink, CallbackTarget);
	}
};

void UPSFl::DelayRealTime(const UObject* WorldContextObject, float Duration, FLatentActionInfo LatentInfo)
{
	if (UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject))
	{
		FLatentActionManager& LatentMgr = World->GetLatentActionManager();
		if (LatentMgr.FindExistingAction<FDelayRealTimeAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
		{
			LatentMgr.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID,
				new FDelayRealTimeAction(Duration, LatentInfo, World->GetRealTimeSeconds()));
		}
	}
}

//Equivalent Custom RealTimed de SetTimer
class FRealTimeTimerManager
{
public:
    // Structure interne pour stocker un timer
    struct FRealTimeTimer
    {
        FTimerDelegate Delegate;
        double TargetTime;
        float Rate;
        bool bLoop;
        float CustomDilation;
        bool bActive = true;
    };

    // Map des timers actifs
    static TMap<FTimerHandle, TSharedPtr<FRealTimeTimer>> ActiveTimers;

    // Tick global à appeler chaque frame (par ex. via TickComponent ou Tick du GameMode)
    static void TickTimers(UWorld* World)
    {
        double Now = World ? World->GetRealTimeSeconds() : 0.0;
        if (!World) return;

        for (auto It = ActiveTimers.CreateIterator(); It; ++It)
        {
            TSharedPtr<FRealTimeTimer> Timer = It.Value();
            if (!Timer->bActive) continue;

            if (Now >= Timer->TargetTime)
            {
                Timer->Delegate.ExecuteIfBound();

                if (Timer->bLoop && Timer->Rate > 0.f)
                {
                    Timer->TargetTime = Now + (Timer->Rate / Timer->CustomDilation);
                }
                else
                {
                    Timer->bActive = false;
                    It.RemoveCurrent();
                }
            }
        }
    }

    static void SetTimer(
        FTimerHandle& InOutHandle,
        const FTimerDelegate& InDelegate,
        float InRate,
        bool bLoop,
        float InFirstDelay = -1.f,
        float CustomDilation = 1.f)
    {
        if (CustomDilation <= 0.f) CustomDilation = 1.f;

        double Now = GWorld ? GWorld->GetRealTimeSeconds() : 0.0;
        double Delay = (InFirstDelay >= 0.f ? InFirstDelay : InRate) / CustomDilation;

        TSharedPtr<FRealTimeTimer> Timer = MakeShared<FRealTimeTimer>();
        Timer->Delegate = InDelegate;
        Timer->Rate = InRate;
        Timer->bLoop = bLoop;
        Timer->CustomDilation = CustomDilation;
        Timer->TargetTime = Now + Delay;
        Timer->bActive = true;

        InOutHandle.Invalidate();
        ActiveTimers.Add(InOutHandle, Timer);
    }

    static void ClearTimer(FTimerHandle& Handle)
    {
        ActiveTimers.Remove(Handle);
    }

    static bool IsTimerActive(const FTimerHandle& Handle)
    {
        TSharedPtr<FRealTimeTimer> Timer = ActiveTimers.FindRef(Handle);
        return Timer.IsValid() && Timer->bActive;
    }
};

//Timer with Callback Realtimed && RealTimed with custom dilation
void UPSFl::SetRealTimeTimerWithCallback(UWorld* World, TFunction<void()> Callback, float Duration)
{
	if (!IsValid(World))
	{
		UE_LOG(LogTemp, Warning, TEXT("%S :: World invalid!"), __FUNCTION__);
		return;
	}

	const double TargetTime = World->GetRealTimeSeconds() + Duration;
	TWeakObjectPtr<UWorld> WeakWorld = World;

	// Fonction récursive
	TSharedRef<TFunction<void()>> Checker = MakeShared<TFunction<void()>>();
	*Checker = [WeakWorld, Callback = MoveTemp(Callback), TargetTime, Checker]() mutable
	{
		if (!WeakWorld.IsValid())
			return;

		UWorld* LocalWorld = WeakWorld.Get();
		if (LocalWorld->GetRealTimeSeconds() >= TargetTime)
		{
			Callback();
		}
		else
		{
			LocalWorld->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateLambda(*Checker)
			);
		}
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(*Checker));
}

void UPSFl::SetDilatedRealTimeTimerWithCallback(UWorld* World, TFunction<void()> Callback, float Duration, float CustomDilation)
{
	if (!IsValid(World))
	{
		UE_LOG(LogTemp, Warning, TEXT("%S :: World invalid!"), __FUNCTION__);
		return;
	}

	if (CustomDilation <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%S :: Invalid CustomDilation (%.2f). Using 1.0."), __FUNCTION__, CustomDilation);
		CustomDilation = 1.f;
	}

	const double TargetTime = World->GetRealTimeSeconds() + (Duration / CustomDilation);
	TWeakObjectPtr<UWorld> WeakWorld = World;

	TSharedRef<TFunction<void()>> Checker = MakeShared<TFunction<void()>>();
	*Checker = [WeakWorld, Callback = MoveTemp(Callback), TargetTime, Checker]() mutable
	{
		if (!WeakWorld.IsValid())
			return;

		UWorld* LocalWorld = WeakWorld.Get();
		if (LocalWorld->GetRealTimeSeconds() >= TargetTime)
		{
			Callback();
		}
		else
		{
			LocalWorld->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateLambda(*Checker)
			);
		}
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(*Checker));
}
//------------------
#pragma endregion Time

#pragma region Physic
//------------------

void UPSFl::AddImpulseDilated(UObject* WorldContextObject, UMeshComponent* Target, FVector Impulse, FName BoneName, bool bVelChange)
{
	if (!IsValid(WorldContextObject) || !IsValid(WorldContextObject->GetWorld()))
	{
		UE_LOG(LogTemp, Warning, TEXT("%S :: World invalid!"), __FUNCTION__);
		return;
	}

	if (!IsValid(Target) || !IsValid(Target->GetOwner()))
	{
		UE_LOG(LogTemp, Warning, TEXT("%S :: target OR target Owner invalid!"), __FUNCTION__);
		return;
	}
	
	float globalDilation =  UGameplayStatics::GetGlobalTimeDilation(WorldContextObject->GetWorld());
	float dilatedTime = Target->GetOwner()->CustomTimeDilation <= 0.0f ? globalDilation : (globalDilation / Target->GetOwner()->CustomTimeDilation);
	Target->AddImpulse((Impulse / dilatedTime), BoneName, bVelChange);
}

// void UPSFl::AddForceDilated(UObject* WorldContextObject, UMeshComponent* target, FVector Impulse, FName BoneName = NAME_None, bool bVelChange = false)
// {
// 	if (!IsValid(WorldContextObject) || !IsValid(WorldContextObject->GetWorld()))
// 	{
// 		UE_LOG(LogTemp, Warning, TEXT("%S :: World invalid!"), __FUNCTION__);
// 		return;
// 	}
//
// 	if (!IsValid(target) || !IsValid(target->GetOwner()))
// 	{
// 		UE_LOG(LogTemp, Warning, TEXT("%S :: target OR target Owner invalid!"), __FUNCTION__);
// 		return;
// 	}
// 	
// 	float dilatedTime = target->GetOwner()->CustomTimeDilation * UGameplayStatics::GetGlobalTimeDilation(GetWorld());
// 	target->AddForce(Impulse / dilatedTime), NAME_None, false);
// }

void UPSFl::GetActivePhysicsComponents(AActor* Actor, TArray<UPrimitiveComponent*>& OutComponents)
{
	OutComponents.Reset(); // évite realloc
	if (!Actor) return;

	// Récupération *directe* des composants primitifs
	const TInlineComponentArray<UPrimitiveComponent*> Components(Actor);

	for (UPrimitiveComponent* Comp : Components)
	{
		if (!Comp) continue;

		// Vérifier si la physique est simulée
		if (Comp->IsSimulatingPhysics())
		{
			// Lire la vélocité sans alloc (ZeroTolerance pour éviter flottants résiduels)
			const FVector Vel = Comp->GetPhysicsLinearVelocity();
			if (!Vel.IsNearlyZero(KINDA_SMALL_NUMBER))
			{
				OutComponents.Add(Comp);
			}
		}
	}
}


//------------------
#pragma endregion Physic

#pragma region Camera
//------------------

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

void UPSFl::ShakeCamera(const UWorld* const world, const TSubclassOf<class UCameraShakeBase>& shake, const float& scale)
{
	if(!IsValid(world) || !IsValid(shake)) return;
	
	APlayerController* playerController =  UGameplayStatics::GetPlayerController(world, 0);
	if(!IsValid(playerController)) return;
	
	playerController->ClientStartCameraShake(shake, scale);
}

//------------------
#pragma endregion Camera

#pragma region Detection
//------------------


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


bool UPSFl::FindClosestPointOnActor(const AActor* actorToTest, const FVector& fromWorldLocation, FVector& outClosestPoint)
{
	if (!IsValid(actorToTest)) return false;

	bool bFoundPoint = false;

	TArray<UActorComponent*> outComps;
	actorToTest->GetComponents(UMeshComponent::StaticClass(), outComps, true);
    
	for (UActorComponent* currentComp : outComps)
	{
		if (UMeshComponent* meshComp = Cast<UMeshComponent>(currentComp))
		{
			FVector currentPoint;
			const float currentDist = meshComp->GetClosestPointOnCollision(fromWorldLocation, currentPoint);

			if (currentDist >= 0 )
			{
				outClosestPoint = currentPoint;
				bFoundPoint = true;
			}
		}
	}
    
	return bFoundPoint;
}

	//------------------
#pragma endregion Detection

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




