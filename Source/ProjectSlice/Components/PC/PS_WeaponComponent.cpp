// Copyright Epic Games, Inc. All Rights Reserved.


#include "PS_WeaponComponent.h"

#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputSubsystems.h"
#include "KismetProceduralMeshLibrary.h"
#include "PS_HookComponent.h"
#include "PS_PlayerCameraComponent.h"
#include "Engine/DamageEvents.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Character/PC/PS_Character.h"
#include "ProjectSlice/Character/PC/PS_PlayerController.h"
#include "ProjectSlice/Data/PS_Constants.h"
#include "ProjectSlice/Data/PS_GlobalType.h"
#include "ProjectSlice/Data/PS_TraceChannels.h"
#include "ProjectSlice/FunctionLibrary/PSFL_CustomProcMesh.h"
#include "ProjectSlice/FunctionLibrary/PSFl.h"
#include "ProjectSlice/FunctionLibrary/PSFL_GeometryScript.h"

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
	return GetSocketLocation(SOCKET_MUZZLE);
}

FVector UPS_WeaponComponent::GetLaserPosition()
{
	return GetSocketLocation(SOCKET_LASER);
}

void UPS_WeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	//Setup default value
	RackDefaultRelativeTransform = SightMesh->GetRelativeTransform();
	_TargetRackRoll = RackDefaultRelativeTransform.Rotator().Roll;

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
	SightTick();

	SightMeshRotation();
		
}

void UPS_WeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
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
	targetPlayerCharacter->GetHookComponent()->InitHookComponent();

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
	
	if (!_SightHitResult.bBlockingHit || !IsValid(_SightHitResult.GetActor()) || !IsValid(_SightHitResult.GetComponent()) || !IsValid(_SightHitResult.GetComponent()->GetOwner())) return;

	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__);
	
	//Check if it's a destructible
	 if(_SightHitResult.GetComponent()->IsA(UGeometryCollectionComponent::StaticClass()))
	 {
	 	GenerateImpactField(_SightHitResult);
	 	return;
	 }
	
	//Init var
	UProceduralMeshComponent* parentProcMeshComponent = Cast<UProceduralMeshComponent>(_SightHitResult.GetComponent());
	UPS_SlicedComponent* currentSlicedComponent = Cast<UPS_SlicedComponent>(_SightHitResult.GetActor()->GetComponentByClass(UPS_SlicedComponent::StaticClass()));

	//Check object validity
	if (!IsValid(parentProcMeshComponent) || !IsValid(currentSlicedComponent)) return;
	
	//Setup material
	ResetSightRackShaderProperties();
	UMaterialInstanceDynamic* matInst = SetupMeltingMat(parentProcMeshComponent);

	//Slice mesh setup
	_LastSliceOutput = FSCustomSliceOutput();
	_LastSliceOutput.bDebug = bDebugSlice;
	
	UPS_SlicedComponent* outHalfComponent;
	FVector sliceLocation = _SightTarget;
	FVector sliceDir = SightMesh->GetUpVector();
	sliceDir.Normalize();

	//Slice
	UPSFL_CustomProcMesh::SliceProcMesh(parentProcMeshComponent, sliceLocation,
		sliceDir, true, SlicedComponent, outHalfComponent, _LastSliceOutput,
		IsValid(matInst) ? EProcMeshSliceCapOption::CreateNewSectionForCap : EProcMeshSliceCapOption::UseLastSectionForCap,
		matInst, true);
	if(!IsValid(outHalfComponent)) return;
	
	// Ensure collision is generated for both meshes	
	// for (int32 newProcMeshSection = 0; newProcMeshSection < outHalfComponent->GetNumSections(); newProcMeshSection++)
	// {
	// 	UpdateMeshTangents(outHalfComponent, newProcMeshSection);
	// }
	//
	// for (int32 currentProcMeshSection = 0; currentProcMeshSection < parentProcMeshComponent->GetNumSections(); currentProcMeshSection++)
	// {
	// 	UpdateMeshTangents(parentProcMeshComponent, currentProcMeshSection);
	// }

	//Debug trace
	if(bDebugSlice)
	{
		DrawDebugLine(GetWorld(), sliceLocation, sliceLocation + sliceDir * 500, FColor::Magenta, false, 2, 10, 3);
		DrawDebugLine(GetWorld(),GetMuzzlePosition(), GetMuzzlePosition() +  SightMesh->GetUpVector() * 500, FColor::Yellow, false, 2, 10, 3);
		DrawDebugLine(GetWorld(),GetMuzzlePosition() + SightMesh->GetUpVector() * 500 , sliceLocation + sliceDir  * 500, FColor::Green, false, 2, 10, 3);
	}

	//Register and instanciate
	outHalfComponent->RegisterComponent();
	outHalfComponent->InitComponent();
	
	if(IsValid(parentProcMeshComponent->GetOwner()))
	{
		//Cast<UMeshComponent>(CurrentFireHitResult.GetActor()->GetRootComponent())->SetCollisionResponseToChannel(ECC_Rope, ECR_Ignore);
		parentProcMeshComponent->GetOwner()->AddInstanceComponent(outHalfComponent);
	}

	//Bound Update
	// parentProcMeshComponent->UpdateBounds();
	// outHalfComponent->UpdateBounds();

	//Physics
	UPhysicalMaterial* physMat = currentSlicedComponent->BodyInstance.GetSimplePhysicalMaterial();
	outHalfComponent->SetPhysMaterialOverride(physMat);	
	outHalfComponent->SetSimulatePhysics(true);
	
	parentProcMeshComponent->SetSimulatePhysics(true);
	
	//Impulse
	if(ActivateImpulseOnSlice)
	{
		FDamageEvent damageEvent = FDamageEvent();
		outHalfComponent->ReceiveComponentDamage(10000,damageEvent,_PlayerController,_PlayerCharacter);
		outHalfComponent->AddImpulse((outHalfComponent->GetUpVector() * -1) * outHalfComponent->GetMass() * 1000, NAME_None, false);
		
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

	//Callback
	OnFireEvent.Broadcast();
}

