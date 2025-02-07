// Copyright Epic Games, Inc. All Rights Reserved.


#include "PS_WeaponComponent.h"

#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputSubsystems.h"
#include "KismetProceduralMeshLibrary.h"
#include "PS_HookComponent.h"
#include "PS_PlayerCameraComponent.h"
#include "..\GPE\PS_SlicedComponent.h"
#include "Engine/DamageEvents.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Character/PC/PS_Character.h"
#include "ProjectSlice/Character/PC/PS_PlayerController.h"
#include "ProjectSlice/Data/PS_Constants.h"
#include "ProjectSlice/Data/PS_TraceChannels.h"
#include "ProjectSlice/FunctionLibrary/PSCustomProcMeshLibrary.h"
#include "ProjectSlice/FunctionLibrary/PSFl.h"

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

}

FVector UPS_WeaponComponent::GetMuzzlePosition()
{
	return GetSocketLocation(FName("Muzzle"));
}

void UPS_WeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	//Setup default value
	RackDefaultRelativeTransform = SightMesh->GetRelativeTransform();
	TargetRackRotation = RackDefaultRelativeTransform.Rotator();

	//Custom Tick
	if (IsValid(GetWorld()))
	{
		FTimerDelegate wallRunTick_TimerDelegate;
		wallRunTick_TimerDelegate.BindUObject(this, &UPS_WeaponComponent::RackTick);
		GetWorld()->GetTimerManager().SetTimer(_RackTickTimerHandle, wallRunTick_TimerDelegate, RackTickRate, true);
		GetWorld()->GetTimerManager().PauseTimer(_RackTickTimerHandle);
	}
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
		Subsystem->RemoveMappingContext(_PlayerController->GetFireMappingContext());
}

void UPS_WeaponComponent::AttachWeapon(AProjectSliceCharacter* targetPlayerCharacter)
{
	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__);
	// Check that the _PlayerCharacter is valid, and has no rifle yet
	if (!IsValid(targetPlayerCharacter)
		|| targetPlayerCharacter->GetHasRifle()
		|| !IsValid(targetPlayerCharacter->GetHookComponent())
		|| !IsValid(targetPlayerCharacter->GetMesh()))return;
	
	// Attach the weapon to the First Person _PlayerCharacter
	this->SetupAttachment(targetPlayerCharacter->GetMesh(), (TEXT("GripPoint")));

	// Attach Sight to Weapon
	SightMesh->SetupAttachment(this,FName("Muzzle"));
		
	// switch bHasRifle so the animation blueprint can switch to another animation set
	targetPlayerCharacter->SetHasRifle(true);

	// Link Hook to Weapon
	targetPlayerCharacter->GetHookComponent()->OnAttachWeapon();

	// Link ForceComp to Weapon
	_ForceComponent = targetPlayerCharacter->GetForceComponent();
}

void UPS_WeaponComponent::InitWeapon(AProjectSliceCharacter* Target_PlayerCharacter)
{
	_PlayerCharacter = Target_PlayerCharacter;
	_PlayerController = Cast<AProjectSlicePlayerController>(Target_PlayerCharacter->GetController());
	_PlayerCamera = _PlayerCharacter->GetFirstPersonCameraComponent();
	
	if(!IsValid(_PlayerController)) return;
		
	//----Input----
	// Set up action bindings
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
		_PlayerController->GetLocalPlayer()))
	{
		// Set the priority of the mapping to 1, so that it overrides the Jump action with the Fire action when using touch input
		Subsystem->AddMappingContext(_PlayerController->GetFireMappingContext(), 1);
	}

	//Setup input Weapon
	_PlayerController->SetupWeaponInputComponent();

	OnWeaponInit.Broadcast();
}

#pragma region Input
//__________________________________________________

void UPS_WeaponComponent::FireTriggered()
{
	if(!IsValid(_PlayerController) ||!_PlayerController->CanFire()) return;
	
	//Update Holding Fire
	bIsHoldingFire = !bIsHoldingFire;

	//Slice when release
	if(!bIsHoldingFire)
	{
		for (UMaterialInstanceDynamic* currentSightedMatElement : _CurrentSightedMatInst)
		{
			if(IsValid(currentSightedMatElement))
			{
				currentSightedMatElement->SetScalarParameterValue(FName("bIsHoldingFire"), bIsHoldingFire ? -1 : 1);
				currentSightedMatElement->SetScalarParameterValue(FName("SliceBumpAlpha"), 0.0f);
			}
		}
		Fire();
	}
	else
		SetupSliceBump();
	
}

