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
#include "Analytics/RPCDoSDetectionAnalytics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Data/PS_TraceChannels.h"

// Sets default values for this component's properties
UPS_WeaponComponent::UPS_WeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	
	// Default offset from the _PlayerCharacter location for projectiles to spawn
	MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);

	//Create Component and Attach
	SightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SightMesh"));
	SightMesh->SetCollisionProfileName(Profile_NoCollision);
	SightMesh->SetGenerateOverlapEvents(false);
	RackDefaultRotation = SightMesh->GetRelativeRotation();

}

void UPS_WeaponComponent::BeginPlay()
{
	Super::BeginPlay();
}


void UPS_WeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//TODO :: Made custom tick for that
	SightShaderTick();

	SightMeshRotation();
		
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
	SightMesh->SetupAttachment(this,FName("Muzzle"));

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
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &UPS_WeaponComponent::FireTriggered);

		// Rotate Rack
		EnhancedInputComponent->BindAction(TurnRackAction, ETriggerEvent::Triggered, this, &UPS_WeaponComponent::TurnRack);

		//Hook Launch
		EnhancedInputComponent->BindAction(HookAction, ETriggerEvent::Triggered, this, &UPS_WeaponComponent::HookObject);

		//Winder Launch
		EnhancedInputComponent->BindAction(WinderAction, ETriggerEvent::Triggered, this, &UPS_WeaponComponent::WindeHook);
		EnhancedInputComponent->BindAction(WinderAction, ETriggerEvent::Completed, this, &UPS_WeaponComponent::WindeHook);				
		
	}

	OnWeaponInit.Broadcast();
}

#pragma region Input
//__________________________________________________

void UPS_WeaponComponent::FireTriggered()
{
	//Update Holding Fire
	bIsHoldingFire = !bIsHoldingFire;

	//Slice when release
	if(!bIsHoldingFire)
	{
		if( _CurrentSightedMatInst->IsValidLowLevel())
		{
			_CurrentSightedMatInst->SetScalarParameterValue(FName("bIsHoldingFire"), bIsHoldingFire ? -1 : 1);
			_CurrentSightedMatInst->SetScalarParameterValue(FName("SliceBumpAlpha"), 0.0f);
		}
		Fire();
	}
	else
		SetupSliceBump();
	
}