//------------------

#pragma endregion Fire

#pragma region Destruction
//------------------

#pragma region CanGenerateImpactField
//------------------

void UPS_WeaponComponent::GenerateImpactField(const FHitResult& targetHit, const FVector extent)
{
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(GetWorld()) || !targetHit.bBlockingHit) return;

	if(!IsValid(FieldSystemActor.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("%S :: SpawnActor failed because no class was specified"),__FUNCTION__);
		return;
	}
	
	//Spawn param
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = _PlayerCharacter;
	SpawnInfo.Instigator = _PlayerCharacter;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	//Determine base loc && rot
	FVector loc =  targetHit.Location + UKismetMathLibrary::GetDirectionUnitVector(targetHit.TraceStart, targetHit.Location) * 100;
	FRotator rot = UKismetMathLibrary::FindLookAtRotation(targetHit.TraceStart, targetHit.Location);
	rot.Roll = -_TargetRackRoll;

	DrawDebugLine(GetWorld(), loc, loc + rot.Vector() * 500, FColor::Yellow, false, 2, 10, 3);
	
	_ImpactField = GetWorld()->SpawnActor<APS_FieldSystemActor>(FieldSystemActor.Get(), loc, rot, SpawnInfo);
	if(!IsValid(_ImpactField)) return;

	//Rotatation local for plane
	_ImpactField->AddActorLocalRotation(FRotator(-90, 0, 0));
	
	//Debug
	if(bDebugChaos) UE_LOG(LogTemp, Log, TEXT("%S :: success %i"), __FUNCTION__, IsValid(_ImpactField));

	//Callback
	OnSliceChaosFieldGeneratedEvent.Broadcast(_ImpactField);
}

//------------------

#pragma endregion CanGenerateImpactField

//------------------
#pragma endregion Destruction

#pragma region Sight
//------------------

