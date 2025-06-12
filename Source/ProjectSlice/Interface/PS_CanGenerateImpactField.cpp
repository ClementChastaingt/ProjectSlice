// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_CanGenerateImpactField.h"
#include "Kismet/KismetMathLibrary.h"


// Add default functionality here for any IPS_CanGenerateImpactField functions that are not pure virtual.
void IPS_CanGenerateImpactField::GenerateImpactField(const FHitResult& targetHit)
{
	// if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(GetWorld()) || !_SightHitResult.bBlockingHit) return;
	//
	// //Spawn param
	// FActorSpawnParameters SpawnInfo;
	// SpawnInfo.Owner = _PlayerCharacter;
	// SpawnInfo.Instigator = _PlayerCharacter;
	// SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	//
	// FRotator rot = UKismetMathLibrary::FindLookAtRotation(_SightHitResult.ImpactPoint, _SightHitResult.ImpactPoint + _SightHitResult.ImpactNormal * -100);
	// rot.Pitch = rot.Pitch - 90.0f;
	// rot.Roll = TargetRackRotation.Roll;
	//_ImpactField = GetWorld()->SpawnActor<AFieldSystemActor>(FieldSystemActor.LoadSynchronous(), _SightHitResult.ImpactPoint + _SightHitResult.ImpactNormal * -100, rot, SpawnInfo);
	
	//Debug
	// if(bDebugChaos) UE_LOG(LogTemp, Log, TEXT("%S :: success %i"), __FUNCTION__, IsValid(_ImpactField));
	//
	// //Callback
	// OnImpulseChaosEvent.Broadcast(_ImpactField);
}
