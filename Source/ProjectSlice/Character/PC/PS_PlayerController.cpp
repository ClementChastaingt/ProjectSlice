// Copyright Epic Games, Inc. All Rights Reserved.


#include "PS_PlayerController.h"

#include "EnhancedInputComponent.h"
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
	}

	// Setup player ref
	if(!IsValid(GetWorld())) return;
	_CurrentPossessingPawn = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

	if(IsValid(_CurrentPossessingPawn))
	{
		_CameraComp = _CurrentPossessingPawn->GetFirstPersonCameraComponent();
		_SlowmoComp = _CurrentPossessingPawn->GetSlowmoComponent();
		_WeaponComp = _CurrentPossessingPawn->GetWeaponComponent();
		_HookComp = _CurrentPossessingPawn->GetHookComponent();
		_ForceComp = _CurrentPossessingPawn->GetForceComponent();
	}

}

#pragma region Keybord && Gamepad
//------------------

void AProjectSlicePlayerController::OnIAActionKeyboardTriggered(const FInputActionInstance& inputActionInstance)
{
	bIsUsingGamepad = false;
}

void AProjectSlicePlayerController::OnIAAxisKeyboardTriggered(const FInputActionInstance& inputActionInstance)
{
	if(!inputActionInstance.GetValue().IsNonZero()) return;
	bIsUsingGamepad = false;
}

void AProjectSlicePlayerController::OnIAActionGamepadTriggered(const FInputActionInstance& inputActionInstance)
{
	bIsUsingGamepad = true;
}

void AProjectSlicePlayerController::OnIAAxisGamepadTriggered(const FInputActionInstance& inputActionInstance)
{
	if(inputActionInstance.GetValue().IsNonZero()) return;
	bIsUsingGamepad = true;
}

//------------------
#pragma endregion Keyborad && Gamepad


void AProjectSlicePlayerController::SetupMovementInputComponent()
{
	if(!IsValid(_CurrentPossessingPawn)) return;
	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Gamepad/keyboard detection
		EnhancedInputComponent->BindAction(IA_ActionKeyboard, ETriggerEvent::Triggered, this, &AProjectSlicePlayerController::OnIAActionKeyboardTriggered);

		EnhancedInputComponent->BindAction(IA_AxisKeyboard, ETriggerEvent::Triggered, this, &AProjectSlicePlayerController::OnIAAxisKeyboardTriggered);

		EnhancedInputComponent->BindAction(IA_ActionGamepad, ETriggerEvent::Triggered, this, &AProjectSlicePlayerController::OnIAActionGamepadTriggered);

		EnhancedInputComponent->BindAction(IA_AxisGamepad, ETriggerEvent::Triggered, this, &AProjectSlicePlayerController::OnIAAxisGamepadTriggered);

		
		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, _CurrentPossessingPawn, &AProjectSliceCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, _CurrentPossessingPawn, &AProjectSliceCharacter::Move);
		
		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, _CurrentPossessingPawn, &AProjectSliceCharacter::Look);
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, _CurrentPossessingPawn, &AProjectSliceCharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, _CurrentPossessingPawn, &AProjectSliceCharacter::StopJumping);

		// Dash
		EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Started, _CurrentPossessingPawn,  &AProjectSliceCharacter::Dash);

		// Crouching
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, _CurrentPossessingPawn, &AProjectSliceCharacter::Crouching);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AProjectSlicePlayerController::SetupMiscComponent()
{
	if(!IsValid(_CurrentPossessingPawn)
		|| !IsValid(_SlowmoComp)) return;
	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{		
		// Slowmotion
		EnhancedInputComponent->BindAction(SlowmoAction, ETriggerEvent::Started, _SlowmoComp, &UPS_SlowmoComponent::OnTriggerSlowmo);

		// Glasses
		EnhancedInputComponent->BindAction(GlassesAction, ETriggerEvent::Started, _CurrentPossessingPawn, &AProjectSliceCharacter::Glasses);
		
		// Stow
		EnhancedInputComponent->BindAction(StowAction, ETriggerEvent::Started, _CurrentPossessingPawn,  &AProjectSliceCharacter::Stow);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AProjectSlicePlayerController::SetupWeaponInputComponent()
{
	if(!IsValid(_CurrentPossessingPawn)
		|| !IsValid(_WeaponComp)
		|| !IsValid(_HookComp)
		|| !IsValid(_ForceComp)) return;
	
	//BindAction
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Fire
		EnhancedInputComponent->BindAction(IA_Fire, ETriggerEvent::Triggered, _WeaponComp, &UPS_WeaponComponent::FireTriggered);

		// Rotate Rack
		EnhancedInputComponent->BindAction(IA_TurnRack, ETriggerEvent::Triggered, _WeaponComp, &UPS_WeaponComponent::TurnRack);

		//Hook Launch
		EnhancedInputComponent->BindAction(IA_Hook, ETriggerEvent::Triggered, _HookComp, &UPS_HookComponent::HookObject);
		
		//Winder Launch 
		EnhancedInputComponent->BindAction(IA_WinderPull, ETriggerEvent::Triggered, _HookComp, &UPS_HookComponent::WindeHook);
		EnhancedInputComponent->BindAction(IA_WinderPull, ETriggerEvent::Completed, _HookComp, &UPS_HookComponent::StopWindeHook);

		EnhancedInputComponent->BindAction(IA_WinderPush, ETriggerEvent::Triggered, _HookComp, &UPS_HookComponent::WindeHook);
		EnhancedInputComponent->BindAction(IA_WinderPush, ETriggerEvent::Completed, _HookComp, &UPS_HookComponent::WindeHook);
		
		//Push Launch
		EnhancedInputComponent->BindAction(IA_ForcePush, ETriggerEvent::Triggered, _ForceComp, &UPS_ForceComponent::StartPush);
		
	}
}