void UPS_WeaponComponent::SightTick()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(GetWorld()) || !IsValid(_PlayerCharacter->GetForceComponent()) || !IsValid(_PlayerCharacter->GetHookComponent()))
	{
		ResetSightRackShaderProperties();
		return;
	}

	//Determine Sight Hit
	_bUseHookStartForLaser = _PlayerCharacter->GetForceComponent()->IsPushing();
	_SightStart = _bUseHookStartForLaser ? _PlayerCharacter->GetHookComponent()->GetHookThrower()->GetSocketLocation(SOCKET_HOOK) : GetMuzzlePosition();
	const FVector targetSight = UPSFl::GetWorldPointInFrontOfCamera(_PlayerController, MaxFireDistance);
	
	const TArray<AActor*> actorsToIgnore = {_PlayerCharacter};
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), _SightStart, targetSight, UEngineTypes::ConvertToTraceType(ECC_Slice),
		false, actorsToIgnore, bDebugSightRack ? EDrawDebugTrace::ForDuration :  EDrawDebugTrace::None, _SightHitResult, true, FLinearColor::Red, FLinearColor::Green,0.1f);
	
	//Determine Laser Hit
	const FVector targetLaser = UPSFl::GetScreenCenterWorldLocation(_PlayerController) + _PlayerCamera->GetForwardVector() * (_SightHitResult.bBlockingHit ? FMath::Abs(_SightHitResult.Distance) + 250.0f : MaxFireDistance);
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), GetLaserPosition(), targetLaser, UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false, actorsToIgnore, bDebugSightRack ? EDrawDebugTrace::ForDuration :  EDrawDebugTrace::None, _LaserHitResult, true,  FLinearColor::Red, FLinearColor::Green,0.1f);

	//Laser && Sight Target
	_LaserTarget = _LaserHitResult.bBlockingHit ? _LaserHitResult.ImpactPoint : targetLaser;
	_SightTarget = _SightHitResult.bBlockingHit ? _SightHitResult.ImpactPoint : targetSight; 
	
	//Stop if in unauthorized slicing situation
	if(_PlayerCharacter->IsWeaponStow() || _PlayerCharacter->GetForceComponent()->IsPushLoading())
	{
		ResetSightRackShaderProperties();
		return;
	}

	//On shoot Bump tick logic 
	SliceBump();

	//Update laser color if necessary
	UpdateLaserColor();

	//Adapt SightMesh scale
	AdaptSightMeshBound();

	//Sight Shader
	UpdateSightRackShader();
	
	//Setup last var
	_LastSightTarget = _SightTarget;
}

#pragma region Rack
//------------------

void UPS_WeaponComponent::UpdateLaserColor()
{
	EPointedObjectType newObjectType = EPointedObjectType::DEFAULT;
	if (_SightHitResult.bBlockingHit && IsValid(_SightHitResult.GetComponent()))
	{
		if(_SightHitResult.GetComponent()->IsA(UGeometryCollectionComponent::StaticClass())) newObjectType = EPointedObjectType::CHAOS;
		else if (_SightHitResult.GetComponent()->IsA(UPS_SlicedComponent::StaticClass())) newObjectType = EPointedObjectType::SLICEABLE;
	}
	
	if (newObjectType != _SightedObjectType)
	{
		_SightedObjectType = newObjectType;
		OnSwitchLaserMatEvent.Broadcast(_SightedObjectType, true);
	}
}

