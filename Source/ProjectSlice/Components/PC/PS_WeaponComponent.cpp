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

// Sets default values for this component's properties
UPS_WeaponComponent::UPS_WeaponComponent()
{

	PrimaryComponentTick.bCanEverTick = true;
	
	// Default offset from the _PlayerCharacter location for projectiles to spawn
	MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);

	//Create Component and Attach
	SightComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SightMesh"));
	SightComponent->SetRelativeScale3D(SightDefaultTransform.GetScale3D());
	
}

void UPS_WeaponComponent::BeginPlay()
{
	HookComponent = Cast<UPS_HookComponent>(GetOwner()->GetComponentByClass(UPS_HookComponent::StaticClass()));
	if(IsValid(HookComponent))
		HookComponent->SetRelativeScale3D(SightDefaultTransform.GetScale3D());
	else
		UE_LOG(LogTemp, Error, TEXT("HookComponent :: Invalid %s "), *GetOwner()->GetName());
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
	if (!IsValid(GetPlayerCharacter()) || !IsValid(GetPlayerController())) return;
	
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetPlayerController()->GetLocalPlayer()))
		Subsystem->RemoveMappingContext(FireMappingContext);
}

void UPS_WeaponComponent::AttachWeapon(AProjectSliceCharacter* Target_PlayerCharacter)
{
	_PlayerCharacter = Target_PlayerCharacter;
	_PlayerController = Cast<APlayerController>(_PlayerCharacter->GetController());

	// Check that the _PlayerCharacter is valid, and has no rifle yet
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || _PlayerCharacter->GetHasRifle())return;

	// Attach the weapon to the First Person _PlayerCharacter
	const FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(_PlayerCharacter->GetMesh1P(), AttachmentRules, FName(TEXT("GripPoint")));
	SightComponent->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale,FName("Muzzle"));
	HookComponent->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale,FName("HookAttach"));
	
	
	// switch bHasRifle so the animation blueprint can switch to another animation set
	GetPlayerCharacter()->SetHasRifle(true);
	
	//Setup Sight Mesh Loc && Rot
	if(IsValid(SightComponent))
	{
		SightComponent->SetRelativeLocation(SightDefaultTransform.GetLocation());
		SightComponent->SetRelativeRotation(SightDefaultTransform.Rotator());
	}

	//----Input----
	// Set up action bindings
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
		GetPlayerController()->GetLocalPlayer()))
	{
		// Set the priority of the mapping to 1, so that it overrides the Jump action with the Fire action when using touch input
		Subsystem->AddMappingContext(FireMappingContext, 1);
	}

	//BindAction
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(GetPlayerController()->InputComponent))
	{
		// Fire
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &UPS_WeaponComponent::Fire);

		// Rotate Rack
		EnhancedInputComponent->BindAction(TurnRackAction, ETriggerEvent::Triggered, this, &UPS_WeaponComponent::TurnRack);

		// Hook Launch
		EnhancedInputComponent->BindAction(HookAction, ETriggerEvent::Triggered, this, &UPS_WeaponComponent::Grapple);
	}
}

#pragma region Input
//__________________________________________________

void UPS_WeaponComponent::Fire()
{
	if (!IsValid(GetPlayerCharacter()) || !IsValid(GetPlayerController()) || !IsValid(GetWorld())) return;

	//Try Slice a Mesh
	
	//Trace Loc && Rot
	const FRotator SpawnRotation = _PlayerController->PlayerCameraManager->GetCameraRotation();
	// MuzzleOffset is in camera space, so transform it to world space before offsetting from the _PlayerCharacter location to find the final muzzle position
	//const FVector SpawnLocation = GetOwner()->GetActorLocation() + SpawnRotation.RotateVector(MuzzleOffset);

	//Trace config
	const TArray<AActor*> ActorsToIgnore{GetPlayerCharacter()};

	//TODO :: Config Collision channel
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), SightComponent->GetComponentLocation(),
	                                      SightComponent->GetComponentLocation() + SpawnRotation.Vector() * 1000,
	                                      UEngineTypes::ConvertToTraceType(ECC_Visibility), false, ActorsToIgnore,
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
	                                                  EProcMeshSliceCapOption::UseLastSectionForCap,
	                                                  HalfSectionMaterial);
	
	currentSlicedComponent->ChildsProcMesh.Add(outHalfComponent);
	
	//Init Physic Config+
	outHalfComponent->bUseComplexAsSimpleCollision = false;
	outHalfComponent->SetCollisionProfileName(TEXT("PhysicsActor"), false);
	outHalfComponent->SetGenerateOverlapEvents(false);
	outHalfComponent->SetNotifyRigidBodyCollision(true);
	outHalfComponent->SetSimulatePhysics(true);

	//Impulse
	outHalfComponent->AddImpulse(FVector(200, 200, 200));
	
	
	// Try and play the sound if specified
	if (IsValid(FireSound))
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetPlayerCharacter()->GetActorLocation());
	
	// Try and play a firing animation if specified
	if (IsValid(FireAnimation))
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = GetPlayerCharacter()->GetMesh1P()->GetAnimInstance();
		if (IsValid(AnimInstance))
			AnimInstance->Montage_Play(FireAnimation, 1.f);
	}
}

void UPS_WeaponComponent::TurnRack()
{
	if (!IsValid(GetPlayerCharacter()) || !IsValid(GetPlayerController()) || !IsValid(SightComponent)) return;
	
	bRackInHorizontal = !bRackInHorizontal;

	StartRackRotation = SightComponent->GetRelativeRotation();
	TargetRackRotation = SightDefaultTransform.Rotator();
	TargetRackRotation.Roll = SightDefaultTransform.Rotator().Roll + (bRackInHorizontal ? 1 : -1 * 90);

	InterpRackRotStartTimestamp = GetWorld()->GetTimeSeconds();
	bInterpRackRotation = true;
	
}

void UPS_WeaponComponent::Grapple()
{
	if (!IsValid(GetPlayerCharacter()) || !IsValid(GetPlayerController()) || !IsValid(HookComponent)) return;

	//Break Hook constraint if already exist
	if(HookComponent->IsConstrainted())
	{
		HookComponent->DettachGrapple();
		return;
	}
		
	//Trace config
	const FRotator SpawnRotation = _PlayerController->PlayerCameraManager->GetCameraRotation();
	const TArray<AActor*> ActorsToIgnore{GetPlayerCharacter(), GetOwner()};
	
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), SightComponent->GetComponentLocation(),
										  SightComponent->GetComponentLocation() + SpawnRotation.Vector() * 1000,
										  UEngineTypes::ConvertToTraceType(ECC_PhysicsBody), false, ActorsToIgnore,
										  bDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, CurrentHookHitResult, true);

	
	if (!CurrentFireHitResult.bBlockingHit || !IsValid(CurrentFireHitResult.GetComponent())) return;

	HookComponent->GrappleObject(CurrentFireHitResult.GetComponent(), FName("None"));

}


//__________________________________________________
#pragma endregion Input


