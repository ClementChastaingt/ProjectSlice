// Copyright Epic Games, Inc. All Rights Reserved.


#include "PS_WeaponComponent.h"

#include "..\..\PC\PS_Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "KismetProceduralMeshLibrary.h"
#include "PS_HookComponent.h"
#include "..\GPE\PS_SlicedComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Data/PS_TraceChannels.h"

// Sets default values for this component's properties
UPS_WeaponComponent::UPS_WeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	
	// Default offset from the _PlayerCharacter location for projectiles to spawn
	MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);

	//Create Component and Attach
	SightComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SightMesh"));
	RackDefaultRotation = SightComponent->GetRelativeRotation();
}

void UPS_WeaponComponent::BeginPlay()
{
	Super::BeginPlay();
}


void UPS_WeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//Smoothly rotate Sight Mesh
	if(bInterpRackRotation)
	{
		const float alpha = (GetWorld()->GetTimeSeconds() - InterpRackRotStartTimestamp) / RackRotDuration;
		float curveAlpha = alpha;
		if(IsValid(RackRotCurve))
			curveAlpha = RackRotCurve->GetFloatValue(alpha);

		const FRotator newRotation = FMath::Lerp(StartRackRotation,TargetRackRotation,curveAlpha);
		SightComponent->SetRelativeRotation(newRotation);

		//Stop Rot
		if(alpha > 1)
			bInterpRackRotation = false;
		
	}
		
}



void UPS_WeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController)) return;
	
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(_PlayerController->GetLocalPlayer()))
		Subsystem->RemoveMappingContext(FireMappingContext);
}

void UPS_WeaponComponent::AttachWeapon(AProjectSliceCharacter* Target_PlayerCharacter)
{	
	// Check that the _PlayerCharacter is valid, and has no rifle yet
	if (!IsValid(Target_PlayerCharacter)
		|| Target_PlayerCharacter->GetHasRifle()
		|| !IsValid(Target_PlayerCharacter->GetHookComponent())
		|| !IsValid(Target_PlayerCharacter->GetMesh()))return;
	
	// Attach the weapon to the First Person _PlayerCharacter
	this->SetupAttachment(Target_PlayerCharacter->GetMesh(), (TEXT("GripPoint")));

	// Attach Sight to Weapon
	SightComponent->SetupAttachment(this,FName("Muzzle"));

	// Attach Hook to Weapon
	_HookComponent = Target_PlayerCharacter->GetHookComponent();
	_HookComponent->SetupAttachment(this,FName("HookAttach"));
	
	// switch bHasRifle so the animation blueprint can switch to another animation set
	Target_PlayerCharacter->SetHasRifle(true);
	
	_HookComponent->OnAttachWeapon();
	
}

void UPS_WeaponComponent::InitWeapon(AProjectSliceCharacter* Target_PlayerCharacter)
{
	_PlayerCharacter = Target_PlayerCharacter;
	_PlayerController = Cast<APlayerController>(Target_PlayerCharacter->GetController());
	
	if(!IsValid(_PlayerController)) return;
		
	//----Input----
	// Set up action bindings
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
		_PlayerController->GetLocalPlayer()))
	{
		// Set the priority of the mapping to 1, so that it overrides the Jump action with the Fire action when using touch input
		Subsystem->AddMappingContext(FireMappingContext, 1);
	}

	//BindAction
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(_PlayerController->InputComponent))
	{
		// Fire
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &UPS_WeaponComponent::Fire);

		// Rotate Rack
		EnhancedInputComponent->BindAction(TurnRackAction, ETriggerEvent::Triggered, this, &UPS_WeaponComponent::TurnRack);

		// Hook Launch
		EnhancedInputComponent->BindAction(HookAction, ETriggerEvent::Triggered, this, &UPS_WeaponComponent::HookObject);
		EnhancedInputComponent->BindAction(HookAction, ETriggerEvent::Ongoing, this, &UPS_WeaponComponent::OnStartWinderPull);
		EnhancedInputComponent->BindAction(HookAction, ETriggerEvent::Completed, this, &UPS_WeaponComponent::OnStopWinderPull);
	}

	OnWeaponInit.Broadcast();
}


#pragma region Input
//__________________________________________________

