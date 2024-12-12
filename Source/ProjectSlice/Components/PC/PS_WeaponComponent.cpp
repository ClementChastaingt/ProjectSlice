// Copyright Epic Games, Inc. All Rights Reserved.


#include "PS_WeaponComponent.h"

#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
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
#include "ProjectSlice/Data/PS_TraceChannels.h"
#include "ProjectSlice/FunctionLibrary/PSCustomProcMeshLibrary.h"

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
		Subsystem->RemoveMappingContext(_PlayerController->GetFireMappingContext());
}

void UPS_WeaponComponent::AttachWeapon(AProjectSliceCharacter* Target_PlayerCharacter)
{
	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__);
	// Check that the _PlayerCharacter is valid, and has no rifle yet
	if (!IsValid(Target_PlayerCharacter)
		|| Target_PlayerCharacter->GetHasRifle()
		|| !IsValid(Target_PlayerCharacter->GetHookComponent())
		|| !IsValid(Target_PlayerCharacter->GetMesh()))return;
	
	// Attach the weapon to the First Person _PlayerCharacter
	this->SetupAttachment(Target_PlayerCharacter->GetMesh(), (TEXT("GripPoint")));

	// Attach Sight to Weapon
	SightMesh->SetupAttachment(this,FName("Muzzle"));
		
	// switch bHasRifle so the animation blueprint can switch to another animation set
	Target_PlayerCharacter->SetHasRifle(true);

	// Link Hook to Weapon
	_HookComponent = Target_PlayerCharacter->GetHookComponent();
	// _HookComponent->SetupAttachment(this,FName("HookAttach"));
	_HookComponent->OnAttachWeapon();
}


void UPS_WeaponComponent::InitWeapon(AProjectSliceCharacter* Target_PlayerCharacter)
{
	_PlayerCharacter = Target_PlayerCharacter;
	_PlayerController = Cast<AProjectSlicePlayerController>(Target_PlayerCharacter->GetController());
	
	if(!IsValid(_PlayerController)) return;
		
	//----Input----
	// Set up action bindings
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
		_PlayerController->GetLocalPlayer()))
	{
		// Set the priority of the mapping to 1, so that it overrides the Jump action with the Fire action when using touch input
		Subsystem->AddMappingContext(_PlayerController->GetFireMappingContext(), 1);
	}

	SetupWeaponInputComponent();

	OnWeaponInit.Broadcast();
}

#pragma region Input
//__________________________________________________

void UPS_WeaponComponent::SetupWeaponInputComponent()
{
	if(!IsValid(_PlayerController))
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("SetupPlayerInputComponent failde PlayerController is invalid"));
		return;
	}
	
	//BindAction
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(_PlayerController->InputComponent))
	{
		// Fire
		EnhancedInputComponent->BindAction(_PlayerController->GetFireAction(), ETriggerEvent::Triggered, this, &UPS_WeaponComponent::FireTriggered);

		// Rotate Rack
		EnhancedInputComponent->BindAction(_PlayerController->GetTurnRackAction(), ETriggerEvent::Triggered, this, &UPS_WeaponComponent::TurnRack);

		//Hook Launch
		EnhancedInputComponent->BindAction(_PlayerController->GetHookAction(), ETriggerEvent::Triggered, this, &UPS_WeaponComponent::HookObject);

		//Winder Launch
		EnhancedInputComponent->BindAction(_PlayerController->GetWinderAction(), ETriggerEvent::Triggered, this, &UPS_WeaponComponent::WindeHook);
		EnhancedInputComponent->BindAction(_PlayerController->GetWinderAction(), ETriggerEvent::Completed, this, &UPS_WeaponComponent::WindeHook);				
		
	}
}

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

