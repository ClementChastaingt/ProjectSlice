// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS_Character.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "PS_PlayerController.h"
#include "Components/ArrowComponent.h"
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

	// Create a RootComponents	
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
	HookComponent->SetupAttachment(CameraSkeletalMeshComponent, FName("HookAttach"));

	//Create ForceComponent
	ForceComponent = CreateDefaultSubobject<UPS_ForceComponent>(TEXT("ForceComponent"));

	//Create PhysicAnimComponent
	PhysicAnimComponent = CreateDefaultSubobject<UPhysicalAnimationComponent>(TEXT("PhysicAnimComponent"));
	
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
	
	if(bDebugMovementTrail) DrawDebugPoint(GetWorld(), GetActorLocation(), 5.0f, FColor::Cyan, false, MovementTrailDuration);

	//-- Capsule vel --
	SetCapsuleVelocity((GetCapsuleComponent()->GetComponentLocation() - GetPredCapsuleLocation()) / DeltaTime);
	SetPredCapsuleLocation(GetCapsuleComponent()->GetComponentLocation());
}

void AProjectSliceCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();
	
	_PlayerController = Cast<AProjectSlicePlayerController>(GetController());

	//Setup input
	// Add Input Mapping Context
	if(IsValid(_PlayerController))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(_PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(_PlayerController->GetDefaultMappingContext(), 0);
		}
	}
	_PlayerController->SetupMovementInputComponent();
	_PlayerController->SetupMiscComponent();

	//Init Weapon Componenet on begin play if attach
	if(GetHasRifle())
		WeaponComponent->InitWeapon(this);


	// Setup ArrowComponent
	if(IsValid(GetArrowComponent()))
		GetArrowComponent()->SetHiddenInGame(!bDebugArrow);

	//Setup default speed
	if(IsValid(GetCharacterMovement()))
	{
		_DefaultMaxWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
		_DefaultMinAnalogSpeed = GetCharacterMovement()->MinAnalogWalkSpeed;
		_DefaultAirControl = GetCharacterMovement()->AirControl;
		_DefaultGravityScale = GetCharacterMovement()->GravityScale;
	}
	
}

#pragma region Move
//------------------

#pragma region CharacterMovementComponent
//------------------

void AProjectSliceCharacter::OnMovementModeChanged(EMovementMode previousMovementMode, uint8 previousCustomMode)
{
	Super::OnMovementModeChanged(previousMovementMode, previousCustomMode);

	if(!IsValid(GetCharacterMovement())
		|| !IsValid(GetProceduralAnimComponent())
		|| !IsValid(GetParkourComponent())) return;

	if (GetCharacterMovement()->IsFalling()) _StartFallingTimestamp = GetWorld()->GetTimeSeconds();
	
	CoyoteTimeStart();
}

void AProjectSliceCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	
	//Dip on Landing
	 if(IsValid(GetProceduralAnimComponent()))
	 	GetProceduralAnimComponent()->LandingDip();

	//Trigger Camera Shake
	const float lastVelocityZ = (GetCapsuleVelocity() - GetPredCapsuleLocation()).Z;
	float velocityAlpha = UKismetMathLibrary::MapRangeClamped(FMath::Square(lastVelocityZ),  0.0f, FMath::Square(GetCharacterMovement()->GetMaxSpeed()), 0.0f,1.0f);
	float timeAlpha = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetTimeSeconds() - _StartFallingTimestamp, 0.0f, FallingMaxDurationToMaxShake, 0.0f, 1.0f);
	float scale = velocityAlpha * timeAlpha;
	GetFirstPersonCameraComponent()->ShakeCamera(EScreenShakeType::LANDING, scale);

	//Reset gravity scale (use for WallRunJumpOff)
	GetCharacterMovement()->GravityScale = _DefaultGravityScale;
	_StartFallingTimestamp = TNumericLimits<float>::Min();

	//Clear coyote time
	GetWorld()->GetTimerManager().ClearTimer(CoyoteTimerHandle);
}

//------------------
#pragma endregion CharacterMovementComponent

#pragma region Dash
//------------------


void AProjectSliceCharacter::Dash()
{
	if(!IsValid(GetParkourComponent())) return;

	GetParkourComponent()->OnDash();
}


//------------------
#pragma endregion Dash

#pragma region Crouch
//------------------

