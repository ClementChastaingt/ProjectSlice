// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_CharacterMovementComponent.h"

#include "AsyncTreeDifferences.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharacterMovement, Log, All);

// Sets default values for this component's properties
UPS_CharacterMovementComponent::UPS_CharacterMovementComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UPS_CharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void UPS_CharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	Super::PhysCustom(deltaTime, Iterations);

	switch(CustomMovementMode)
	{
		case ECustomMovementMode::CMOVE_None:
			break;
		
		case ECustomMovementMode::CMOVE_Slide:
			 PhysWalking(deltaTime, Iterations);
			 break;

		case ECustomMovementMode::CMOVE_WallRun:
			PhysWalking(deltaTime, Iterations);
			break;
		
		default:
			UE_LOG(LogCharacterMovement, Warning, TEXT("%s has unsupported movement mode %d"), *CharacterOwner->GetName(), int32(MovementMode));
			SetMovementMode(MOVE_None);
			break;
	}

	
}


// Called every frame
void UPS_CharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