//__________________________________________________
#pragma endregion Input

#pragma region Fire
//------------------


void UPS_WeaponComponent::Fire()
{
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(GetWorld())) return;

	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__);
	
	if (!_SightHitResult.bBlockingHit || !IsValid(_SightHitResult.GetComponent()->GetOwner())) return;
	
	//Cut ProceduralMesh
	UProceduralMeshComponent* parentProcMeshComponent = Cast<UProceduralMeshComponent>(_SightHitResult.GetComponent());
	UPS_SlicedComponent* currentSlicedComponent = Cast<UPS_SlicedComponent>(_SightHitResult.GetActor()->GetComponentByClass(UPS_SlicedComponent::StaticClass()));

	if (!IsValid(parentProcMeshComponent) || !IsValid(currentSlicedComponent)) return;
	
	//Setup material
	ResetSightRackProperties();
	UMaterialInstanceDynamic* matInst = SetupMeltingMat(parentProcMeshComponent);

	//Slice mesh
	UProceduralMeshComponent* outHalfComponent;
	FVector sliceLocation = _SightHitResult.Location;
	FVector sliceDir = SightMesh->GetUpVector();
	// UMeshComponent* sliceTarget = Cast<UMeshComponent>(CurrentFireHitResult.GetComponent());
	// FVector sliceDir = SightMesh->GetUpVector() / (sliceTarget->GetLocalBounds().BoxExtent * sliceTarget->GetComponentScale());
	sliceDir.Normalize();
	
	UPSCustomProcMeshLibrary::SliceProcMesh(parentProcMeshComponent, sliceLocation,
		sliceDir, true,
		outHalfComponent, _sliceOutput,
		IsValid(matInst) ? EProcMeshSliceCapOption::CreateNewSectionForCap : EProcMeshSliceCapOption::UseLastSectionForCap,
		matInst);
	if(!IsValid(outHalfComponent)) return;
	
	// Ensure collision is generated for both meshes
	outHalfComponent->bUseComplexAsSimpleCollision = false;
	
	// for (int32 newProcMeshSection = 0; newProcMeshSection < outHalfComponent->GetNumSections(); newProcMeshSection++)
	// {
	// 	UpdateMeshTangents(outHalfComponent, newProcMeshSection);
	// }
	//
	// for (int32 currentProcMeshSection = 0; currentProcMeshSection < parentProcMeshComponent->GetNumSections(); currentProcMeshSection++)
	// {
	// 	UpdateMeshTangents(parentProcMeshComponent, currentProcMeshSection);
	// }
	
	outHalfComponent->UpdateBounds();

	//Debug trace
	if(bDebugSlice)
	{
		DrawDebugLine(GetWorld(), _SightHitResult.Location,  _SightHitResult.Location + sliceDir * 500, FColor::Magenta, false, 2, 10, 3);
		DrawDebugLine(GetWorld(),GetMuzzlePosition(), GetMuzzlePosition() +  SightMesh->GetUpVector() * 500, FColor::Yellow, false, 2, 10, 3);
		DrawDebugLine(GetWorld(),GetMuzzlePosition() + SightMesh->GetUpVector() * 500 , _SightHitResult.Location + sliceDir  * 500, FColor::Green, false, 2, 10, 3);
	}

	//Register and instanciate
	outHalfComponent->RegisterComponent();
	if(IsValid(parentProcMeshComponent->GetOwner()))
	{
		//Cast<UMeshComponent>(CurrentFireHitResult.GetActor()->GetRootComponent())->SetCollisionResponseToChannel(ECC_Rope, ECR_Ignore);
		parentProcMeshComponent->GetOwner()->AddInstanceComponent(outHalfComponent);
	}
		
	//Init Physic Config;
	outHalfComponent->SetGenerateOverlapEvents(true);
	outHalfComponent->SetCollisionProfileName(Profile_GPE, true);
	outHalfComponent->SetNotifyRigidBodyCollision(true);

	UPhysicalMaterial* physMat = currentSlicedComponent->BodyInstance.GetSimplePhysicalMaterial();
	outHalfComponent->SetPhysMaterialOverride(physMat);	
	outHalfComponent->SetSimulatePhysics(true);
	
	parentProcMeshComponent->SetSimulatePhysics(true);
	
	//Impulse
	if(ActivateImpulseOnSlice)
	{
		FDamageEvent damageEvent = FDamageEvent();
		outHalfComponent->ReceiveComponentDamage(10000,damageEvent,_PlayerController,_PlayerCharacter);
		outHalfComponent->AddImpulse((outHalfComponent->GetUpVector() * -1) * outHalfComponent->GetMass() , NAME_None, false);
		
		//TODO :: Rework Impulse
		//outHalfComponent->AddImpulse(FVector(500, 0, 500), NAME_None, true);
		//outHalfComponent->AddImpulse(_PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector() + CurrentFireHitResult.Normal * 500, NAME_None, true);
	}
	
	// Try and play the sound if specified
	if (IsValid(FireSound))
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, _PlayerCharacter->GetActorLocation());
	
	// Try and play a firing animation if specified
	if (IsValid(FireAnimation))
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = _PlayerCharacter->GetMesh()->GetAnimInstance();
		if (IsValid(AnimInstance))
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	
	}
}

