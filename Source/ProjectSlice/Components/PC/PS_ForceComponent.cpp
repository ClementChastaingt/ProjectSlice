// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ForceComponent.h"

#include "Field/FieldSystemComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Character/PC/PS_Character.h"
#include "ProjectSlice/Data/PS_Constants.h"
#include "ProjectSlice/Data/PS_GlobalType.h"
#include "ProjectSlice/Data/PS_TraceChannels.h"
#include "ProjectSlice/FunctionLibrary/PSFl.h"


class UPS_SlicedComponent;
// Sets default values for this component's properties
UPS_ForceComponent::UPS_ForceComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UPS_ForceComponent::BeginPlay()
{
	Super::BeginPlay();

	_PlayerCharacter = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if(!IsValid(_PlayerCharacter)) return;
	
	_PlayerController = Cast<AProjectSlicePlayerController>(_PlayerCharacter->GetController());
	if(!IsValid(_PlayerController)) return;
	
	//Screw Attach 
	AttachScrew();

	//Callback
	if(IsValid(_PlayerCharacter->GetSlowmoComponent()))
	{
		_PlayerCharacter->GetProceduralAnimComponent()->OnScrewResetEnd.AddUniqueDynamic(this, &UPS_ForceComponent::OnScrewResetEndEventReceived);
	}
	
}


void UPS_ForceComponent::UpdatePushTargetLoc()
{
	if(!_bIsPushLoading) return;
	
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetWeaponComponent())) return;

	OnSpawnPushDistorsion.Broadcast(_CurrentPushHitResult.bBlockingHit && !bIsQuickPush);
	
	if(!_CurrentPushHitResult.bBlockingHit || bIsQuickPush) return;
	
	//Target Loc
	//_PushTargetLoc = _PlayerCharacter->GetWeaponComponent()->GetSightHitResult().Location;
	FVector dir = (_CurrentPushHitResult.Normal * - 1) + _PlayerCharacter->GetActorForwardVector();
	dir.Normalize();
	
	//FVector start = _CurrentPushHitResult.Location;
	FVector start = _PlayerCharacter->GetWeaponComponent()->GetSightTarget();
	
	const FVector pushTargetLoc = start + dir * -10.0f;

	//Target Rot
	const FRotator pushTargetRot = UKismetMathLibrary::FindLookAtRotation(start + _CurrentPushHitResult.Normal * -500, pushTargetLoc);

	//Debug
	//if(bDebugPush) DrawDebugLine(GetWorld(), start, start + dir * -100, FColor::Yellow, false, 2, 10, 3);
	OnPushTargetUpdate.Broadcast(pushTargetLoc,pushTargetRot);
	
}

// Called every frame
void UPS_ForceComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if(IsValid(_PlayerCharacter)) _CurrentPushHitResult = _PlayerCharacter->GetWeaponComponent()->GetSightHitResult();
	
	UpdatePushTargetLoc();

	MoveImpactField();
}


#pragma region Push
//------------------


void UPS_ForceComponent::UnloadPush()
{
	if(bDebugPush) UE_LOG(LogTemp, Log, TEXT("%S"),__FUNCTION__);
	
	_ReleasePushTimestamp = GetWorld()->GetAudioTimeSeconds();
	_bIsPushLoading = false;
	OnPushEvent.Broadcast(_bIsPushLoading);
}