void UPS_WeaponComponent::Fire()
{
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(GetWorld())) return;

	//Try Slice a Mesh
	//Trace Loc && Rot
	const FRotator SpawnRotation = _PlayerController->PlayerCameraManager->GetCameraRotation();
	// MuzzleOffset is in camera space, so transform it to world space before offsetting from the _PlayerCharacter location to find the final muzzle position
	//const FVector SpawnLocation = GetOwner()->GetActorLocation() + SpawnRotation.RotateVector(MuzzleOffset);

	//Trace config
	const TArray<AActor*> ActorsToIgnore{_PlayerCharacter};
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), SightComponent->GetComponentLocation(),
	                                      SightComponent->GetComponentLocation() + SpawnRotation.Vector() * MaxFireDistance,
	                                      UEngineTypes::ConvertToTraceType(ECC_Slice), false, ActorsToIgnore,
	                                        bDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, CurrentFireHitResult, true);

	if (!CurrentFireHitResult.bBlockingHit || !IsValid(CurrentFireHitResult.GetComponent()->GetOwner())) return;


	//Cut ProceduralMesh
	UProceduralMeshComponent* currentProcMeshComponent = Cast<UProceduralMeshComponent>(CurrentFireHitResult.GetComponent());
	UPS_SlicedComponent* currentSlicedComponent = Cast<UPS_SlicedComponent>(CurrentFireHitResult.GetActor()->GetComponentByClass(UPS_SlicedComponent::StaticClass()));

	if (!IsValid(currentProcMeshComponent) || !IsValid(currentSlicedComponent)) return;

	UProceduralMeshComponent* outHalfComponent;
	UKismetProceduralMeshLibrary::SliceProceduralMesh(currentProcMeshComponent, CurrentFireHitResult.Location,
	                                                  SightComponent->GetUpVector(), true,
	                                                  outHalfComponent,
	                                                  EProcMeshSliceCapOption::CreateNewSectionForCap,
	                                                  HalfSectionMaterial);
	outHalfComponent->RegisterComponent();
	if(IsValid(currentProcMeshComponent->GetOwner()))
	{
		Cast<UMeshComponent>(CurrentFireHitResult.GetActor()->GetRootComponent())->SetCollisionResponseToChannel(ECC_Rope, ECR_Ignore);
		currentProcMeshComponent->GetOwner()->AddInstanceComponent(outHalfComponent);
	}

	
	//Init Physic Config+
	outHalfComponent->bUseComplexAsSimpleCollision = false;
	outHalfComponent->SetGenerateOverlapEvents(true);
	outHalfComponent->SetCollisionProfileName(Profile_GPE, false);
	outHalfComponent->SetNotifyRigidBodyCollision(true);
	outHalfComponent->SetSimulatePhysics(true);


	//Impulse
	//TODO :: Rework Impulse
	outHalfComponent->AddImpulse(FVector(500, 500, 500), NAME_None, true);
	
	
	// Try and play the sound if specified
	if (IsValid(FireSound))
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, _PlayerCharacter->GetActorLocation());
	
	// Try and play a firing animation if specified
	if (IsValid(FireAnimation))
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = _PlayerCharacter->GetMesh()->GetAnimInstance();
		if (IsValid(AnimInstance))
			AnimInstance->Montage_Play(FireAnimation, 1.f);
	}
}

void UPS_WeaponComponent::TurnRack()
{
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(SightComponent)) return;
	
	bRackInHorizontal = !bRackInHorizontal;

	StartRackRotation = SightComponent->GetRelativeRotation();
	TargetRackRotation = RackDefaultRotation;
	TargetRackRotation.Roll = RackDefaultRotation.Roll + (bRackInHorizontal ? 1 : -1 * 90);

	InterpRackRotStartTimestamp = GetWorld()->GetTimeSeconds();
	bInterpRackRotation = true;
	
}

void UPS_WeaponComponent::HookObject()
{
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(_HookComponent)) return;
	
	_HookComponent->HookObject();
}

void UPS_WeaponComponent::OnStartWinderPull()
{
	UE_LOG(LogTemp, Log, TEXT("%S"), __FUNCTION__);
	_HookComponent->SetCableWinderPull(true);
}

void UPS_WeaponComponent::OnStopWinderPull()
{
	_HookComponent->SetCableWinderPull(false);
}


//__________________________________________________
#pragma endregion Input