void AProjectSliceCharacter::Crouching()
{
	if(!IsValid(_PlayerController) || !IsValid(GetParkourComponent()) || !IsValid(GetWorld()) || !_PlayerController->CanCrouch()) return;

	if(GetParkourComponent()->IsWallRunning()
		|| GetParkourComponent()->IsLedging()
		||  GetParkourComponent()->GetMantlePhase() != EMantlePhase::NONE)
		return;

	_PlayerController->SetIsCrouchInputTrigger(!_PlayerController->IsCrouchInputTrigger());
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
	if(!IsValid(_PlayerController) || !IsValid(GetHookComponent()) || !IsValid(GetCharacterMovement())) return;
	
	OnJumpLocation = GetActorLocation();

	if(bIsCrouched)
		_PlayerController->SetIsCrouchInputTrigger(false);

	//Dettach rope if try jump on swing
	if(GetHookComponent()->IsObjectHooked() && GetHookComponent()->IsPlayerSwinging())
	{
		GetHookComponent()->DettachHook();
	}
	
	Super::Jump();
	
	//If in WallRunning 
	if(IsValid(GetParkourComponent()))
	{
		if(!GetParkourComponent()->IsWallRunning())
		{
			if(GetParkourComponent()->GetOverlapInfos().IsEmpty()) return;
			
			//Try Parkour if already overlap wall 
			for (const FOverlapInfo& currentOverlapInfo : GetParkourComponent()->GetOverlapInfos())
			{
				if (!IsValid(currentOverlapInfo.OverlapInfo.Component.Get()) || !IsValid(currentOverlapInfo.OverlapInfo.Component.Get()->GetOwner()) ||
					currentOverlapInfo.OverlapInfo.Component.Get()->GetOwner() == this) continue;
				GetParkourComponent()->OnComponentBeginOverlap.Broadcast(GetParkourComponent(), currentOverlapInfo.OverlapInfo.Component.Get()->GetOwner(), currentOverlapInfo.OverlapInfo.Component.Get(), currentOverlapInfo.GetBodyIndex(), currentOverlapInfo.bFromSweep, currentOverlapInfo.OverlapInfo);
				break;
			}
		}
		else
			GetParkourComponent()->JumpOffWallRun();
	}

	OnJumpEvent.Broadcast();

}

void AProjectSliceCharacter::

OnJumped_Implementation()
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

void AProjectSliceCharacter::Move(const FVector2D& inputValue)
{
	if(!IsValid(_PlayerController)) return;
		
	//0.2 is the Deadzone min threshold for Gamepad
	if (inputValue.Size() > 0.2)
	{
		const double inputWeight = UKismetMathLibrary::MapRangeClamped(GetVelocity().Length(), 0, GetCharacterMovement()->GetMaxSpeed(), _PlayerController->GetInputMaxSmoothingWeight(), _PlayerController->GetInputMinSmoothingWeight());
		const float moveX = FMath::WeightedMovingAverage(inputValue.X, _PlayerController->GetMoveInput().X, inputWeight);
		const float moveY = FMath::WeightedMovingAverage(inputValue.Y, _PlayerController->GetMoveInput().Y, inputWeight);
				
		_PlayerController->SetMoveInput(FVector2D(moveX, moveY));
		
		//Add movement
		AddMovementInput(GetActorForwardVector() * CustomTimeDilation,moveY);
		AddMovementInput(GetActorRightVector() * CustomTimeDilation, moveX);
	}
	else
	{
		StopMoving();
	}
	
}

void AProjectSliceCharacter::StopMoving() const
{
	_PlayerController->SetMoveInput(FVector2D::ZeroVector);
}

//------------------
#pragma endregion Move

#pragma region Look

void AProjectSliceCharacter::Look(const FVector2D& inputValue)
{
	// add yaw and pitch input to controller
	AddControllerYawInput(inputValue.X);
	AddControllerPitchInput(inputValue.Y);
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

#pragma region Glasses
//------------------

void AProjectSliceCharacter::Glasses()
{
	if(!IsValid(GetFirstPersonCameraComponent()))
		return;

	_bGlassesActive = !_bGlassesActive;

	//Trigger PostProcess Outline on face sliced
	GetFirstPersonCameraComponent()->TriggerGlasses(_bGlassesActive, true);
	GetFirstPersonCameraComponent()->DisplayOutlineOnSightedComp(_bGlassesActive);

	
}



#pragma endregion Glasses


#pragma region Weapon
//------------------

void AProjectSliceCharacter::Stow()
{
	bIsWeaponStow = !bIsWeaponStow;

	_PlayerController->SetCanFire(!bIsWeaponStow);

	//Callback
	OnStowEvent.Broadcast(bIsWeaponStow);
	
}

//------------------

#pragma endregion Weapon