void UPS_ForceComponent::ReleasePush()
{
	if(GetWorld()->GetTimerManager().IsTimerActive(_CoolDownTimerHandle)) return;
	
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetWeaponComponent()) || !IsValid(GetWorld()))
	{
		StopPush();
		return;
	}

	//Quick pushing check && if player target is valid, if don't do quickPush logic
	bIsQuickPush = GetWorld()->GetAudioTimeSeconds() - _StartForcePushTimestamp <= QuickPushTimeThreshold;
	if(!bIsQuickPush)
	{
		bIsQuickPush =
			!_CurrentPushHitResult.bBlockingHit
			|| !IsValid(_CurrentPushHitResult.GetActor())
			|| !IsValid(_CurrentPushHitResult.GetComponent())
			|| !_CurrentPushHitResult.GetComponent()->IsSimulatingPhysics();
	}
	if(bDebugPush) UE_LOG(LogTemp, Log, TEXT("%S :: bIsQuickPush %i"), __FUNCTION__, bIsQuickPush);
	
	//Setup force var
	const float alphaInput = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetAudioTimeSeconds(), _StartForcePushTimestamp,_StartForcePushTimestamp + MaxPushForceTime,0.5f, 1.0f);
	const float force = PushForce * alphaInput;
	float mass = UPSFl::GetObjectUnifiedMass(_CurrentPushHitResult.GetComponent());
	
	//Determine dir
	if(bIsQuickPush)
	{
		_DirForcePush = _PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector();
		_DirForcePush.Normalize();
		_StartForcePushLoc = _PlayerCharacter->GetMesh()->GetSocketLocation(SOCKET_HAND_LEFT) + _DirForcePush;
	}
	else
	{
		_DirForcePush = (_CurrentPushHitResult.Normal * - 1) + _PlayerCharacter->GetActorForwardVector();
		_DirForcePush.Normalize();
		_StartForcePushLoc = _CurrentPushHitResult.Location - _DirForcePush * (ConeLength/3);
	}
	
	//Cone raycast
	TArray<FHitResult> outHits;
	TArray<AActor*> actorsToIgnore;
	actorsToIgnore.AddUnique(_PlayerCharacter);
	
	UPSFl::SweepConeMultiByChannel(GetWorld(),_StartForcePushLoc, _DirForcePush,ConeAngleDegrees, ConeLength, StepInterval,outHits, ECC_GPE, actorsToIgnore, bDebugPush);

	// Remove duplicate components
	TSet<UPrimitiveComponent*> uniqueComp;
	TArray<FHitResult> filteredHitResult;
	for (FHitResult outHit : outHits)
	{
		if(!IsValid(outHit.GetComponent()) || !outHit.GetComponent()->IsSimulatingPhysics()) continue;
		
		if (outHit.GetComponent() && !uniqueComp.Contains(outHit.GetComponent()))
		{
			uniqueComp.Add(outHit.GetComponent());
			filteredHitResult.Add(outHit);
		}
	}

	// Sort by distance (nearest to farthest)
	Algo::Sort(filteredHitResult, [](const FHitResult& A, const FHitResult& B)
	{
		return A.Distance < B.Distance;
	});
	
	
	//Impulse
	for (FHitResult outHitResult : filteredHitResult)
	{
		//Check comp type
		UMeshComponent* outMeshComp = Cast<UMeshComponent>(outHitResult.GetComponent());
		UGeometryCollectionComponent* outGeometryComp = Cast<UGeometryCollectionComponent>(outHitResult.GetComponent());
		
		if(!IsValid(outMeshComp) && !IsValid(outGeometryComp)) continue;

		//Determine response latency 
		const float dist = UKismetMathLibrary::Vector_DistanceSquared(outHitResult.TraceStart, outMeshComp->GetComponentLocation());
		//const float dist = UKismetMathLibrary::Vector_Distance(_StartForcePushLoc, _StartForcePushLoc + _DirForcePush * ConeLength);
		const float duration = UKismetMathLibrary::SafeDivide(dist, ConeLength * ConeLength);
		
		//Chaos
		if(IsValid(outGeometryComp) && !IsValid(_ImpactField) && !_bCanMoveField)
		{
			const float radius = FMath::Sqrt(dist) * FMath::DegreesToRadians(ConeAngleDegrees);

			//Destruct with delay for match with VFX
			FTimerHandle timerChaosHandle;
			FTimerDelegate timerChaosDelegate;
			
			timerChaosDelegate.BindUFunction(this, FName("GenerateImpactField"), outHitResult, FVector::One() * radius); 
			GetWorld()->GetTimerManager().SetTimer(timerChaosHandle, timerChaosDelegate, duration, false);

			UE_LOG(LogTemp, Warning, TEXT("radius %f, duration %f"),radius, duration);
		}

		//Impulse
		if(IsValid(outMeshComp))
		{
			//Calculate mass for weight force 
			mass = UPSFl::GetObjectUnifiedMass(outMeshComp);

			//Impulse with delay for match with VFX
			FTimerHandle timerImpulseHandle;
			FTimerDelegate timerImpulseDelegate;
				
			timerImpulseDelegate.BindUObject(this, &UPS_ForceComponent::Impulse,outMeshComp, _StartForcePushLoc + _DirForcePush * (force * mass)); 
			GetWorld()->GetTimerManager().SetTimer(timerImpulseHandle, timerImpulseDelegate, duration, false);
			
			if(bDebugPush) UE_LOG(LogTemp, Log, TEXT("%S :: actor %s, compHit %s,  force %f, mass %f, pushForce %f, alphainput %f, duration %f"),__FUNCTION__,*outMeshComp->GetOwner()->GetActorNameOrLabel(), *outMeshComp->GetName(), force, mass, PushForce, alphaInput, duration);
		}
	}

	//---Feedbacks----
	//Push cone burst feedback
	_PushForceRotation = UKismetMathLibrary::FindLookAtRotation(_StartForcePushLoc, _StartForcePushLoc + _DirForcePush * ConeLength).Clamp();
	OnSpawnPushBurst.Broadcast(_StartForcePushLoc, _DirForcePush * ConeLength);
		
	//Play the sound if specified
	if(IsValid(PushSound))
		UGameplayStatics::SpawnSoundAttached(PushSound, _PlayerCharacter->GetMesh());

	//Stop push
	StopPush();

}