void UPS_WeaponComponent::TurnRack()
{
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(SightMesh)) return;
	
	_StartRackRoll = SightMesh->GetRelativeRotation().Clamp().Roll;
	
	const float divider = UKismetMathLibrary::SafeDivide(_StartRackRoll, 90.0f) + 1.0f;
	const float rollTarget = 90.0f * UKismetMathLibrary::FTrunc(divider);
	
	if(bDebugSightRack) UE_LOG(LogTemp, Log, TEXT("%S :: DividerRounded %i, Divider %f, RollTarget %f, Currentroll %f"), __FUNCTION__, UKismetMathLibrary::Round(divider), divider, rollTarget, _StartRackRoll);
	
	_TargetRackRoll = rollTarget;
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
				
		FRotator newRotation = SightMesh->GetRelativeRotation().Clamp();
		const float newRoll= FMath::Lerp(_StartRackRoll,_TargetRackRoll, curveAlpha);
		newRotation.Roll = newRoll;
		
		SightMesh->SetRelativeRotation(newRotation);

		if(bDebugSightRack) UE_LOG(LogTemp, Log, TEXT("%S :: StartRackRoll %f, TargetRackRoll %f, alpha %f"),__FUNCTION__,_StartRackRoll,_TargetRackRoll, alpha);

		//Stop Rot
		if(alpha >= 1 && !_bTurnRackTargetSetuped)
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
	_StartRackRoll = SightMesh->GetRelativeRotation().Clamp().Roll;
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
	if(!_bTurnRackTargetSetuped)
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

	//TargetRackRotation = UKismetMathLibrary::FindLookAtRotation(GetMuzzlePosition(), _LaserTarget);
	_TargetRackRoll = angleToInputTargetLoc;
	
	//Active rot interp
	bInterpRackRotation = true;
}

//------------------
#pragma endregion TurnRackTarget

//------------------
#pragma endregion Rack

#pragma region Adaptation
//------------------

void UPS_WeaponComponent::AdaptSightMeshBound()
{
	if (!IsValid(_PlayerCamera) || !IsValid(_PlayerCharacter) || !IsValid(SightMesh)) return;
	
	if (_LastSightTarget.Equals(_SightTarget, 1.0f)) return;

	//Init work var
	FVector MuzzleLoc = GetMuzzlePosition();
	FVector ViewDir = _PlayerCamera->GetForwardVector();
	
	UProceduralMeshComponent* procComp = Cast<UProceduralMeshComponent>(_SightHitResult.GetComponent());
	UGeometryCollectionComponent* chaosComp = Cast<UGeometryCollectionComponent>(_SightHitResult.GetComponent());
		
	//--Default adaptation--
	if (!_SightHitResult.bBlockingHit
		|| !IsValid(_SightHitResult.GetComponent())
		|| (IsValid(_SightHitResult.GetComponent()) && !IsValid(procComp) && !IsValid(chaosComp)))
	{
		//Override scale 
		SightMesh->SetWorldScale3D(DefaultAdaptationScale);

		//Determine Rotation
		// FRotator LookRot = UKismetMathLibrary::FindLookAtRotation(MuzzleLoc,_LaserTarget);
		// LookRot.Roll = SightMesh->GetComponentRotation().Roll;
		FRotator rot = FRotator::ZeroRotator;
		rot.Roll = SightMesh->GetRelativeRotation().Roll;
		SightMesh->SetRelativeRotation(rot);

		if (bDebugRackBoundAdaptation) UE_LOG(LogTemp, Log, TEXT("%S :: Default"), __FUNCTION__);
		
		return;
	}

	//--Chaos adaptation--
	if(IsValid(chaosComp))
	{
		FRotator rot = FRotator::ZeroRotator;
		rot.Roll = SightMesh->GetRelativeRotation().Roll;
		SightMesh->SetRelativeRotation(rot);
		
		SightMesh->SetWorldScale3D(FVector(1.0f, 1.0f, 1.0f));
		
		if (bDebugRackBoundAdaptation) UE_LOG(LogTemp, Log, TEXT("%S :: Chaos"), __FUNCTION__);
		
		return;
	}
	
	//--Geometry adaptation (Sliceable)--
	if (!IsValid(procComp)) return;
	AdaptToProjectedHull(MuzzleLoc, ViewDir, procComp);
	
	
}

