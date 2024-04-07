// Copyright Epic Games, Inc. All Rights Reserved.


#include "PS_WeaponComponent.h"
#include "..\PC\PS_Character.h"
#include "..\GPE\PS_Projectile.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "KismetProceduralMeshLibrary.h"
#include "PS_SlicedComponent.h"

// Sets default values for this component's properties
UPS_WeaponComponent::UPS_WeaponComponent()
{
	// Default offset from the _PlayerCharacter location for projectiles to spawn
	MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);

	//Create Component
	SightComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SightMesh"));
	SightComponent->SetupAttachment(this);
	
}

void UPS_WeaponComponent::BeginPlay()
{
	//....
} 


void UPS_WeaponComponent::Fire()
{
	if (_PlayerCharacter == nullptr || _PlayerCharacter->GetController() == nullptr)
	{
		return;
	}

	// Try and fire a projectile
	UWorld* const World = GetWorld();
	if (World != nullptr)
	{
		const APlayerController* PlayerController = Cast<APlayerController>(_PlayerCharacter->GetController());

		//Trace Loc && Rot
		const FRotator SpawnRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
		// MuzzleOffset is in camera space, so transform it to world space before offsetting from the _PlayerCharacter location to find the final muzzle position
		const FVector SpawnLocation = GetOwner()->GetActorLocation() + SpawnRotation.RotateVector(MuzzleOffset);

		//Trace config
		const TArray<AActor*> ActorsToIgnore{_PlayerCharacter};
		
		//TODO :: Config Collision channel
		UKismetSystemLibrary::LineTraceSingle(GetWorld(), SpawnLocation,
		                                      SpawnLocation + SpawnRotation.GetComponentForAxis(EAxis::X) * 1000,
		                                      UEngineTypes::ConvertToTraceType(ECC_Visibility), true, ActorsToIgnore,
		                                      EDrawDebugTrace::ForDuration, CurrentFireHitResult, true);

		//Cut ProceduralMesh
		if (CurrentFireHitResult.bBlockingHit)
		{
			CurrentSlicedComponent = Cast<UPS_SlicedComponent>(CurrentFireHitResult.GetComponent());

			if (!IsValid(CurrentSlicedComponent)) return;

			//TODO :: Have to stock it on the parent SlicedComponent ?
			UProceduralMeshComponent* outHalfComponent;
			UMaterialInterface* HalfSectionMaterial = CurrentSlicedComponent->GetParentMesh()->GetMaterial(0);
			
			UKismetProceduralMeshLibrary::SliceProceduralMesh(CurrentSlicedComponent, CurrentFireHitResult.Location,
			                                                  SightComponent->GetUpVector(), true,
			                                                  outHalfComponent,
			                                                  EProcMeshSliceCapOption::CreateNewSectionForCap,
			                                                  HalfSectionMaterial);
		}

		//Add Physic and Impulse
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

void UPS_WeaponComponent::AttachWeapon(AProjectSliceCharacter* Target_PlayerCharacter)
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
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &UPS_WeaponComponent::Fire);
	}
	

		
}

void UPS_WeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (_PlayerCharacter == nullptr)
	{
		return;
	}

	if (const APlayerController* PlayerController = Cast<APlayerController>(_PlayerCharacter->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(FireMappingContext);
		}
	}
}