//------------------

#pragma endregion Fire

#pragma region Sight
//------------------

#pragma region Rack
//------------------

void UPS_WeaponComponent::TurnRack()
{
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(SightMesh)) return;
	
	StartRackRotation = SightMesh->GetRelativeRotation().Clamp();
	
	const float divider = UKismetMathLibrary::SafeDivide(StartRackRotation.Roll, 90.0f) + 1.0f;
	const float rollTarget = 90.0f * UKismetMathLibrary::FTrunc(divider);
	
	if(bDebugSightRack) UE_LOG(LogTemp, Log, TEXT("%S :: DividerRounded %i, Divider %f, RollTarget %f, Currentroll %f"), __FUNCTION__, UKismetMathLibrary::Round(divider), divider, rollTarget, StartRackRotation.Clamp().Roll);
	
	TargetRackRotation.Roll = rollTarget;
	InterpRackRotStartTimestamp = GetWorld()->GetAudioTimeSeconds();
	bInterpRackRotation = true;
	
}

void UPS_WeaponComponent::SightMeshRotation()
{
	//Smoothly rotate Sight Mesh
	if(bInterpRackRotation) 
	{
		float alpha = ((GetWorld()->GetAudioTimeSeconds() - InterpRackRotStartTimestamp) / RackRotDuration);
		alpha = FMath::Clamp(alpha,0.0f,1.0f);
		float curveAlpha = alpha;
		if(IsValid(RackRotCurve))
			curveAlpha = RackRotCurve->GetFloatValue(alpha);
				
		const FRotator newRotation = FMath::Lerp(StartRackRotation,TargetRackRotation, curveAlpha);
		SightMesh->SetRelativeRotation(newRotation);

		if(bDebugSightRack) UE_LOG(LogTemp, Log, TEXT("%S :: StartRackRotation %s, TargetRackRotation %s, alpha %f"),__FUNCTION__,*StartRackRotation.ToString(),*TargetRackRotation.ToString(), alpha);

		//Stop Rot
		if(alpha > 1 && !_bTurnRackTargetSetuped)
			bInterpRackRotation = false;
		
	}
}

void UPS_WeaponComponent::RackTick()
{
	if(!IsValid(_PlayerController)) return;

	const FVector2D inputValue = _PlayerController->GetLookInput().ClampAxes(-1.0f,1.0f);
	_SightLookInput = (_SightLookInput + inputValue).ClampAxes(-1.0f,1.0f);
	
}

#pragma region TurnRackTarget
//------------------