void UPS_WeaponComponent::Fire()
{
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(GetWorld())) return;

	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__);
	
	//Try Slice a Mesh
	const FRotator SpawnRotation = _PlayerController->PlayerCameraManager->GetCameraRotation();
	// MuzzleOffset is in camera space, so transform it to world space before offsetting from the _PlayerCharacter location to find the final muzzle position
	//const FVector SpawnLocation = GetOwner()->GetActorLocation() + SpawnRotation.RotateVector(MuzzleOffset);

	
	//Trace config
	const TArray<AActor*> ActorsToIgnore{_PlayerCharacter};
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), SightMesh->GetComponentLocation(),
	                                      SightMesh->GetComponentLocation() + SightMesh->GetForwardVector() * MaxFireDistance,
	                                      UEngineTypes::ConvertToTraceType(ECC_Slice), false, ActorsToIgnore,
	                                        false ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, CurrentFireHitResult, true);

	if (!CurrentFireHitResult.bBlockingHit || !IsValid(CurrentFireHitResult.GetComponent()->GetOwner())) return;
	
	//Cut ProceduralMesh
	UProceduralMeshComponent* currentProcMeshComponent = Cast<UProceduralMeshComponent>(CurrentFireHitResult.GetComponent());
	UPS_SlicedComponent* currentSlicedComponent = Cast<UPS_SlicedComponent>(CurrentFireHitResult.GetActor()->GetComponentByClass(UPS_SlicedComponent::StaticClass()));

	if (!IsValid(currentProcMeshComponent) || !IsValid(currentSlicedComponent)) return;
	
	//Setup material
	ResetSightRackProperties();
	
	// UMaterialInstanceDynamic* matInst  = UKismetMaterialLibrary::CreateDynamicMaterialInstance(GetWorld(), HalfSectionMaterial);
	// if(!IsValid(matInst) || !IsValid(GetWorld()) || !IsValid(currentProcMeshComponent->GetMaterial(0))) return;
	//
	// matInst->SetScalarParameterValue(FName("StartTime"), GetWorld()->GetTimeSeconds());
	//
	// FLinearColor baseMaterialColor;
	// currentProcMeshComponent->GetMaterial(0)->GetVectorParameterValue(FName("Base Color"), baseMaterialColor);
	// matInst->SetVectorParameterValue(FName("TargetColor"), baseMaterialColor);


	if(!IsValid(GetWorld()) || !IsValid(currentProcMeshComponent->GetMaterial(0))) return;
	
	UMaterialInstanceDynamic* matInst  = UKismetMaterialLibrary::CreateDynamicMaterialInstance(GetWorld(), currentProcMeshComponent->GetMaterial(0));
	if(!IsValid(matInst)) return;
	
	matInst->SetScalarParameterValue(FName("StartTime"), GetWorld()->GetTimeSeconds());
	matInst->SetScalarParameterValue(FName("bIsMelting"), true);

	//Slice mesh
	UProceduralMeshComponent* outHalfComponent;
	//TODO :: replace by EProcMeshSliceCapOption::CreateNewSectionForCap
	FVector sliceDir = SightMesh->GetUpVector() * currentProcMeshComponent->GetComponentScale();
	sliceDir.Normalize();
	
	UKismetProceduralMeshLibrary::SliceProceduralMesh(currentProcMeshComponent, CurrentFireHitResult.Location,
	                                                  sliceDir, true,
	                                                  outHalfComponent,
	                                                  EProcMeshSliceCapOption::UseLastSectionForCap,
	                                                  matInst);
	if(!IsValid(outHalfComponent)) return;
	
	if(bDebugSlice)
	{
		DrawDebugLine(GetWorld(), CurrentFireHitResult.Location,  CurrentFireHitResult.Location + sliceDir * 500, FColor::Magenta, false, 2, 10, 3);
		DrawDebugLine(GetWorld(), SightMesh->GetComponentLocation(),  SightMesh->GetComponentLocation() +  SightMesh->GetUpVector() * 500, FColor::Yellow, false, 2, 10, 3);
	}

	//Register and instanciate
	outHalfComponent->RegisterComponent();
	if(IsValid(currentProcMeshComponent->GetOwner()))
	{
		//Cast<UMeshComponent>(CurrentFireHitResult.GetActor()->GetRootComponent())->SetCollisionResponseToChannel(ECC_Rope, ECR_Ignore);
		currentProcMeshComponent->GetOwner()->AddInstanceComponent(outHalfComponent);
	}
		
	//Init Physic Config
	outHalfComponent->bUseComplexAsSimpleCollision = false;
	outHalfComponent->SetGenerateOverlapEvents(true);
	outHalfComponent->SetCollisionProfileName(Profile_GPE, true);
	outHalfComponent->SetNotifyRigidBodyCollision(true);
	outHalfComponent->SetSimulatePhysics(true);
	currentProcMeshComponent->SetSimulatePhysics(true);


	//Impulse
	//TODO :: Rework Impulse
	outHalfComponent->AddImpulse(FVector(500, 0, 500), NAME_None, true);
	
	
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
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(SightMesh)) return;
	
	bRackInHorizontal = !bRackInHorizontal;

	StartRackRotation = SightMesh->GetRelativeRotation();
	TargetRackRotation = RackDefaultRotation;
	TargetRackRotation.Roll = RackDefaultRotation.Roll + (bRackInHorizontal ? 1 : -1 * 90);

	InterpRackRotStartTimestamp = GetWorld()->GetAudioTimeSeconds();
	bInterpRackRotation = true;
	
}

void UPS_WeaponComponent::HookObject()
{
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(_HookComponent)) return;
	
	_HookComponent->HookObject();
}

void UPS_WeaponComponent::WindeHook()
{
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(_HookComponent)) return;
	
	_HookComponent->WindeHook();
}

void UPS_WeaponComponent::StopWindeHook()
{
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(_HookComponent)) return;
	
	_HookComponent->StopWindeHook();
}


//__________________________________________________
#pragma endregion Input

#pragma region Sight
//------------------

void UPS_WeaponComponent::SightMeshRotation()
{
	//Smoothly rotate Sight Mesh
	if(bInterpRackRotation)
	{
		const float alpha = (GetWorld()->GetAudioTimeSeconds() - InterpRackRotStartTimestamp) / RackRotDuration;
		float curveAlpha = alpha;
		if(IsValid(RackRotCurve))
			curveAlpha = RackRotCurve->GetFloatValue(alpha);

		const FRotator newRotation = FMath::Lerp(StartRackRotation,TargetRackRotation,curveAlpha);
		SightMesh->SetRelativeRotation(newRotation);

		//Stop Rot
		if(alpha > 1)
			bInterpRackRotation = false;
		
	}
}

