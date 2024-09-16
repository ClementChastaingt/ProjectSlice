// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS_Character.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "PS_PlayerController.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Components/PC/PS_ParkourComponent.h"
#include "ProjectSlice/Components/PC/PS_PlayerCameraComponent.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

AProjectSliceCharacter::AProjectSliceCharacter()
{
	//If inherited components are invalids
	if(!IsValid(GetMesh()) || !IsValid(GetCapsuleComponent()) || !IsValid(GetArrowComponent()))
	{
		UE_LOG(LogTemp, Error, TEXT("Inherited Mesh Or Arrow Invalid"));
		return;
	}
	
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// Create a RootCompônents
	FirstPersonRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FP_Root"));
	FirstPersonRoot->SetupAttachment(GetCapsuleComponent());

	MeshRoot = CreateDefaultSubobject<USpringArmComponent>(TEXT("Mesh_Root"));
	MeshRoot->SetupAttachment(FirstPersonRoot);
	MeshRoot->TargetArmLength = 0.0f;
	MeshRoot->bUsePawnControlRotation = true;
	MeshRoot->bDoCollisionTest = false;
	MeshRoot->bInheritPitch = true;
	MeshRoot->bInheritYaw = true;
	MeshRoot->bInheritRoll = false;

	CamRoot = CreateDefaultSubobject<USpringArmComponent>(TEXT("Cam_Root"));
	CamRoot->SetupAttachment(FirstPersonRoot);
	CamRoot->TargetArmLength = 0.0f;
	CamRoot->bUsePawnControlRotation = true;
	CamRoot->bDoCollisionTest = false;
	CamRoot->bInheritPitch = true;
	CamRoot->bInheritYaw = true;
	CamRoot->bInheritRoll = false;
	
	// Create a CameraComponent
	CameraSkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Cam_Skel"));
	CameraSkeletalMeshComponent->SetupAttachment(CamRoot);
	
	FirstPersonCameraComponent = CreateDefaultSubobject<UPS_PlayerCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(CameraSkeletalMeshComponent, FName("Camera"));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	//Setup Mesh
	//Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	GetMesh()->SetOnlyOwnerSee(true);
	GetMesh()->SetupAttachment(MeshRoot);
	GetMesh()->bCastDynamicShadow = false;
	GetMesh()->CastShadow = false;

	//Create WeaponComponent
	ComponentsManager = CreateDefaultSubobject<UPS_ComponentsManager>(TEXT("ComponentManager"));

	//Create WeaponComponent
	ProceduralAnimComponent = CreateDefaultSubobject<UPS_ProceduralAnimComponent>(TEXT("ProceduralAnimComponent"));

	//Create SlowmoComponent
	SlowmoComponent = CreateDefaultSubobject<UPS_SlowmoComponent>(TEXT("SlowmoComponent"));
	
	//Create ParkourComponent
	ParkourComponent = CreateDefaultSubobject<UPS_ParkourComponent>(TEXT("ParkourComponent"));
	ParkourComponent->SetupAttachment(RootComponent);
	
	//Create WeaponComponent
	WeaponComponent = CreateDefaultSubobject<UPS_WeaponComponent>(TEXT("WeaponComponent"));
	WeaponComponent->SetupAttachment(GetMesh());

	//Create HookComponent
	HookComponent = CreateDefaultSubobject<UPS_HookComponent>(TEXT("HookComponent"));
	
	//Attach Weapon Componenet on begin play
	WeaponComponent->AttachWeapon(this);

	//Config CharacterMovement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...
	
	
}

void AProjectSliceCharacter::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);

	if(!IsValid(_PlayerController)) return;

	if(GEngine && bDebugVelocity) GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan,FString::Printf(TEXT("Player Velocity: %f"), GetVelocity().Length()));
	
	if(bDebugMovementTrail) DrawDebugPoint(GetWorld(), GetActorLocation(), 5.0f, FColor::Cyan, false,10.0f);
}

void AProjectSliceCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	// Add Input Mapping Context
	if(IsValid(_PlayerController))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(_PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(_PlayerController->GetDefaultMappingContext(), 0);
		}
	}

	//Init Weapon Componenet on begin play if attach
	if(GetHasRifle())
		WeaponComponent->InitWeapon(this);


	// Setup ArrowComponent
	if(IsValid(GetArrowComponent()))
		GetArrowComponent()->SetHiddenInGame(!bDebugArrow);

	//Setup default speed 
	DefaultMaxWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
	DefaultMinAnalogSpeed = GetCharacterMovement()->MinAnalogWalkSpeed;
	
}

void AProjectSliceCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	_PlayerController = Cast<AProjectSlicePlayerController>(GetController());
	if(!IsValid(_PlayerController))
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("SetupPlayerInputComponent failde PlayerController is invalid"));
		return;
	}
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(_PlayerController->GetJumpAction(), ETriggerEvent::Started, this, &AProjectSliceCharacter::Jump);
		EnhancedInputComponent->BindAction(_PlayerController->GetJumpAction(), ETriggerEvent::Completed, this, &AProjectSliceCharacter::StopJumping);

		// Crouching
		EnhancedInputComponent->BindAction(_PlayerController->GetCrouchAction(), ETriggerEvent::Started, this, &AProjectSliceCharacter::Crouching);

		// Moving
		EnhancedInputComponent->BindAction(_PlayerController->GetMoveAction(), ETriggerEvent::Triggered, this, &AProjectSliceCharacter::Move);
		EnhancedInputComponent->BindAction(_PlayerController->GetMoveAction(), ETriggerEvent::Completed, this, &AProjectSliceCharacter::Move);
		
		// Looking
		EnhancedInputComponent->BindAction(_PlayerController->GetLookAction(), ETriggerEvent::Triggered, this, &AProjectSliceCharacter::Look);

		// Slowmotion
		EnhancedInputComponent->BindAction(_PlayerController->GetSlowmoAction(), ETriggerEvent::Started, this, &AProjectSliceCharacter::Slowmo);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

#pragma region Move
//------------------

#pragma region CharacterMovementComponent
//------------------

void AProjectSliceCharacter::OnMovementModeChanged(EMovementMode previousMovementMode, uint8 previousCustomMode)
{
	Super::OnMovementModeChanged(previousMovementMode, previousCustomMode);

	CoyoteTimeStart();	
}

void AProjectSliceCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	
	//Dip on Landing
	 if(IsValid(GetProceduralAnimComponent()))
	 	GetProceduralAnimComponent()->LandingDip();

	//Clear coyote time
	GetWorld()->GetTimerManager().ClearTimer(CoyoteTimerHandle);
}

//------------------
#pragma endregion CharacterMovementComponent

#pragma region Crouch
//------------------

void AProjectSliceCharacter::Crouching()
{
	if(!IsValid(GetParkourComponent()) || !IsValid(GetWorld())) return;

	if(GetParkourComponent()->GetIsWallRunning()
		|| GetParkourComponent()->IsLedging()
		||  GetParkourComponent()->GetMantlePhase() != EMantlePhase::NONE)
		return;

	bIsCrouchInputTrigger = !bIsCrouchInputTrigger;
	GetParkourComponent()->OnCrouch();
}

void AProjectSliceCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
}

void AProjectSliceCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
}

//------------------
#pragma endregion Crouch

#pragma region Jump
//------------------

bool AProjectSliceCharacter::CanJumpInternal_Implementation() const
{
	if(!CoyoteTimerHandle.IsValid())
	{
		//If crouch && try jump force jump (Uncrouch automaticaly)
		if(bIsCrouched && IsValid(GetParkourComponent()))
			return true;
		
		return Super::CanJumpInternal_Implementation();
	}
	
	return (Super::CanJumpInternal_Implementation() || GetWorld()->GetTimerManager().GetTimerRemaining(CoyoteTimerHandle) > 0) && !GetWorld()->GetTimerManager().IsTimerActive(CoyoteTimerHandle);
	
}


void AProjectSliceCharacter::Jump()
{	
	OnJumpLocation = GetActorLocation();

	if(bIsCrouched)
		bIsCrouchInputTrigger = false;
	
	Super::Jump();
	
	//If in WallRunning 
	if(IsValid(GetParkourComponent()))
	{
		if(!GetParkourComponent()->GetIsWallRunning())
		{
			if(GetParkourComponent()->GetOverlapInfos().IsEmpty()) return;
			
			//Try Parkour if already overlap wall 
			for (const FOverlapInfo& currentOverlapInfo : GetParkourComponent()->GetOverlapInfos())
			{
				if (!IsValid(currentOverlapInfo.OverlapInfo.Component.Get()) || !IsValid(currentOverlapInfo.OverlapInfo.Component.Get()->GetOwner()) ||
					currentOverlapInfo.OverlapInfo.Component.Get()->GetOwner() == this) continue;
				GetParkourComponent()->SetForceWallRun(true);
				GetParkourComponent()->OnComponentBeginOverlap.Broadcast(GetParkourComponent(), currentOverlapInfo.OverlapInfo.Component.Get()->GetOwner(), currentOverlapInfo.OverlapInfo.Component.Get(), currentOverlapInfo.GetBodyIndex(), currentOverlapInfo.bFromSweep, currentOverlapInfo.OverlapInfo);
				break;
			}
		}
		else
			GetParkourComponent()->JumpOffWallRun();
	}

}

void AProjectSliceCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	//Clear Coyote time
	GetWorld()->GetTimerManager().ClearTimer(CoyoteTimerHandle);

	//Dip
	if(IsValid(GetProceduralAnimComponent()))
		GetProceduralAnimComponent()->StartDip(5.0f, 1.0f);
}

void AProjectSliceCharacter::StopJumping()
{
	Super::StopJumping();
}

#pragma region Coyote
//------------------


void AProjectSliceCharacter::CoyoteTimeStart()
{
	//Coyote Time
	if(GetCharacterMovement()->MovementMode == MOVE_Falling)
	{
		const float alpha = FMath::Clamp(UKismetMathLibrary::NormalizeToRange(GetVelocity().Length(),0,GetCharacterMovement()->GetMaxSpeed()),0,1);
		const float coyoteDuration = FMath::Lerp(0.25,1,alpha) * CoyoteTime;
		
		FTimerDelegate coyote_TimerDelegate;
		coyote_TimerDelegate.BindUObject(this, &AProjectSliceCharacter::CoyoteTimeStop);
		GetWorld()->GetTimerManager().SetTimer(CoyoteTimerHandle, coyote_TimerDelegate, coyoteDuration, false);
	}
}

void AProjectSliceCharacter::CoyoteTimeStop()
{
	
}

//------------------
#pragma endregion Coyote


//------------------
#pragma endregion Jump

void AProjectSliceCharacter::Move(const FInputActionValue& Value)
{
	//0.2 is the Deadzone min threshold for Gamepad
	if (IsValid(_PlayerController) && _PlayerController->CanMove() && Value.Get<FVector2D>().Size() > 0.2)
	{
		const double inputWeight = UKismetMathLibrary::MapRangeClamped(GetVelocity().Length(), 0, GetCharacterMovement()->GetMaxSpeed(),InputMaxSmoothingWeight, InputMinSmoothingWeight);
		const float moveX = FMath::WeightedMovingAverage(Value.Get<FVector2D>().X, _PlayerController->GetMoveInput().X, inputWeight);
		const float moveY = FMath::WeightedMovingAverage(Value.Get<FVector2D>().Y, _PlayerController->GetMoveInput().Y, inputWeight);

		_PlayerController->SetMoveInput(FVector2D(moveX, moveY));
		
		//Add movement
		AddMovementInput(GetActorForwardVector(), _PlayerController->GetMoveInput().Y);
		AddMovementInput(GetActorRightVector(), _PlayerController->GetMoveInput().X);

		UE_LOG(LogTemp, Error, TEXT("%S :: %s"), __FUNCTION__, *_PlayerController->GetMoveInput().ToString());
	}
	else
		_PlayerController->SetMoveInput(FVector2D::ZeroVector);


	
}

void AProjectSliceCharacter::StopMoving()
{
	if(IsValid(_PlayerController))
	
		_PlayerController->SetMoveInput(FVector2D::ZeroVector);
}

//------------------
#pragma endregion Move

#pragma region Look
//------------------

void AProjectSliceCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (IsValid(_PlayerController) && _PlayerController->CanLook())
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

//------------------
#pragma endregion Look

#pragma region Slowmo
//------------------

void AProjectSliceCharacter::SetPlayerTimeDilation(const float newDilation)
{
	CustomTimeDilation = newDilation;
}

void AProjectSliceCharacter::Slowmo()
{
	if(IsValid(GetSlowmoComponent()))
	{
		GetSlowmoComponent()->OnTriggerSlowmo();
	}
}

//------------------
#pragma endregion Slowmo


#pragma region Weapon
//------------------


void AProjectSliceCharacter::SetHasRifle(bool bNewHasRifle)
{
	bHasRifle = bNewHasRifle;
}

bool AProjectSliceCharacter::GetHasRifle()
{
	return bHasRifle;
}

//------------------

#pragma endregion Weapon