void UPS_WeaponComponent::SetupTurnRackTargetting()
{
	if(_bTurnRackTargetSetuped) return;
	
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(SightMesh) || !IsValid(GetWorld())) return;

	if(bDebugSightRack) UE_LOG(LogTemp, Error, TEXT("%S"),__FUNCTION__);

	_PlayerController->SetCanLook(false);
	
	//Trigger slowmo
	//_PlayerCharacter->GetSlowmoComponent()->OnTriggerSlowmo();

	//Setup work var
	StartRackRotation = SightMesh->GetRelativeRotation();
	InterpRackRotStartTimestamp = GetWorld()->GetAudioTimeSeconds();
	_bTurnRackTargetSetuped = true;

	//Reactive custom tick
	GetWorld()->GetTimerManager().UnPauseTimer(_RackTickTimerHandle);

	//Callback
	OnToggleTurnRackTargetEvent.Broadcast(true);
}

void UPS_WeaponComponent::StopTurnRackTargetting()
{
	if(!_bTurnRackTargetSetuped) return;

	if(bDebugSightRack) UE_LOG(LogTemp, Error, TEXT("%S"),__FUNCTION__);
	
	_PlayerController->SetCanLook(true);
	
	//Trigger slowmo
	//_PlayerCharacter->GetSlowmoComponent()->OnTriggerSlowmo();

	bInterpRackRotation = false;
	_bTurnRackTargetSetuped = false;

	//Stop custom tick
	GetWorld()->GetTimerManager().PauseTimer(_RackTickTimerHandle);

	//Callback
	OnToggleTurnRackTargetEvent.Broadcast(false);
}

void UPS_WeaponComponent::TurnRackTarget()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(_PlayerCharacter->GetFirstPersonCameraComponent())) return;
		
	//Determine dir by input world
	const UPS_PlayerCameraComponent* playerCam = _PlayerCharacter->GetFirstPersonCameraComponent();
	
	//Setup or stop if no slideable object was sighted
	if(!_bSightMeshIsInUse)
	{
		StopTurnRackTargetting();
		return;
	}else if(!_bTurnRackTargetSetuped)
	{
		SetupTurnRackTargetting();
	}
	
	FVector dir = playerCam->GetRightVector() * _SightLookInput.X + playerCam->GetUpVector() * _SightLookInput.Y * -1;
	dir.Normalize();

	if(dir.IsNearlyZero()) return;

	const FVector sightDir = SightMesh->GetRightVector();

	const float dotProdRight = playerCam->GetRightVector().Dot(dir);
	const float dotProdDown = (playerCam->GetUpVector() * -1).Dot(dir);
	
	const float angleToInputTargetLoc = UKismetMathLibrary::DegAcos(dotProdRight) * FMath::Sign(dotProdDown);
	const FVector muzzleDir = playerCam->GetRightVector() * FMath::Sign(dotProdRight);

	if(bDebugSightRack)
	{
		DrawDebugDirectionalArrow(GetWorld(), SightMesh->GetComponentLocation(), SightMesh->GetComponentLocation() + dir * 100, 10.0f,FColor::Yellow, false, 0.1, 10, 3);
		DrawDebugDirectionalArrow(GetWorld(), SightMesh->GetComponentLocation(), SightMesh->GetComponentLocation() + muzzleDir * 100, 10.0f,FColor::Orange, false, 0.1, 10, 3);
		DrawDebugDirectionalArrow(GetWorld(), SightMesh->GetComponentLocation(), SightMesh->GetComponentLocation() + sightDir * 100, 10.0f,FColor::Green, false, 0.1, 10, 3);
	}
	TargetRackRotation.Roll = angleToInputTargetLoc;
	
	//Active rot interp
	bInterpRackRotation = true;
}

//------------------
#pragma endregion TurnRackTarget

//------------------
#pragma endregion Rack

