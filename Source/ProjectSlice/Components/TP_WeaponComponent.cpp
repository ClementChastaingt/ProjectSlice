// Copyright Epic Games, Inc. All Rights Reserved.


#include "TP_WeaponComponent.h"
#include "..\PC\PS_Character.h"
#include "..\GPE\PS_Projectile.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

// Sets default values for this component's properties
UTP_WeaponComponent::UTP_WeaponComponent()
{
	// Default offset from the _PlayerCharacter location for projectiles to spawn
	MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);

	//Create Component
	SightComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SightMesh"));
	SightComponent->SetupAttachment(this);
	
}

void UTP_WeaponComponent::BeginPlay()
{
	//....
} 


void UTP_WeaponComponent::Fire()
{
	if (_PlayerCharacter == nullptr || _PlayerCharacter->GetController() == nullptr)
	{
		return;
	}

	// Try and fire a projectile
	if (ProjectileClass != nullptr)
	{
		UWorld* const World = GetWorld();
		if (World != nullptr)
		{
			const APlayerController* PlayerController = Cast<APlayerController>(_PlayerCharacter->GetController());
			
			 FRotator SpawnRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
			// MuzzleOffset is in camera space, so transform it to world space before offsetting from the _PlayerCharacter location to find the final muzzle position
			const FVector SpawnLocation = GetOwner()->GetActorLocation() + SpawnRotation.RotateVector(MuzzleOffset);
			
			//UKismetSystemLibrary::LineTraceSingle(GetWorld(),SpawnLocation , ,UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel11), true,ActorsToIgnore, EDrawDebugTrace::None, hitResult, true);
			

			
			// Set Spawn Collision Handling Override
			 FActorSpawnParameters ActorSpawnParams;
			 ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
			
			// Spawn the projectile at the muzzle
			World->SpawnActor<AProjectSliceProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);
		}
	}
	
	// Try and play the sound if specified
	if (FireSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, _PlayerCharacter->GetActorLocation());
	}
	
	// Try and play a firing animation if specified
	if (FireAnimation != nullptr)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = _PlayerCharacter->GetMesh1P()->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}
}

void UTP_WeaponComponent::AttachWeapon(AProjectSliceCharacter* Target_PlayerCharacter)
{
	_PlayerCharacter = Target_PlayerCharacter;
	_PlayerController = Cast<APlayerController>(_PlayerCharacter->GetController());

	// Check that the _PlayerCharacter is valid, and has no rifle yet
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || _PlayerCharacter->GetHasRifle())
	{
		return;
	}

	// Attach the weapon to the First Person _PlayerCharacter
	const FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(_PlayerCharacter->GetMesh1P(), AttachmentRules, FName(TEXT("GripPoint")));
	
	// switch bHasRifle so the animation blueprint can switch to another animation set
	_PlayerCharacter->SetHasRifle(true);
	

	//Setup Sight Mesh
	//Place Sight Component to Projectile spawn place
	const FRotator SpawnRotation = _PlayerController->PlayerCameraManager->GetCameraRotation();
	const FVector SpawnLocation = GetOwner()->GetActorLocation() + SpawnRotation.RotateVector(MuzzleOffset);

	if(IsValid(SightComponent) && IsValid(SightMeshClass.GetDefaultObject()))
	{
		SightComponent->SetStaticMesh(SightMeshClass.GetDefaultObject());
		SightComponent->SetWorldTransform(FTransform(SpawnRotation,SpawnLocation,SightMeshScale), false);
	}


	// Set up action bindings
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
		_PlayerController->GetLocalPlayer()))
	{
		// Set the priority of the mapping to 1, so that it overrides the Jump action with the Fire action when using touch input
		Subsystem->AddMappingContext(FireMappingContext, 1);
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(
		_PlayerController->InputComponent))
	{
		// Fire
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &UTP_WeaponComponent::Fire);
	}
	

		
}

void UTP_WeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (_PlayerCharacter == nullptr)
	{
		return;
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(_PlayerCharacter->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(FireMappingContext);
		}
	}
}