void UPS_ForceComponent::Impulse(UMeshComponent* inComp, const FVector impulse)
{
	//impulse
	inComp->AddImpulse(impulse, NAME_None, false);
}

void UPS_ForceComponent::SetupPush()
{
	if(!IsValid(GetWorld())) return;

	if(_bIsPushLoading || GetWorld()->GetTimerManager().IsTimerActive(_CoolDownTimerHandle)) return;

	if(bDebugPush) UE_LOG(LogTemp, Log, TEXT("%S"),__FUNCTION__);
	
	_StartForcePushTimestamp = GetWorld()->GetAudioTimeSeconds();

	_bIsPushing = true;
	_bIsPushLoading = true;
	_bIsPushReleased = false;
	OnPushEvent.Broadcast(_bIsPushing);
}

void UPS_ForceComponent::StopPush()
{
	if(bDebugPush) UE_LOG(LogTemp, Log, TEXT("%S"),__FUNCTION__);

	_bIsPushing = false;
	_bIsPushLoading = false;
	_bIsPushReleased = true;
	bIsQuickPush = false;

	//TODO:: Made cooldown proportional to force weight
	UPSFl::StartCooldown(GetWorld(),CoolDownDuration,_CoolDownTimerHandle);
}

//------------------
#pragma endregion Push

#pragma region Screw

//------------------

void UPS_ForceComponent::OnScrewResetEndEventReceived()
{
	//Release force when screw reseting finished
	ReleasePush();
}