void UPS_WeaponComponent::Fire()
{
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(GetWorld())) return;

	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__);
	
	//Trace config
	const TArray<AActor*> ActorsToIgnore{_PlayerCharacter};
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), SightMesh->GetComponentLocation(),
	                                      SightMesh->GetComponentLocation() + SightMesh->GetForwardVector() * MaxFireDistance,
	                                      UEngineTypes::ConvertToTraceType(ECC_Slice), false, ActorsToIgnore,
	                                        true ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, CurrentFireHitResult, true);

	if (!CurrentFireHitResult.bBlockingHit || !IsValid(CurrentFireHitResult.GetComponent()->GetOwner())) return;
	
	//Cut ProceduralMesh
	UProceduralMeshComponent* parentProcMeshComponent = Cast<UProceduralMeshComponent>(CurrentFireHitResult.GetComponent());
	UPS_SlicedComponent* currentSlicedComponent = Cast<UPS_SlicedComponent>(CurrentFireHitResult.GetActor()->GetComponentByClass(UPS_SlicedComponent::StaticClass()));

	if (!IsValid(parentProcMeshComponent) || !IsValid(currentSlicedComponent)) return;
	
	//Setup material
	ResetSightRackProperties();
	UMaterialInstanceDynamic* matInst = SetupMeltingMat(parentProcMeshComponent);

	//Slice mesh
	UProceduralMeshComponent* outHalfComponent;
	FVector sliceLocation = CurrentFireHitResult.Location;
	FVector sliceDir = SightMesh->GetUpVector();
	// UMeshComponent* sliceTarget = Cast<UMeshComponent>(CurrentFireHitResult.GetComponent());
	// FVector sliceDir = SightMesh->GetUpVector() / (sliceTarget->GetLocalBounds().BoxExtent * sliceTarget->GetComponentScale());
	sliceDir.Normalize();
	
	//TODO :: replace by EProcMeshSliceCapOption::CreateNewSectionForCap for reactivate melting mat
	UPSCustomProcMeshLibrary::SliceProcMesh(parentProcMeshComponent, sliceLocation,
		sliceDir, true,
		outHalfComponent, _sliceOutput,
		IsValid(matInst) ? EProcMeshSliceCapOption::CreateNewSectionForCap : EProcMeshSliceCapOption::UseLastSectionForCap,
		matInst);
	if(!IsValid(outHalfComponent)) return;

	//Launch melting reset timer
	FTimerHandle currentMeltingHandle;
	FTimerDelegate  currentMeltingTimerDelegate;
	
	currentMeltingTimerDelegate.BindUFunction(this, FName("ResetMeltingMat"), parentProcMeshComponent, outHalfComponent, _sliceOutput);
	GetWorld()->GetTimerManager().SetTimer(currentMeltingHandle, currentMeltingTimerDelegate, MeltingLifeTime, false);

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
		DrawDebugLine(GetWorld(), CurrentFireHitResult.Location,  CurrentFireHitResult.Location + sliceDir * 500, FColor::Magenta, false, 2, 10, 3);
		DrawDebugLine(GetWorld(), SightMesh->GetComponentLocation(),  SightMesh->GetComponentLocation() +  SightMesh->GetUpVector() * 500, FColor::Yellow, false, 2, 10, 3);
		DrawDebugLine(GetWorld(), SightMesh->GetComponentLocation() + SightMesh->GetUpVector() * 500 , CurrentFireHitResult.Location + sliceDir  * 500, FColor::Green, false, 2, 10, 3);
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
	if(!IsValid(_PlayerCharacter) || !IsValid(GetWorld()) || _PlayerCharacter->IsIsWeaponStow()) return;

	const FVector start = GetSightMeshComponent()->GetComponentLocation();
	const FVector target = GetSightMeshComponent()->GetComponentLocation() + GetSightMeshComponent()->GetForwardVector() * MaxFireDistance;
		
	FHitResult outHit;
	const TArray<AActor*> actorsToIgnore = {_PlayerCharacter};
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), start, target, UEngineTypes::ConvertToTraceType(ECC_Slice),
		false, actorsToIgnore, EDrawDebugTrace::None, outHit, true);

	//TODO:: Change by laser VFX
	DrawDebugLine(GetWorld(), start, target, FColor::Red, false);
	
	//On shoot Bump tick logic 
	SliceBump();

	UMeshComponent* sliceTarget = Cast<UMeshComponent>(outHit.GetComponent());
	if(outHit.bBlockingHit && IsValid(outHit.GetActor()))
	{
		if(!IsValid(sliceTarget)) return;
		
		if(bDebugSightShader)
		{
			DrawDebugBox(GetWorld(),(sliceTarget->GetComponentLocation() + sliceTarget->GetLocalBounds().Origin),sliceTarget->GetComponentRotation().RotateVector(sliceTarget->GetLocalBounds().BoxExtent * sliceTarget->GetComponentScale()), FColor::Yellow, false,-1 , 1 ,2);
		}
		
		if(IsValid(_CurrentSightedComponent) && _CurrentSightedComponent == outHit.GetComponent())
			return;
		
		//Reset last material properties
		ResetSightRackProperties();
		if(!outHit.GetActor()->ActorHasTag(FName("Sliceable")) ) return;
		_CurrentSightedComponent = outHit.GetComponent();
		_CurrentSightedFace = outHit.FaceIndex;
		for (int i =0 ; i<_CurrentSightedComponent->GetNumMaterials(); i++)
		{
			//Setup slice rack shader material inst
			UMaterialInterface* newMaterialMaster;
			
			UMaterialInstanceDynamic* sightedMatInst = Cast<UMaterialInstanceDynamic>(_CurrentSightedComponent->GetMaterial(i));
			const bool bUseADynMatAsMasterMat = IsValid(sightedMatInst);

			//TODO :: When use more complex material reuse that
			// newMaterialMaster = bUseADynMatAsMasterMat ? sightedMatInst->GetMaterial() : _CurrentSightedComponent->GetMaterial(i);
			//newMaterialMaster = bUseADynMatAsMasterMat ? _HalfSectionMatInst[i]->GetMaterial() : _CurrentSightedComponent->GetMaterial(i);
			//newMaterialMaster = bUseADynMatAsMasterMat ? SliceableMaterial : _CurrentSightedComponent->GetMaterial(i);
			
			//
			// if(!IsValid(newMaterialMaster))
			// {
			// 	newMaterialMaster = SliceableMaterial;
			// 	UE_LOG(LogTemp, Error, TEXT("Invalid newMaterialMaster use SliceableMaterial"));
			// }
			//
			newMaterialMaster = SliceableMaterial;
			
			//Set new Object Mat instance
			UMaterialInstanceDynamic* matInstObject  = UKismetMaterialLibrary::CreateDynamicMaterialInstance(GetWorld(),newMaterialMaster);
			
			if(!IsValid(matInstObject)) continue;
			//UE_LOG(LogTemp, Log, TEXT("bUseADynMatAsMasterMat %i, Material %i :  %s, newMaterialMaster %s"), bUseADynMatAsMasterMat, i, *GetNameSafe(matInstObject), *GetNameSafe(newMaterialMaster));
			
			//Start display Slice Rack Ray
			matInstObject->SetScalarParameterValue(FName("bIsInUse"), true);
			
			//If display in a currently melting face
			if(bUseADynMatAsMasterMat) UpdateMeltingParams(sightedMatInst, matInstObject);
		
			//Add in queue
			_CurrentSightedMatInst.Insert(matInstObject,i);
			_CurrentSightedBaseMats.Insert(newMaterialMaster, i);
			_CurrentSightedComponent->SetMaterial(i, matInstObject);

			//Trigger PostProcess Outline on face sliced
			if(i>0)
				_PlayerCharacter->GetFirstPersonCameraComponent()->OnTriggerOutline(true);
		}

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
	if(IsValid(_CurrentSightedComponent))
	{
		int i = 0;
		//Reset sighted material by their base
		for (UMaterialInterface* material : _CurrentSightedBaseMats)
		{
			UMaterialInstanceDynamic* currentUsedMatInst = Cast<UMaterialInstanceDynamic>(_CurrentSightedComponent->GetMaterial(i));
			

			if(!IsValid(currentUsedMatInst)) continue;

			float bIsMelting = 0.0f;
			currentUsedMatInst->GetScalarParameterValue(FName("bIsMelting"),bIsMelting);
		
			//If display in a currently melting face
		
			//Set new Object Mat instance if sigthed element was Melting
			UMaterialInstanceDynamic* matInstObject = nullptr;
			if(bIsMelting)
			{
				matInstObject = UKismetMaterialLibrary::CreateDynamicMaterialInstance(GetWorld(),material);
				if(!IsValid(matInstObject))
				{
					i++;
					continue;
				}
				UpdateMeltingParams(currentUsedMatInst, matInstObject);
			}
			
			
			_CurrentSightedComponent->SetMaterial(i, bIsMelting ? matInstObject : material);
			
			if(bDebugSightShader) UE_LOG(LogTemp, Warning, TEXT("%S :: reset %s with %s material"), __FUNCTION__, *_CurrentSightedComponent->GetName(), *material->GetName());

			//Stop PostProcess Outline on faced sliced
			_PlayerCharacter->GetFirstPersonCameraComponent()->OnTriggerOutline(false);

			//Increment index
			i++;
			
		}
		
		//Reset variables
		_CurrentSightedComponent = nullptr;
		_CurrentSightedMatInst.Empty();
		_CurrentSightedBaseMats.Empty();
		
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

void UPS_WeaponComponent::ResetMeltingMat(UProceduralMeshComponent* const parentProcComp, UProceduralMeshComponent* const halfChildComponent, const FSCustomSliceOutput& slicingDatas)
{
	//Reset sighted material by their base
	if(_CurrentSightedComponent != parentProcComp)
		parentProcComp->SetMaterial(slicingDatas.InProcMeshCapIndex,SliceableMaterial);

	if(_CurrentSightedComponent != halfChildComponent)
		halfChildComponent->SetMaterial(slicingDatas.OutProcMeshCapIndex,SliceableMaterial);
	
	// UMaterialInstanceDynamic* parentMatInst = Cast<UMaterialInstanceDynamic>(parentProcComp->GetMaterial(slicingDatas.InProcMeshCapIndex));
	// UMaterialInstanceDynamic* childMatInst =  Cast<UMaterialInstanceDynamic>(halfChildComponent->GetMaterial(slicingDatas.InProcMeshCapIndex));
	//
	// //Reset sighted material by their base
	// // parentProcComp->SetMaterial(slicingDatas.InProcMeshCapIndex,_CurrentSightedBaseMats[slicingDatas.InProcMeshCapIndex]);
	// // halfChildComponent->SetMaterial(slicingDatas.OutProcMeshCapIndex,_CurrentSightedBaseMats[slicingDatas.InProcMeshCapIndex]);
	//
	// if(IsValid(parentMatInst))
	// {
	// 	parentMatInst->MarkAsGarbage();
	// 	parentProcComp->SetMaterial(slicingDatas.InProcMeshCapIndex, nullptr);
	// 	
	// 	//parentMatInst->SetScalarParameterValue(FName("bIsMelting"), false);
	// }
	//
	// if(IsValid(childMatInst))
	// {
	// 	childMatInst->MarkAsGarbage();
	// 	halfChildComponent->SetMaterial(slicingDatas.InProcMeshCapIndex, nullptr);
	// 	
	// 	//childMatInst->SetScalarParameterValue(FName("bIsMelting"), false);
	// }
	
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
	