void UPS_WeaponComponent::SightShaderTick()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(GetWorld())) return;

	const FVector start = GetSightMeshComponent()->GetComponentLocation();
	const FVector target =  GetSightMeshComponent()->GetComponentLocation() + GetSightMeshComponent()->GetForwardVector() * MaxFireDistance;
		
	FHitResult outHit;
	const TArray<AActor*> actorsToIgnore = {_PlayerCharacter};
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), start, target, UEngineTypes::ConvertToTraceType(ECC_Slice),
		false, actorsToIgnore, EDrawDebugTrace::None, outHit, true);

	//TODO:: Change by laser VFX
	DrawDebugLine(GetWorld(), start, target, FColor::Red, false);
	
	//On shoot Bump tick logic 
	SliceBump();
	
	if(outHit.bBlockingHit && IsValid(outHit.GetActor()))
	{
		if(!IsValid(outHit.GetComponent())) return;
		
		if(bDebugSightShader)
		{
			UE_LOG(LogTemp, Log, TEXT("%S :: %s , origin %s, extent %s"), __FUNCTION__, *outHit.GetComponent()->GetName(), *outHit.GetComponent()->GetLocalBounds().Origin.ToString(), *outHit.GetComponent()->GetLocalBounds().BoxExtent.ToString());
			DrawDebugBox(GetWorld(),outHit.GetComponent()->GetComponentLocation() + outHit.GetComponent()->GetLocalBounds().Origin, outHit.GetComponent()->GetLocalBounds().BoxExtent, FColor::Yellow, false,-1 , 1 ,2);
		}
		
		if(IsValid(_CurrentSightedComponent) && _CurrentSightedComponent == outHit.GetComponent())
			return;
		
		//Reset last material properties
		ResetSightRackProperties();
		if(!IsValid(outHit.GetComponent()->GetMaterial(0)) /*|| !outHit.GetActor()->ActorHasTag(FName("Sliceable"))*/) return;
		_CurrentSightedComponent = outHit.GetComponent();
				
		//Set new Object Mat instance
		UMaterialInstanceDynamic* matInstObject  = UKismetMaterialLibrary::CreateDynamicMaterialInstance(GetWorld(), _CurrentSightedComponent->GetMaterial(0));
		if(!IsValid(matInstObject)) return;
		matInstObject->SetScalarParameterValue(FName("bIsInUse"), true);

		_CurrentSightedMatInst = matInstObject;
		_CurrentSightedBaseMat = _CurrentSightedComponent->GetMaterial(0);
		_CurrentSightedComponent->SetMaterial(0, _CurrentSightedMatInst);

		//Setup Bump to Old params if effective
		ForceInitSliceBump();

		if(bDebugSightShader) UE_LOG(LogTemp, Log, TEXT("%S :: activate sight shader on %s"), __FUNCTION__, *_CurrentSightedComponent->GetName());
	}
	//If don't Lbock reset old mat properties
	else if(!outHit.bBlockingHit && IsValid(_CurrentSightedComponent))
		ResetSightRackProperties();
		
}

void UPS_WeaponComponent::ResetSightRackProperties()
{
	if(IsValid(_CurrentSightedComponent) && _CurrentSightedMatInst->IsValidLowLevel())
	{
		if(bDebugSightShader) UE_LOG(LogTemp, Warning, TEXT("%S :: reset %s with %s material"), __FUNCTION__, *_CurrentSightedComponent->GetName(), *_CurrentSightedBaseMat->GetName());
		
		_CurrentSightedComponent->SetMaterial(0, _CurrentSightedBaseMat);
		_CurrentSightedComponent = nullptr;
		_CurrentSightedMatInst->MarkAsGarbage();
		_CurrentSightedMatInst = nullptr;
	}
}

void UPS_WeaponComponent::ForceInitSliceBump()
{
	if( _CurrentSightedMatInst->IsValidLowLevel())
	{
		_CurrentSightedMatInst->SetScalarParameterValue(FName("bIsHoldingFire"), bIsHoldingFire ? -1 : 1);
		_CurrentSightedMatInst->SetScalarParameterValue(FName("SliceBumpAlpha"), CurrentCurveAlpha);
	}
}


void UPS_WeaponComponent::SetupSliceBump()
{
	if(IsValid(_CurrentSightedComponent) && _CurrentSightedMatInst->IsValidLowLevel() && IsValid(GetWorld()))
	{
		if(bDebugSightShader) UE_LOG(LogTemp, Warning, TEXT("%S :: Bump %s"), __FUNCTION__, bIsHoldingFire ? TEXT("IN") : TEXT("OUT"));


		StartSliceBumpTimestamp = GetWorld()->GetTimeSeconds();
		bSliceBumping = true;

		_CurrentSightedMatInst->SetScalarParameterValue(FName("bIsHoldingFire"), bIsHoldingFire ? -1 : 1);
	}
}

void UPS_WeaponComponent::SliceBump()
{
	if(!IsValid(_CurrentSightedComponent) || !_CurrentSightedMatInst->IsValidLowLevel() || !IsValid(GetWorld())) return;

	if(bSliceBumping)
	{
		const float alpha = (GetWorld()->GetTimeSeconds() - StartSliceBumpTimestamp) / SliceBumpDuration;

		CurrentCurveAlpha = alpha;
		if(IsValid(SliceBumpCurve))
		{
			CurrentCurveAlpha = SliceBumpCurve->GetFloatValue(alpha);
		}

		if(bDebugSightSliceBump) UE_LOG(LogTemp, Log, TEXT("%S :: curveAlpha %f "), __FUNCTION__, CurrentCurveAlpha);
		_CurrentSightedMatInst->SetScalarParameterValue(FName("SliceBumpAlpha"), CurrentCurveAlpha);

		if(alpha >= 1)
			bSliceBumping = false;
	}
			
}

//------------------
#pragma endregion Sight
	