void UPS_ForceComponent::AttachScrew()
{
	USkeletalMeshComponent* playerSkel = _PlayerCharacter->GetMesh();
	if(!IsValid(playerSkel))
	{
		UE_LOG(LogTemp, Warning, TEXT("%S :: Skeletal Mesh not found !"),__FUNCTION__);
		return;
	}

	UStaticMesh* LoadedScrewMesh =  ScrewMesh.LoadSynchronous();
	if(!IsValid(LoadedScrewMesh))
	{
		UE_LOG(LogTemp, Warning, TEXT("%S :: ScrewMesh not found or invalid !"),__FUNCTION__);
		return;
	}

	// Parkour list of socket names
	for (FName SocketName : ScrewSocketNames)
	{
		// Check if socket exists
		if (_PlayerCharacter->GetMesh()->DoesSocketExist(SocketName))
		{
			// Create a new StaticMeshComponent
			UStaticMeshComponent* NewMeshComponent = NewObject<UStaticMeshComponent>(this);
			NewMeshComponent->SetStaticMesh(Cast<UStaticMesh>(LoadedScrewMesh));
			NewMeshComponent->RegisterComponent();
			
			//Setup mesh transform
			NewMeshComponent->SetRelativeRotation(FRotator(90.0f,0.0f,0.0f));
			NewMeshComponent->SetRelativeScale3D(FVector(0.25f,0.25f,0.25f));
            
			// Attach the mesh to the socket			
			NewMeshComponent->AttachToComponent(playerSkel, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%S :: Socket %s does not exist!"),__FUNCTION__, *SocketName.ToString());
		}
	}

	
}

//------------------
#pragma endregion Screw

#pragma region Destruction
//------------------

#pragma region CanGenerateImpactField
//------------------

void UPS_ForceComponent::GenerateImpactField(const FHitResult& targetHit, const FVector extent)
{
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(GetWorld()) || !targetHit.bBlockingHit) return;

	if(!IsValid(FieldSystemActor.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("%S :: SpawnActor failed because no class was specified"),__FUNCTION__);
		return;
	}
	
	//Spawn param
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = _PlayerCharacter;
	SpawnInfo.Instigator = _PlayerCharacter;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	//Spawn Location
	FVector loc = targetHit.Location;
		
	//Spawn Rotation
	FRotator rot = UKismetMathLibrary::FindLookAtRotation(_StartForcePushLoc, _StartForcePushLoc + _DirForcePush * ConeLength);
	rot.Pitch = rot.Pitch - 90.0f;

	//Spawn scale
	FVector scale = (extent * 2) / 100;
		
	_ImpactField = GetWorld()->SpawnActor<AFieldSystemActor>(FieldSystemActor.Get(), loc, rot, SpawnInfo);
	_ImpactField->SetActorScale3D(scale);

	if(!IsValid(_ImpactField)) return;
	
	//Stock move field var
	_GenerateFieldTimestamp = GetWorld()->GetTimeSeconds();
	_FieldTransformOnGeneration = FTransform(rot, loc, scale);
	_bCanMoveField = true;
	
	//Debug
	if(bDebugChaos) UE_LOG(LogTemp, Log, TEXT("%S :: success %i"), __FUNCTION__, IsValid(_ImpactField));

	//Callback
	OnForceChaosFieldGeneratedEvent.Broadcast(_ImpactField);
}

void UPS_ForceComponent::MoveImpactField()
{
	if(!IsValid(_ImpactField) || !IsValid(GetWorld())) return;

	if(!_bCanMoveField) return;

	//Init work var
	const float moveDist = ConeLength + FieldSystemMoveTargetLocFwdOffset;
	const FVector targetLoc = _StartForcePushLoc + _DirForcePush * moveDist;

	const float distFromStart = UKismetMathLibrary::Vector_DistanceSquared(_FieldTransformOnGeneration.GetLocation(), targetLoc);
	const float distToTarget = UKismetMathLibrary::Vector_DistanceSquared(_ImpactField->GetActorLocation(),targetLoc);
	
	const float moveDuration = UKismetMathLibrary::SafeDivide(FMath::Sqrt(distFromStart), moveDist);

	//Override life span
	const float newLifeSpan = moveDuration + FieldSystemDestroyingDelay;
	if(_ImpactField->GetLifeSpan() != newLifeSpan) _ImpactField->SetLifeSpan(moveDuration + FieldSystemDestroyingDelay);
	
	//Move
	const float alpha = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetTimeSeconds(), _GenerateFieldTimestamp, _GenerateFieldTimestamp + moveDuration, 0.0f, 1.0f);
	_ImpactField->SetActorLocation(FMath::Lerp(_FieldTransformOnGeneration.GetLocation(),targetLoc,alpha));

	//Scale
	const float radius = FMath::Abs((FMath::Sqrt(distToTarget) - FMath::Sqrt(distFromStart)) * FMath::DegreesToRadians(ConeAngleDegrees));
	_ImpactField->SetActorScale3D(_FieldTransformOnGeneration.GetScale3D() + ((FVector::One() * radius * 2) / 100));

	if (bDebugChaos) UE_LOG(LogTemp, Log, TEXT("%S :: duration %f, lifespan %f,  distFromStart %f, distToTarget %f, radiusOffset %f, alpha %f"),__FUNCTION__, moveDuration, _ImpactField->GetLifeSpan(),  distFromStart, distToTarget, radius, alpha);

	//Stop moving
	if (alpha >= 1.0f)
	{
		_bCanMoveField = false;
	}
}

//------------------

#pragma endregion CanGenerateImpactField

//------------------
#pragma endregion Destruction
	
	
	

