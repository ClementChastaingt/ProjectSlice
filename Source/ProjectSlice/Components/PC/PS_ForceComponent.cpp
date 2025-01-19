// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ForceComponent.h"

#include "KismetTraceUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Character/PC/PS_Character.h"
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
	
}


// Called every frame
void UPS_ForceComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}


#pragma region Push
//------------------


void UPS_ForceComponent::UpdatePushForce()
{
	
}

void UPS_ForceComponent::StartPush(const FInputActionInstance& inputActionInstance)
{
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetWeaponComponent()) || !IsValid(GetWorld())) return;
	
	_CurrentPushHitResult = _PlayerCharacter->GetWeaponComponent()->GetSightHitResult();
	
	if(!_CurrentPushHitResult.bBlockingHit || !IsValid(_CurrentPushHitResult.GetActor()) || !IsValid(_CurrentPushHitResult.GetComponent())) return;

	if(!_CurrentPushHitResult.GetComponent()->IsSimulatingPhysics()) return;
	
	//Setup work variables
	const float mass = UPSFl::GetObjectUnifiedMass(_CurrentPushHitResult.GetComponent());
	const float alphaInput = FMath::Clamp(inputActionInstance.GetValue().Get<float>(),0.0f,1.0f);
	const float force = PushForce * 1000/** alpha*/;

	//Calculate direction of impulse
	//Forward dir
	//FVector dirPlayer =_CurrentPushHitResult.Location - _CurrentPushHitResult.TraceStart;
	
	//Normal dir
	FVector dir = (_CurrentPushHitResult.Normal * - 1) + _PlayerCharacter->GetActorForwardVector();
	dir.Normalize();
	FVector start = _CurrentPushHitResult.Location - dir * (ConeLength/3);
	
	//Cone raycast
	TArray<UPrimitiveComponent*> outHits;

	TArray<AActor*> ignoredActors;
	ignoredActors.AddUnique(_PlayerCharacter);

	static const FName SphereTraceMultiName(TEXT("SweepTraceCone"));
	FCollisionQueryParams QueryParams = ConfigureCollisionParams(SphereTraceMultiName, false, ignoredActors, true, GetWorld());

	UPSFl::SweepConeMultiByChannel(GetWorld(),start, dir,ConeAngleDegrees, ConeLength, StepInterval,SphereRadius, outHits, ECC_GPE, QueryParams);	

	//Impulse 
	for (UPrimitiveComponent* compHit : outHits)
	{
		UE_LOG(LogTemp, Error, TEXT("%S :: actor %s, "),__FUNCTION__,*compHit->GetName());
		compHit->AddImpulse(_CurrentPushHitResult.Location + dir * force, NAME_None, false);
	}
	
	if(bDebugPush) UE_LOG(LogTemp, Log, TEXT("%S :: force %f,mass %f, alphainput %f"),__FUNCTION__, force, mass, alphaInput);
	
	//_CurrentPushHitResult.GetComponent()->AddRadialImpulse(fwdDir, PushRadius, PushForce, RIF_Linear, true);
	
	// Try and play the sound if specified
	if(IsValid(PushSound))
		UGameplayStatics::SpawnSoundAttached(PushSound, _PlayerCharacter->GetMesh());

	//Callback
	OnPushEvent.Broadcast(true);
	
}

//------------------
#pragma endregion Push
	