#pragma region Ray_Rack
//------------------
void UPS_WeaponComponent::AdaptSightMeshBound()
{
	if(!IsValid(SightMesh)) return;
	
	_bSightMeshIsInUse = IsValid(_CurrentSightedComponent) && !_PlayerCharacter->IsWeaponStow();
	SightMesh->SetVisibility(_bSightMeshIsInUse);

	if(!_bSightMeshIsInUse) return;

	//Setup variables
	UProceduralMeshComponent* procMeshComp = Cast<UProceduralMeshComponent>(_CurrentSightedComponent);
	double roll = 180.0f;
	double reminder;
	UKismetMathLibrary::FMod(SightMesh->GetComponentRotation().Roll, roll, reminder);
	const bool bRackIsHorizontal = (FMath::IsNearlyEqual(FMath::Abs(reminder), 0.0f, 5.0f) || FMath::IsNearlyEqual(FMath::Abs(reminder), 180.0f, 5.0f));

	//Get origin and extent
	FVector origin, extent;
	procMeshComp->GetLocalBounds().GetBox().GetCenterAndExtents(origin, extent);
	extent = procMeshComp->GetComponentRotation().RotateVector(extent * procMeshComp->GetComponentScale()); 
	origin = procMeshComp->GetComponentTransform().TransformPosition(origin);

	//Ratio to max dist
	float sightAjustementDist = (_SightHitResult.Distance / MaxFireDistance);/** 10.0f*/

	//Debug
	if(bDebugSightShader)
	{
		DrawDebugPoint(GetWorld(), origin + extent, 20.f, FColor::Yellow, false);
		DrawDebugLine(GetWorld(), origin, origin + extent, FColor::Yellow, false, 2, 10, 3);
		DrawDebugPoint(GetWorld(), origin - extent, 20.f, FColor::Orange, false);
		DrawDebugLine(GetWorld(), origin, origin - extent, FColor::Orange, false, 2, 10, 3);
	}
	
	//Determine placement on distance
	FVector2D origin2D, extent2D;
	origin2D = UKismetMathLibrary::Conv_VectorToVector2D(origin);
	extent2D = UKismetMathLibrary::Conv_VectorToVector2D(extent);

	const float maxScale = FMath::Abs(bRackIsHorizontal ? UKismetMathLibrary::SafeDivide(extent.Y, procMeshComp->GetComponentScale().Y) : UKismetMathLibrary::SafeDivide(extent.Z, procMeshComp->GetComponentScale().Z));
	const float distOriginToBorder =  UKismetMathLibrary::SafeDivide(UKismetMathLibrary::Distance2D(origin2D, extent2D), maxScale);
	const float distSigthBorder = UKismetMathLibrary::SafeDivide(UKismetMathLibrary::Distance2D(UKismetMathLibrary::Conv_VectorToVector2D(_SightHitResult.Location), origin2D + extent2D), maxScale);
	const float maxDistToBorder = UKismetMathLibrary::SafeDivide((bRackIsHorizontal ?  distOriginToBorder : origin.Z + extent.Z), maxScale);
	
	const float distToBorder =  FMath::Abs(bRackIsHorizontal ? (maxDistToBorder - FMath::Abs(distSigthBorder)) : (maxDistToBorder -  FMath::Abs(_SightHitResult.Location.Z)));

	//FVector scaleWeight = procMeshComp->GetComponentRotation().RotateVector(procMeshComp->GetLocalBounds().GetBox().GetExtent() * procMeshComp->GetComponentScale());
	//float maxScale = UKismetMathLibrary::NormalizeToRange(bRackIsHorizontal ? scaleWeight.Y : scaleWeight.Z, 0.0f,1.0f);
	const float sightAjustementBound = UKismetMathLibrary::MapRangeClamped(distToBorder, 0.0f, maxDistToBorder, MinSightRayMultiplicator, 1.0f);

	//UE_LOG(LogTemp, Error, TEXT("sightAjustementDist %f, bRackIsHorizontal %i, sightAjustementBound %f, maxScale %f,distToBorder %f, maxDistToBorder %f"),sightAjustementDist, bRackIsHorizontal, sightAjustementBound,maxScale,distToBorder, maxDistToBorder);

	FVector newScale = RackDefaultRelativeTransform.GetScale3D();
	newScale.X = newScale.X * sightAjustementDist;
	newScale.Y = sightAjustementBound;
	newScale.Z = 0.01f;

	FVector newLoc = RackDefaultRelativeTransform.GetLocation();
	newLoc.X = newLoc.X * sightAjustementDist;
	
	SightMesh->SetRelativeScale3D(newScale);
	SightMesh->SetRelativeLocation(newLoc);
	
	if(bDebugSightShader) UE_LOG(LogTemp, Error, TEXT("%S :: sightAjustementBound %f, sightAjustementDist %f, newScale %s, newLoc %s"),__FUNCTION__, sightAjustementBound, sightAjustementDist, *newScale.ToString(), *newLoc.ToString());
		
}
//------------------
#pragma endregion Ray_Rack

