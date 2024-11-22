// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_CharacterBase.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"


// Sets default values
APS_CharacterBase::APS_CharacterBase()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void APS_CharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APS_CharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void APS_CharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

#pragma region Utilities
//------------------

FVector APS_CharacterBase::GetFootPlacementLoc() const
{
	FVector footPlacement = GetCapsuleComponent()->GetComponentLocation();
	footPlacement.Z = footPlacement.Z - GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

	return footPlacement;
}

bool APS_CharacterBase::IsInAir() const
{
	const bool bIsInAir = GetCharacterMovement()->IsFalling() || GetCharacterMovement()->IsFlying();
	return bIsInAir;
}


//------------------
#pragma endregion Utilities