void UPS_WeaponComponent::AdaptToProjectedHull(const FVector& MuzzleLoc, const FVector& ViewDir, UMeshComponent* meshComp)
{
	FHullWidthOutData OutDatas = FHullWidthOutData();
	double Width = UPSFL_GeometryScript::ComputeProjectedHullWidth(
		meshComp,
		ViewDir,
		_SightHitResult.ImpactPoint,
		OutDatas,
		bDebugRackBoundAdaptation
	);
	//TODO :: Temp Fix 
	Width = Width * ProjectedAdaptationWeigth.X;

	// Project muzzle and center into same 2D frame
	FVector2d ProjMuzzle = FVector2d(OutDatas.OutProjectionFrame.ToPlane(MuzzleLoc));
	FVector2d ProjCenter = FVector2d(OutDatas.OutProjectionFrame.ToPlane(OutDatas.OutCenter3D));

	float Length = FVector2d::Distance(ProjMuzzle, ProjCenter);
	Length = Length * ProjectedAdaptationWeigth.Y;

	if (!FMath::IsFinite(Width) || !FMath::IsFinite(Length) || Length < 1.f || !SightMesh->GetStaticMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SightMesh] Invalid width or length — skipping scale."));
		return;
	}

	//Adpat Mesh Scale
	FVector MeshExtent = SightMesh->GetStaticMesh()->GetBounds().BoxExtent;
	if (!MeshExtent.IsNearlyZero())
	{
		float WidthScale = Width / (MeshExtent.Y * 2.f);  // full width
		float LengthScale = Length / MeshExtent.X;

		FVector NewScale = FVector(LengthScale, WidthScale, 1.f);
		const bool newScaleIsFinite = FMath::IsFinite(NewScale.X) && FMath::IsFinite(NewScale.Y) && FMath::IsFinite(NewScale.Z);

		if (!newScaleIsFinite || NewScale.ContainsNaN())
		{
			UE_LOG(LogTemp, Warning, TEXT("[SightMesh] Non-finite scale detected. Skipping."));
			return;
		}

		SightMesh->SetWorldScale3D(NewScale);
	}
	
	// Compensation de visée (triangle reste centré même si visée excentrée)
	FRotator LookRot = UPSFL_GeometryScript::ComputeAdjustedAimLookAt(
		MuzzleLoc,
		OutDatas.OutCenter3D,
		_SightHitResult.ImpactPoint,
		OutDatas.OutProjectionFrame,
		1.0f // ← si tu veux corriger à 100%
	);

	LookRot.Roll = SightMesh->GetComponentRotation().Roll; // garde Roll intact pr TurnRack()
	SightMesh->SetWorldRotation(LookRot);

	//Debug
	if (bDebugRackBoundAdaptation)
	{
		UE_LOG(LogTemp, Log, TEXT("%S :: Geometry (2D projected), Width %f, Length %f"), __FUNCTION__, Width, Length);
	}
}

//------------------
#pragma endregion Adaptation

#pragma region Shader
//------------------

void UPS_WeaponComponent::UpdateSightRackShader()
{
	UMeshComponent* sliceTarget = Cast<UMeshComponent>(_SightHitResult.GetComponent());
	if(_SightHitResult.bBlockingHit && IsValid(_SightHitResult.GetActor()))
	{
		if(!IsValid(sliceTarget)) return;
				
		if(bDebugSightShader)
		{
			DrawDebugBox(GetWorld(),(sliceTarget->GetComponentLocation() + sliceTarget->GetLocalBounds().Origin),sliceTarget->GetComponentRotation().RotateVector(sliceTarget->GetLocalBounds().BoxExtent * sliceTarget->GetComponentScale()), FColor::Yellow, false,-1 , 1 ,2);
		}
		
		if(IsValid(_CurrentSightedComponent) && _CurrentSightedComponent == _SightHitResult.GetComponent()) return;
		
		//Reset last material properties
		ResetSightRackShaderProperties();
		if(!_SightHitResult.GetComponent()->IsA(UProceduralMeshComponent::StaticClass())) return;
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
	//If don't block reset old mat properties
	else if(!_SightHitResult.bBlockingHit && IsValid(_CurrentSightedComponent))
	{
		//Reset sight shader
		ResetSightRackShaderProperties();
	}
	return;
}

void UPS_WeaponComponent::ResetSightRackShaderProperties()
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
		//SightMesh->SetVisibility(false);
		
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
	