#pragma region Shader
//------------------

void UPS_WeaponComponent::SightShaderTick()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(GetWorld()) || _PlayerCharacter->IsWeaponStow())
	{
		ResetSightRackProperties();
		return;
	}

	//Determine Sight Hit
	const bool bUseHookStartForLaser = _PlayerCharacter->GetForceComponent()->IsPushing();
	const FVector start = bUseHookStartForLaser ? _PlayerCharacter->GetHookComponent()->GetHookThrower()->GetSocketLocation(SOCKET_HOOK) : GetMuzzlePosition();
	const FVector target = UPSFl::GetWorldPointInFrontOfCamera(_PlayerController, MaxFireDistance);
	
	const TArray<AActor*> actorsToIgnore = {_PlayerCharacter};
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), start, target, UEngineTypes::ConvertToTraceType(ECC_Slice),
		false, actorsToIgnore, EDrawDebugTrace::None, _SightHitResult, true);

	//Laser
	LaserTarget = UPSFl::GetScreenCenterWorldLocation(_PlayerController) + _PlayerCamera->GetForwardVector() * MaxFireDistance;
	SightTarget = UPSFl::GetScreenCenterWorldLocation(_PlayerController) + _PlayerCamera->GetForwardVector() * _SightHitResult.Distance;

	//On shoot Bump tick logic 
	SliceBump();

	UMeshComponent* sliceTarget = Cast<UMeshComponent>(_SightHitResult.GetComponent());
	if(_SightHitResult.bBlockingHit && IsValid(_SightHitResult.GetActor()))
	{
		if(!IsValid(sliceTarget)) return;
		
		if(bDebugSightShader)
		{
			DrawDebugBox(GetWorld(),(sliceTarget->GetComponentLocation() + sliceTarget->GetLocalBounds().Origin),sliceTarget->GetComponentRotation().RotateVector(sliceTarget->GetLocalBounds().BoxExtent * sliceTarget->GetComponentScale()), FColor::Yellow, false,-1 , 1 ,2);
		}

		//Adapt sightMesh scale to sighted object bound
		AdaptSightMeshBound();
		
		if(IsValid(_CurrentSightedComponent) && _CurrentSightedComponent == _SightHitResult.GetComponent())
			return;
		
		//Reset last material properties
		ResetSightRackProperties();
		if(!_SightHitResult.GetActor()->ActorHasTag(FName("Sliceable")) ) return;
		_CurrentSightedComponent = _SightHitResult.GetComponent();
		_CurrentSightedFace = _SightHitResult.FaceIndex;
		for (int i =0 ; i<_CurrentSightedComponent->GetNumMaterials(); i++)
		{
			//Setup slice rack shader material inst
			UMaterialInterface* newMaterialMaster = _CurrentSightedComponent->GetMaterial(i);
			if(!IsValid(newMaterialMaster))
			{
				newMaterialMaster = SliceableMaterial;
				UE_LOG(LogTemp, Error, TEXT("Invalid newMaterialMaster use SliceableMaterial"));
			}

			//Mat inst
			UMaterialInstanceDynamic* sightedMatInst = Cast<UMaterialInstanceDynamic>(newMaterialMaster);
			const bool bUseADynMatAsMasterMat = IsValid(sightedMatInst);
		
			//If already use an instance use i else create one
			if(!bUseADynMatAsMasterMat)
			{
				//Set new Object Mat instance
				sightedMatInst = UKismetMaterialLibrary::CreateDynamicMaterialInstance(GetWorld(),newMaterialMaster);
				if(!IsValid(sightedMatInst)) continue;
			}
			
			//Start display Slice Rack Ray
			sightedMatInst->SetScalarParameterValue(FName("bIsInUse"), true);
		
			//Add in queue
			_CurrentSightedMatInst.Insert(sightedMatInst,i);
			_CurrentSightedBaseMats.Insert(newMaterialMaster, i);
			if(!bUseADynMatAsMasterMat)_CurrentSightedComponent->SetMaterial(i, sightedMatInst);
		}

		//Start PostProcess Outline on faced sliced
		if(IsValid(_PlayerCamera))
		{
			_PlayerCamera->TriggerGlasses(_PlayerCharacter->IsGlassesActive(), false);
			_PlayerCamera->DisplayOutlineOnSightedComp(true);
		}
		
		//Setup Bump to Old params if effective
		ForceInitSliceBump();

		if(bDebugSightShader) UE_LOG(LogTemp, Log, TEXT("%S :: activate sight shader on %s"), __FUNCTION__, *_CurrentSightedComponent->GetName());
	}
	//If don't Lbock reset old mat properties
	else if(!_SightHitResult.bBlockingHit && IsValid(_CurrentSightedComponent))
	{
		//Reset SightRack display
		SightMesh->SetVisibility(false);

		//Reset sight shader
		ResetSightRackProperties();
	}

		
}

