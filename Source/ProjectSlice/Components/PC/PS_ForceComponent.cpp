// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ForceComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "ProjectSlice/Character/PC/PS_Character.h"
#include "ProjectSlice/Components/GPE/PS_SlicedComponent.h"
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
	
}


void UPS_ForceComponent::UpdatePushTargetLoc()
{
	if(!_bIsPushing) return;
	
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetWeaponComponent())) return;

	OnSpawnPushDistorsion.Broadcast(_CurrentPushHitResult.bBlockingHit);
	if(!_CurrentPushHitResult.bBlockingHit) return;

	//Target Loc
	//_PushTargetLoc = _PlayerCharacter->GetWeaponComponent()->GetSightHitResult().Location;
	FVector dir = (_CurrentPushHitResult.Normal * - 1) + _PlayerCharacter->GetActorForwardVector();
	dir.Normalize();
	FVector start = _CurrentPushHitResult.Location;
	
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

	_CurrentPushHitResult = _PlayerCharacter->GetWeaponComponent()->GetSightHitResult();
	
	UpdatePushTargetLoc();
}


#pragma region Push
//------------------


void UPS_ForceComponent::ReleasePush()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetWeaponComponent()) || !IsValid(GetWorld()))
	{
		StopPush();
		return;
	}
	
	if(!_CurrentPushHitResult.bBlockingHit || !IsValid(_CurrentPushHitResult.GetActor()) || !IsValid(_CurrentPushHitResult.GetComponent()))
	{
		StopPush();
		return;
	}

	if(!_CurrentPushHitResult.GetComponent()->IsSimulatingPhysics())
	{
		StopPush();
		return;
	}
	
	//Setup work variables
	UPhysicalMaterial* physMat = _CurrentPushHitResult.GetComponent()->BodyInstance.GetSimplePhysicalMaterial();
	const float density = IsValid(physMat) ? physMat->Density : 1.0f;
		
	const float mass = UPSFl::GetObjectUnifiedMass(_CurrentPushHitResult.GetComponent());
	const float alphaInput = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetAudioTimeSeconds(), _StartForcePushTimestamp,_StartForcePushTimestamp + MaxPushForceTime,0.5f, 1.0f);
	const float force = PushForce * mass * alphaInput;
	
	//Determine dir
	FVector dir = (_CurrentPushHitResult.Normal * - 1) + _PlayerCharacter->GetActorForwardVector();
	dir.Normalize();
	FVector start = _CurrentPushHitResult.Location - dir * (ConeLength/3);
	
	//Cone raycast
	TArray<UPrimitiveComponent*> outHits;
	TArray<AActor*> actorsToIgnore;
	actorsToIgnore.AddUnique(_PlayerCharacter);
	
	UPSFl::SweepConeMultiByChannel(GetWorld(),start, dir,ConeAngleDegrees, ConeLength, StepInterval,outHits, ECC_GPE, actorsToIgnore, bDebugPush);	

	//Impulse
	for (UPrimitiveComponent* compHit : outHits)
	{
		if(!IsValid(Cast<UProceduralMeshComponent>(compHit))) continue;
		if(bDebugPush) UE_LOG(LogTemp, Error, TEXT("%S :: actor %s, force %f,mass %f, pushForce %f, alphainput %f"),__FUNCTION__,*compHit->GetOwner()->GetActorNameOrLabel(), force, mass, PushForce, alphaInput);
		compHit->AddImpulse(start + dir * force, NAME_None, false);
	}
	
	// Try and play the sound if specified
	if(IsValid(PushSound))
		UGameplayStatics::SpawnSoundAttached(PushSound, _PlayerCharacter->GetMesh());

	//Stop
	StopPush();
}

void UPS_ForceComponent::SetupPush()
{
	if(!IsValid(GetWorld())) return;
	
	_StartForcePushTimestamp = GetWorld()->GetAudioTimeSeconds();
	
	_bIsPushing = true;
	OnPushEvent.Broadcast(_bIsPushing);
}

void UPS_ForceComponent::StopPush()
{
	_ReleasePushTimestamp = GetWorld()->GetAudioTimeSeconds();
	_bIsPushing = false;
	OnPushEvent.Broadcast(_bIsPushing);
}

//------------------
#pragma endregion Push

#pragma region Screw
//------------------
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
	
	

