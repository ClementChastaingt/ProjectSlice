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
#include "ProjectSlice/Components/PC/PS_ParkourComponent.h"

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
	
	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	//Setup Mesh
	//Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	GetMesh()->SetOnlyOwnerSee(true);
	GetMesh()->SetupAttachment(FirstPersonCameraComponent);
	GetMesh()->bCastDynamicShadow = false;
	GetMesh()->CastShadow = false;
	GetMesh()->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));
	
	
	//Create ParkourComponent
	ParkourComponent = CreateDefaultSubobject<UPS_ParkourComponent>(TEXT("ParkourComponent"));
	ParkourComponent->SetupAttachment(RootComponent);
	
	//Create WeaponComponent
	WeaponComponent = CreateDefaultSubobject<UPS_WeaponComponent>(TEXT("WeaponComponent"));
	WeaponComponent->SetupAttachment(GetMesh());
	WeaponComponent->SetRelativeLocation(FVector(30.f, 0.f, 150.f));

	//Create HookComponent
	HookComponent = CreateDefaultSubobject<UPS_HookComponent>(TEXT("HookComponent"));
	HookComponent->SetRelativeLocation(FVector(30.f, 0.f, 150.f));

	//Create WeaponComponent
	ComponentsManager = CreateDefaultSubobject<UPS_ComponentsManager>(TEXT("ComponentManager"));
	
	//Attach Weapon Componenet on begin play
	WeaponComponent->AttachWeapon(this);
}

void AProjectSliceCharacter::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);

	if(bDebugMovementTrail) DrawDebugPoint(GetWorld(), GetActorLocation(), 5.0f, FColor::Cyan, false,10.0f);
}

void AProjectSliceCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	// Add Input Mapping Context
	_PlayerController = Cast<AProjectSlicePlayerController>(GetController());
	if(IsValid(_PlayerController))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(_PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	//Init Weapon Componenet on begin play if attach
	if(GetHasRifle())
		WeaponComponent->InitWeapon(this);


	// Setup ArrowComponent
	if(IsValid(GetArrowComponent()))
		GetArrowComponent()->SetHiddenInGame(!bDebugArrow);
}


void AProjectSliceCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AProjectSliceCharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AProjectSliceCharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AProjectSliceCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AProjectSliceCharacter::Move);
		
		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AProjectSliceCharacter::Look);
		
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

#pragma region Move
//------------------

void AProjectSliceCharacter::OnMovementModeChanged(EMovementMode previousMovementMode, uint8 previousCustomMode)
{
	Super::OnMovementModeChanged(previousMovementMode, previousCustomMode);
	
}

void AProjectSliceCharacter::Jump()
{
	Super::Jump();

	//If in WallRunning 
	if(IsValid(GetParkourComponent()))
	{
		if(!GetParkourComponent()->GetIsWallRunning())
		{
			if(GetParkourComponent()->GetOverlapInfos().IsEmpty()) return;
				
			//Try WallRunning if already overlap wall 
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

void AProjectSliceCharacter::StopJumping()
{
	Super::StopJumping();
}

void AProjectSliceCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	if (IsValid(_PlayerController))
	{	
		_PlayerController->SetMoveInput(Value.Get<FVector2D>());

		// add movement 
		AddMovementInput(GetActorForwardVector(), _PlayerController->GetMoveInput().Y);
		AddMovementInput(GetActorRightVector(), _PlayerController->GetMoveInput().X);
	}
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

	if (IsValid(GetController()))
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

//------------------
#pragma endregion Look

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