void UPS_WeaponComponent::ResetSightRackProperties()
{
	if(IsValid(_CurrentSightedComponent))
	{
		int i = 0;
		//Reset sighted material by their base
		for (UMaterialInterface* material : _CurrentSightedBaseMats)
		{
			if(!IsValid(material))
			{
				UE_LOG(LogTemp, Error, TEXT("%S :: material invalid"),__FUNCTION__);
				continue;
			}
			
			UMaterialInstanceDynamic* currentUsedMatInst = Cast<UMaterialInstanceDynamic>(_CurrentSightedComponent->GetMaterial(i));
			if(!IsValid(currentUsedMatInst)) continue;

			//Get Melt info
			float bIsMelting = 0.0f;
			currentUsedMatInst->GetScalarParameterValue(FName("bIsMelting"),bIsMelting);
			
			//If display in a currently melting face update Mat instance if sigthed element was Melting
			UMaterialInstanceDynamic* currentSightedBaseMatInst = Cast<UMaterialInstanceDynamic>(material);
			const bool bUseADynMatAsMasterMat = IsValid(currentSightedBaseMatInst);
						
			if(bUseADynMatAsMasterMat)
			{
				//Stop display Slice Rack Ray
				currentSightedBaseMatInst->SetScalarParameterValue(FName("bIsInUse"), false);

				//Melt
				if(bIsMelting)UpdateMeltingParams(currentUsedMatInst, currentSightedBaseMatInst);
			}			
			if(!bUseADynMatAsMasterMat)_CurrentSightedComponent->SetMaterial(i, material);

			//Debug
			if(bDebugSightShader) UE_LOG(LogTemp, Warning, TEXT("%S :: reset %s with %s material"), __FUNCTION__, *_CurrentSightedComponent->GetName(), *material->GetName());

			//Stop PostProcess Outline on faced sliced
			_PlayerCharacter->GetFirstPersonCameraComponent()->TriggerGlasses(_PlayerCharacter->IsGlassesActive(), false);

			if(IsValid(_PlayerCamera))
				_PlayerCamera->DisplayOutlineOnSightedComp(false);

			//Increment index
			i++;
			
		}
		
		//Reset variables
		_CurrentSightedComponent = nullptr;
		_bSightMeshIsInUse = false;
		_CurrentSightedMatInst.Empty();
		_CurrentSightedBaseMats.Empty();

		//Unactive sight
		SightMesh->SetVisibility(false);
		
	}
}

void UPS_WeaponComponent::ForceInitSliceBump()
{
	for (auto currentSightedMatInstElement : _CurrentSightedMatInst)
	{
		if(IsValid(currentSightedMatInstElement))
		{
			currentSightedMatInstElement->SetScalarParameterValue(FName("bIsHoldingFire"), bIsHoldingFire ? -1 : 1);
			currentSightedMatInstElement->SetScalarParameterValue(FName("SliceBumpAlpha"), CurrentCurveAlpha);
		}
	}
	
}

