// Copyright Epic Games, Inc. All Rights Reserved.


#include "PS_PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "PS_Character.h"
#include "Kismet/GameplayStatics.h"

void AProjectSlicePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// get the enhanced input subsystem
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		// add the mapping context so we get controls
		Subsystem->AddMappingContext(InputMappingContext, 0);

		UE_LOG(LogTemp, Warning, TEXT("BeginPlay"));
	}

	// Setup player ref
	if(!IsValid(GetWorld())) return;
	CurrentPossessingPawn = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
}


FVector AProjectSlicePlayerController::GetWorldInputDirection() const
{
	FVector worldInputDirection = CurrentPossessingPawn->GetFirstPersonCameraComponent()->GetRightVector() * GetMoveInput().X + CurrentPossessingPawn->GetFirstPersonCameraComponent()->GetForwardVector() * GetMoveInput().Y;
	worldInputDirection.Z = 0;
	worldInputDirection.Normalize();
	return worldInputDirection;
}
