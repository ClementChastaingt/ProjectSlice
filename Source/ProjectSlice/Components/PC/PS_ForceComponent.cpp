// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ForceComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Character/PC/PS_Character.h"


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


void UPS_ForceComponent::StartPush()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetWeaponComponent())) return;

	_CurrentPushHitResult = _PlayerCharacter->GetWeaponComponent()->GetSightHitResult();
	
	if(!_CurrentPushHitResult.bBlockingHit || !IsValid(_CurrentPushHitResult.GetActor()) || !IsValid(_CurrentPushHitResult.GetComponent())) return;

	if(bDebugPush)UE_LOG(LogTemp, Log, TEXT("%S"), __FUNCTION__);

	DrawDebugLine(GetWorld(), _CurrentPushHitResult.TraceStart, _CurrentPushHitResult.TraceEnd, FColor::Yellow, false, 2, 10, 3);

	FVector fwdDir = _CurrentPushHitResult.TraceEnd - _CurrentPushHitResult.TraceStart;
	fwdDir.Normalize();

	const float force = PushForce * _CurrentPushHitResult.GetComponent()->GetMass() * _CurrentPushHitResult.GetComponent()->GetMassScale();
	_CurrentPushHitResult.GetComponent()->AddImpulse(fwdDir * force, NAME_None, false);
	//_CurrentPushHitResult.GetComponent()->AddRadialImpulse(fwdDir, PushRadius, PushForce, RIF_Linear, true);

	// Try and play the sound if specified
	if(IsValid(PushSound))
		UGameplayStatics::SpawnSoundAttached(PushSound, _PlayerCharacter->GetMesh());

	
}

//------------------
#pragma endregion Push
	