void UPS_WeaponComponent::SetupSliceBump()
{
	if(IsValid(_CurrentSightedComponent) && IsValid(GetWorld()))
	{
		if(bDebugSightShader) UE_LOG(LogTemp, Warning, TEXT("%S :: Bump %s"), __FUNCTION__, bIsHoldingFire ? TEXT("IN") : TEXT("OUT"));
		
		StartSliceBumpTimestamp = GetWorld()->GetTimeSeconds();
		bSliceBumping = true;

		for (UMaterialInstanceDynamic* currentSightedMatInstElement : _CurrentSightedMatInst)
		{
			if(IsValid(currentSightedMatInstElement))
				currentSightedMatInstElement->SetScalarParameterValue(FName("bIsHoldingFire"), bIsHoldingFire ? -1 : 1);
		}
		
	}
}

void UPS_WeaponComponent::SliceBump()
{
	if(!IsValid(_CurrentSightedComponent) || !IsValid(GetWorld())) return;

	if(bSliceBumping)
	{
		const float alpha = (GetWorld()->GetTimeSeconds() - StartSliceBumpTimestamp) / SliceBumpDuration;

		CurrentCurveAlpha = alpha;
		if(IsValid(SliceBumpCurve))
		{
			CurrentCurveAlpha = SliceBumpCurve->GetFloatValue(alpha);
		}

		
		if(bDebugSightSliceBump) UE_LOG(LogTemp, Log, TEXT("%S :: curveAlpha %f "), __FUNCTION__, CurrentCurveAlpha);
		for (UMaterialInstanceDynamic* currentSightedMatInstElement : _CurrentSightedMatInst)
		{
			if(!IsValid(currentSightedMatInstElement)) continue;
			currentSightedMatInstElement->SetScalarParameterValue(FName("SliceBumpAlpha"), CurrentCurveAlpha);
		}

		if(alpha >= 1)
			bSliceBumping = false;
	}
			
}

//------------------
#pragma endregion Shader

//------------------
#pragma endregion Sight

#pragma region Slice
//------------------

UMaterialInstanceDynamic* UPS_WeaponComponent::SetupMeltingMat(const UProceduralMeshComponent* const procMesh)
{
	UMaterialInstanceDynamic* matInst  = UKismetMaterialLibrary::CreateDynamicMaterialInstance(GetWorld(), SliceableMaterial);
	
	if(!IsValid(matInst) || !IsValid(GetWorld())) return nullptr;

	matInst->SetScalarParameterValue(FName("StartTime"), GetWorld()->GetTimeSeconds());
	matInst->SetScalarParameterValue(FName("Duration"), MeltingLifeTime);
		
	FLinearColor baseMaterialColor;
	procMesh->GetMaterial(0)->GetVectorParameterValue(FName("Base Color"), baseMaterialColor);
	matInst->SetVectorParameterValue(FName("Base Color"), baseMaterialColor);
	
	matInst->SetScalarParameterValue(FName("bIsMelting"), true);

	_HalfSectionMatInst.AddUnique(matInst);
	
	return matInst;
}

void UPS_WeaponComponent::UpdateMeshTangents(UProceduralMeshComponent* const procMesh, const int32 sectionIndex)
{
	TArray<FVector> outVerticles;
	TArray<int32> outTriangles;
	TArray<FVector> outNormals;
	TArray<FVector2D> outUV;
	TArray<FColor> vertexColors;
	TArray<FProcMeshTangent> outTangent;
	
	UKismetProceduralMeshLibrary::GetSectionFromProceduralMesh(procMesh,sectionIndex, outVerticles, outTriangles, outNormals, outUV, outTangent);
	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(outVerticles,outTriangles, outUV, outNormals, outTangent);
	
	procMesh->UpdateMeshSection(sectionIndex, outVerticles, outNormals, outUV, vertexColors, outTangent);
}


void UPS_WeaponComponent::UpdateMeltingParams(const UMaterialInstanceDynamic* const sightedMatInst,UMaterialInstanceDynamic* const matInstObject)
{
	float bIsMelting, startTime;
	sightedMatInst->GetScalarParameterValue(FName("bIsMelting"), bIsMelting);
	sightedMatInst->GetScalarParameterValue(FName("StartTime"), startTime);
			
	if(bIsMelting)
	{
		matInstObject->SetScalarParameterValue(FName("bIsMelting"), bIsMelting);
		matInstObject->SetScalarParameterValue(FName("StartTime"), startTime);
		matInstObject->SetScalarParameterValue(FName("Duration"), MeltingLifeTime);
	}
}


//------------------
#pragma endregion Slice
	


