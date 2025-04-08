// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_HookComponent.h"

#include "PS_PlayerCameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "CableComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetArrayLibrary.h"
#include "ProjectSlice/Data/PS_TraceChannels.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Character/PC/PS_Character.h"
#include "ProjectSlice/Components/GPE/PS_SlicedComponent.h"
#include "ProjectSlice/Data/PS_Constants.h"
#include "ProjectSlice/FunctionLibrary/PSFl.h"

class UCableComponent;

// Sets default values for this component's properties
UPS_HookComponent::UPS_HookComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	HookThrower = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HookThrower"));
	HookThrower->SetCollisionProfileName(Profile_NoCollision);
	
	HookCollider = CreateDefaultSubobject<UBoxComponent>(TEXT("HookCollider"));
	HookCollider->SetCollisionProfileName(Profile_NoCollision);
	
	FirstCable = CreateDefaultSubobject<UCableComponent>(TEXT("FirstCable"));
	FirstCable->SetCollisionProfileName(Profile_NoCollision, true);

	HookPhysicConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("HookConstraint"));
	HookPhysicConstraint->SetDisableCollision(true);

	ConstraintAttachSlave = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConstraintAttachSlave"));
	ConstraintAttachMaster = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConstraintAttachMaster"));
	
	// HookMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookMesh"));
	// HookMesh->SetSimulatePhysics(false);

}


// Called when the game starts
void UPS_HookComponent::BeginPlay()
{
	Super::BeginPlay();

	_PlayerCharacter = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if(!IsValid(_PlayerCharacter)) return;
	
	_PlayerController = Cast<AProjectSlicePlayerController>(_PlayerCharacter->GetController());
	if(!IsValid(_PlayerController)) return;
	
	if(IsValid(FirstCable))
		FirstCableDefaultLenght = FirstCable->CableLength;

	//Callback
	if(IsValid(_PlayerCharacter->GetWeaponComponent()))
	{
		_PlayerCharacter->GetWeaponComponent()->OnWeaponInit.AddUniqueDynamic(this, &UPS_HookComponent::OnInitWeaponEventReceived);
	}
	if(IsValid(_PlayerCharacter->GetSlowmoComponent()))
	{
		_PlayerCharacter->GetSlowmoComponent()->OnSlowmoEvent.AddUniqueDynamic(this, &UPS_HookComponent::OnSlowmoTriggerEventReceived);
	}
	if(IsValid(_PlayerCharacter->GetParkourComponent()))
	{
		_PlayerCharacter->GetParkourComponent()->OnComponentBeginOverlap.AddUniqueDynamic(this, &UPS_HookComponent::OnParkourDetectorBeginOverlapEventReceived);
	}

	//Custom tick - substep to tick at 120 fps (more stable but cable can flicker on unwrap)
	if(bCanUseSubstepTick)
	{
		FTimerDelegate TimerDelegate;
        TimerDelegate.BindUObject(this, &UPS_HookComponent::SubstepTick);
		GetWorld()->GetTimerManager().SetTimer(_SubstepTickHandler, TimerDelegate, 1/360.0f, true);
	}
	
	//Constraint display
	GetConstraintAttachMaster()->SetVisibility(bDebugSwing);
	GetConstraintAttachSlave()->SetVisibility(bDebugSwing);
	
}

void UPS_HookComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	//Callback
	GetHookThrower()->OnComponentHit.RemoveDynamic(this, &UPS_HookComponent::OnHookThrowerHitReceived);
	if(IsValid(_PlayerCharacter->GetWeaponComponent()))
	{
		_PlayerCharacter->GetWeaponComponent()->OnWeaponInit.RemoveDynamic(this, &UPS_HookComponent::OnInitWeaponEventReceived);
	}
	if(IsValid(_PlayerCharacter->GetSlowmoComponent()))
	{
		_PlayerCharacter->GetSlowmoComponent()->OnSlowmoEvent.RemoveDynamic(this, &UPS_HookComponent::OnSlowmoTriggerEventReceived);
	}
	if(IsValid(_PlayerCharacter->GetParkourComponent()))
	{
		_PlayerCharacter->GetParkourComponent()->OnComponentBeginOverlap.RemoveDynamic(this, &UPS_HookComponent::OnParkourDetectorBeginOverlapEventReceived);
	}

}
// Called every frame
void UPS_HookComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//Force attach when arm use physic
	if (IsValid(HookThrower) && HookThrower->IsSimulatingPhysics())
	{
		FVector TargetLocation = GetComponentLocation();
		FQuat TargetRotation = GetComponentQuat();
		HookThrower->SetWorldLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	//CableLogics
	if(!bCanUseSubstepTick) CableWraping();
	PowerCablePull();

}

#pragma region Event_Receiver
//-----------------

void UPS_HookComponent::OnAttachWeapon()
{
	//Setup HookThrower
	HookThrower->SetupAttachment(this);
	HookCollider->SetupAttachment(this);
	HookCollider->SetCollisionProfileName(Profile_NoCollision);
	
	//Setup Cable
	FirstCable->SetupAttachment(HookThrower);
	FirstCable->SetVisibility(false);

	//Rendering Custom Depth stencil
	FirstCable->SetRenderCustomDepth(true);
	FirstCable->SetCustomDepthStencilValue(200);
	FirstCable->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
	
	CableListArray.AddUnique(FirstCable);
	CableCapArray.Add(nullptr);

	//Setup Physic contraint
	HookPhysicConstraint->SetupAttachment(this);
	ConstraintAttachSlave->SetupAttachment(this);
	ConstraintAttachMaster->SetupAttachment(this);
				
	// //Setup HookMesh
	// HookMesh->SetCollisionProfileName(FName("NoCollision"), true);
	// HookMesh->SetupAttachment(CableMesh, FName("RopeEnd"));

	//Callback
	HookThrower->OnComponentBeginOverlap.AddUniqueDynamic(this, &UPS_HookComponent::OnHookThrowerOverlapReceived);
	HookThrower->OnComponentHit.AddUniqueDynamic(this, &UPS_HookComponent::OnHookThrowerHitReceived);

	HookCollider->OnComponentBeginOverlap.AddUniqueDynamic(this, &UPS_HookComponent::OnHookCapsuleBeginOverlapEvent);
	HookCollider->OnComponentEndOverlap.AddUniqueDynamic(this,  &UPS_HookComponent::OnHookCapsuleEndOverlapEvent);
}


void UPS_HookComponent::OnInitWeaponEventReceived()
{
	
}


void UPS_HookComponent::OnSlowmoTriggerEventReceived(const bool bIsSlowed)
{
	//If slowmo is in use
	const UPS_SlowmoComponent* slowmoComp = _PlayerCharacter->GetSlowmoComponent();
	if(IsValid(slowmoComp) && IsValid(AttachedMesh))
	{
		if(!IsValid(AttachedMesh->GetOwner())) return;
		AttachedMesh->GetOwner()->CustomTimeDilation = bIsSlowed ? (slowmoComp->GetGlobalTimeDilationTarget() / slowmoComp->GetPlayerTimeDilationTarget()) : 1.0f;
	}
}


//------------------
#pragma endregion Event_Receiver

#pragma region Arm
//------------------

void UPS_HookComponent::OnHookThrowerHitReceived(UPrimitiveComponent* hitComponent, AActor* otherActor,
	UPrimitiveComponent* otherComp, FVector normalImpulse, const FHitResult& hit)
{
	UE_LOG(LogTemp, Error, TEXT("%S to"), __FUNCTION__);
	if(!IsValid(otherActor) || !IsValid(otherComp) || !IsValid(HookThrower)) return;
	//-hit.ImpactNormal 
	// Calculate repulsive force
	//HookThrower->AddImpulse(-normalImpulse * ArmRepulseStrenght, NAME_None, false);
}

void UPS_HookComponent::OnHookThrowerOverlapReceived(UPrimitiveComponent* overlappedComponent, AActor* otherActor,
	UPrimitiveComponent* otherComp, int32 otherBodyIndex, bool bFromSweep, const FHitResult& sweepResult)
{
	UE_LOG(LogTemp, Error, TEXT("%S te "), __FUNCTION__);
}

void UPS_HookComponent::OnHookCapsuleBeginOverlapEvent(UPrimitiveComponent* overlappedComponent, AActor* otherActor,
	UPrimitiveComponent* otherComp, int32 otherBodyIndex, bool bFromSweep, const FHitResult& sweepResult)
{
	UE_LOG(LogTemp, Warning, TEXT("%S ta"), __FUNCTION__);
}

void UPS_HookComponent::OnHookCapsuleEndOverlapEvent(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex)
{
	UE_LOG(LogTemp, Warning, TEXT("%S ti"), __FUNCTION__);
}

//------------------
#pragma endregion Arm

#pragma region Cable_Wrap_Logic
//------------------

void UPS_HookComponent::SubstepTick()
{
	if(!bCanUseSubstepTick) return;
	
	CableWraping();
}

void UPS_HookComponent::CableWraping()
{
	//Try Wrap only if attached
	if(!IsValid(AttachedMesh) || !IsValid(GetOwner()) || !IsValid(HookThrower)) return;

	if(bDisableCableCodeLogic) return;

	//Wrap Logics
	WrapCableAddByLast();
	WrapCableAddByFirst();
	
	UnwrapCableByLast();
	UnwrapCableByFirst();
	
	//Rope Adaptation
	//AdaptCableTens();
}

void UPS_HookComponent::ConfigLastAndSetupNewCable(UCableComponent* lastCable,const FSCableWarpParams& currentTraceCableWarp, UCableComponent*& newCable) const
{
	//Init works Variables
	const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepRelative, true);

	//Attach Last Cable to Hitted Object, Set his position to it
	lastCable->AttachToComponent(currentTraceCableWarp.OutHit.GetComponent(), AttachmentRule);
	lastCable->SetWorldLocation(currentTraceCableWarp.OutHit.Location, false, nullptr,ETeleportType::TeleportPhysics);

	//Make lastCable tense
	lastCable->CableLength = 5.0f;
	lastCable->bUseSubstepping = false; 

	newCable = Cast<UCableComponent>(GetOwner()->AddComponentByClass(UCableComponent::StaticClass(), false, FTransform(), false));
	if(!IsValid(newCable)) return;
	//Necessary for appear in 
	//GetOwner()->AddInstanceComponent(newCable);

	//Config newCable

	//-Material
	//Debug Cable Color OR use FirstCable material
	SetupCableMaterial(newCable);

	//Rendering
	newCable->SetRenderCustomDepth(true);
	newCable->SetCustomDepthStencilValue(200.0f);
	newCable->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
	newCable->SetReceivesDecals(false);

	//Opti
	newCable->bUseSubstepping = true;
	newCable->bSkipCableUpdateWhenNotVisible = true;

	//Collision
	newCable->bEnableCollision = false;
	newCable->SetCollisionProfileName(Profile_NoCollision, true);

	//Attach
	newCable->bAttachEnd = true;
	newCable->SetAttachEndToComponent(currentTraceCableWarp.OutHit.GetComponent());
	newCable->EndLocation = currentTraceCableWarp.OutHit.GetComponent()->GetComponentTransform().InverseTransformPosition(currentTraceCableWarp.OutHit.Location);

}

void UPS_HookComponent::ConfigCableToFirstCableSettings(UCableComponent* newCable) const
{
	if(!IsValid(FirstCable) || !IsValid(newCable)) return;
	
	//Rendering
	newCable->CableWidth = FirstCable->CableWidth;
	newCable->NumSides = FirstCable->NumSides;
	newCable->TileMaterial = FirstCable->TileMaterial;

	//Tense
	//TODO :: Review this thing for Pull by Tens func
	newCable->CableLength = FirstCable->CableLength;
	newCable->SolverIterations = FirstCable->SolverIterations;
	newCable->bEnableStiffness = FirstCable->bEnableStiffness;

	//Force 
	newCable->CableGravityScale = FirstCable->CableGravityScale;
	
}

void UPS_HookComponent::SetupCableMaterial(UCableComponent* newCable) const
{
	if (bDebug && bDebugMaterialColors)
	{
		if (!IsValid(CableDebugMaterialInst)) return;
	
		UMaterialInstanceDynamic* dynMatInstance = newCable->CreateDynamicMaterialInstance(0, CableDebugMaterialInst);
	
		if (!IsValid(dynMatInstance)) return;
		dynMatInstance->SetVectorParameterValue(FName("Color"),UKismetMathLibrary::HSVToRGB(UKismetMathLibrary::RandomFloatInRange(0, 360), 1, 1, 1));
	}
	else if(IsValid(FirstCable))
	{
		UMaterialInterface* currentCableMaterialInst = FirstCable->GetMaterial(0);
		if(IsValid(currentCableMaterialInst))
			newCable->CreateDynamicMaterialInstance(0, currentCableMaterialInst);
	}
}

void UPS_HookComponent::WrapCableAddByFirst()
{
	//-----Add Wrap Logic-----
	//Add By First
	if(!CableListArray.IsValidIndex(0) || CableListArray.IsEmpty()) return;
	
	UCableComponent* lastCable = CableListArray[0];
	if(!IsValid(lastCable)) return;
	FSCableWarpParams currentTraceCableWarp = TraceCableWrap(lastCable, true);
	
	//If Trace Hit nothing or Invalid object return
	if (!currentTraceCableWarp.OutHit.bBlockingHit || !IsValid(currentTraceCableWarp.OutHit.GetComponent())) return;
		
	//If Location Already Exist return
	if (!CheckPointLocation(CablePointLocations, currentTraceCableWarp.OutHit.Location, CableWrapErrorTolerance)) return;
	
	//----Last Cable && New Points---
	//Add new Point Loc && Hitted Component to Array
	CablePointLocations.Insert(currentTraceCableWarp.OutHit.Location,0);
	CablePointComponents.Insert(currentTraceCableWarp.OutHit.GetComponent(),0);
	CablePointUnwrapAlphaArray.Insert(0.0f,0);
	
	//Config lastCable And Setup newCable
	UCableComponent* newCable = nullptr;
	ConfigLastAndSetupNewCable(lastCable, currentTraceCableWarp, newCable);
	if (!IsValid(newCable))
	{
		UE_LOG(LogTemp, Error, TEXT("%S :: localNewCable Invalid"), __FUNCTION__);
		return;
	}

	//----Caps Sphere---
	//Add Sphere on Caps
	if(bCanUseSphereCaps) AddSphereCaps(currentTraceCableWarp, true);
	

	//Add latest cable to attached cables array && Add this new cable to "cable list array"
	if(CableAttachedArray.IsEmpty())
		CableAttachedArray.Add(newCable);
	else
		CableAttachedArray.Insert(newCable,1);
	CableListArray.Insert(newCable,1);

	//Attach New Cable to Hitted Object && Set his position to it
	const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepRelative, true);
	newCable->SetWorldLocation(CablePointLocations.IsValidIndex(1) ? CablePointLocations[1] : CablePointLocations[0] );
	newCable->AttachToComponent(CablePointLocations.IsValidIndex(1) ? CablePointComponents[1] : CablePointComponents[0], AttachmentRule);
	
	//----Set New Cable Params identical to First Cable---
	if (bCableUseSharedSettings) ConfigCableToFirstCableSettings(newCable);
	
}

void UPS_HookComponent::WrapCableAddByLast()
{
	//-----Add Wrap Logic-----
	if(!CableListArray.IsValidIndex(CableListArray.Num()-1) || CableListArray.IsEmpty()) return;

	UCableComponent* lastCable = CableListArray[CableListArray.Num() - 1];
	if(!IsValid(lastCable)) return;
	FSCableWarpParams currentTraceCableWarp = TraceCableWrap(lastCable, false);

	//If Trace Hit nothing or Invalid object return
	if (!currentTraceCableWarp.OutHit.bBlockingHit || !IsValid(currentTraceCableWarp.OutHit.GetComponent())) return;
	
	//If Location Already Exist return
	if (!CheckPointLocation(CablePointLocations, currentTraceCableWarp.OutHit.Location, CableWrapErrorTolerance)) return;
	
	
	//----Last Cable && New Points---
	//Add new Point Loc && Hitted Component to Array
	CableAttachedArray.Add(lastCable);
	CablePointLocations.Add(currentTraceCableWarp.OutHit.Location);
	CablePointComponents.Add(currentTraceCableWarp.OutHit.GetComponent());
	CablePointUnwrapAlphaArray.Add(0.0f);

	//Config lastCable And Setup newCable
	UCableComponent* newCable = nullptr;
	ConfigLastAndSetupNewCable(lastCable, currentTraceCableWarp, newCable);
	if (!IsValid(newCable))
	{
		UE_LOG(LogTemp, Error, TEXT("%S :: localNewCable Invalid"),__FUNCTION__);
		return;
	}

	//----Caps Sphere---
	//Add Sphere on Caps
	if(bCanUseSphereCaps) AddSphereCaps(currentTraceCableWarp, false);
	
	//Add newCable to list
	CableListArray.Add(newCable);
	
	//Attach New Cable to Hitted Object && Set his position to it
	AttachCableToHookThrower(newCable);

	//----Set New Cable Params identical to First Cable---
	if (bCableUseSharedSettings) ConfigCableToFirstCableSettings(newCable);

}

void UPS_HookComponent::UnwrapCableByFirst()
{	
	//-----Unwrap Logic-----
	
	if(!CableListArray.IsValidIndex(1) || !CableListArray.IsValidIndex(0)) return;

	UCableComponent* pastCable = CableListArray[1];
	UCableComponent* currentCable = CableListArray[0];

	if(!IsValid(currentCable) || !IsValid(pastCable)) return;
	
	//----Unwrap Trace-----
	//If no hit, or hit very close to trace end then process unwrap
	
	if(!CablePointUnwrapAlphaArray.IsValidIndex(0))
	{
		UE_LOG(LogTemp, Error, TEXT("%S :: CablePointUnwrapAlphaArray[%i] Invalid"),__FUNCTION__, CablePointUnwrapAlphaArray.Num()-1);
		return;
	}

	//Trace
	FHitResult outHit;
	if(!TraceCableUnwrap(pastCable, currentCable, true, outHit)) return;
	
	if(outHit.bBlockingHit && !outHit.Location.Equals(outHit.TraceEnd, CableUnwrapErrorMultiplier))
	{
		CablePointUnwrapAlphaArray[0] = 0.0f;
		return;
	}

	//----Custom tick-----
	//Unwrap with delay frames to prevent flickering of wrap/unwrap cycles.Basically increase point alpha value by 1 each frame, if it's more than custom value then process. Use subtle values for responsive unwrap.
	CablePointUnwrapAlphaArray[0] = CablePointUnwrapAlphaArray[0] + 1;
	if(CablePointUnwrapAlphaArray[0] < CableUnwrapFirstFrameDelay)
		return;

	//----Destroy and remove Last Cable tick-----
	if(!CableListArray.IsValidIndex(1)) return;
	
	//If attached cables are more than 0, remove the second one (when first cable gets closer to it)
	CableAttachedArray.RemoveAt(CableAttachedArray.IsValidIndex(1) ? 1 : 0);
	
	//In any case, destroy the second cable (the first one is our main cable)
	CableListArray[1]->DestroyComponent();

	//----Caps Sphere---
	if(bCanUseSphereCaps && CableCapArray.IsValidIndex(1))
	{
		CableCapArray[1]->DestroyComponent();
		CableCapArray.RemoveAt(1);
	}

	//End Unwrap
	CableListArray.RemoveAt(1);
	CablePointLocations.RemoveAt(0);
	CablePointComponents.RemoveAt(0);
	CablePointUnwrapAlphaArray.RemoveAt(0);
	
	//----Set first cable Loc && Attach----
	if(!CableListArray.IsValidIndex(0)) return;
	UCableComponent* firstCable = CableListArray[0];
	
	if(CablePointLocations.IsValidIndex(0))
	{
		const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepRelative, true);

		//Set the latest oldest point as cable active point && attach cable to the latest component point
		firstCable->SetWorldLocation(CablePointLocations[0], false,nullptr, ETeleportType::TeleportPhysics);
		firstCable->AttachToComponent(CablePointComponents[0], AttachmentRule);
	}
	else
	{
		//Reset to base stiffness preset
		firstCable->CableLength = 25.0f;
		firstCable->bUseSubstepping = true;
		
		//Reset to HookAttach default set
		AttachCableToHookThrower(firstCable);
	}
}

void UPS_HookComponent::UnwrapCableByLast()
{	
	//-----Unwrap Logic-----
	//Init works Variables
	int32 cableListLastIndex = CableListArray.Num()-1;
	const int32 cableAttachedLastIndex = CableAttachedArray.Num()-1;
	const int32 cablePointLocationsLastIndex = CablePointLocations.Num()-1;
	
	//Remove By Last
	if(!CableAttachedArray.IsValidIndex(cableAttachedLastIndex) || !CableListArray.IsValidIndex(cableListLastIndex)) return;

	UCableComponent* pastCable = CableAttachedArray[cableAttachedLastIndex];
	UCableComponent* currentCable = CableListArray[cableListLastIndex];
	
	if(!IsValid(currentCable) || !IsValid(pastCable)) return;
	
	//----Unwrap Trace-----
	//If no hit, or hit very close to trace end then process unwrap
	
	float cablePointUnwrapAlphaLastIndex = CablePointUnwrapAlphaArray.Num()-1;
	if(!CablePointUnwrapAlphaArray.IsValidIndex(cablePointUnwrapAlphaLastIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("%S :: CablePointUnwrapAlphaArray[%i] Invalid"),__FUNCTION__, CablePointUnwrapAlphaArray.Num()-1);
		return;
	}
	
	//Trace
	FHitResult outHit;
	if(!TraceCableUnwrap(pastCable, currentCable, false, outHit)) return;
	
	if(outHit.bBlockingHit && !outHit.Location.Equals(outHit.TraceEnd, CableUnwrapErrorMultiplier))
	{
		CablePointUnwrapAlphaArray[cablePointUnwrapAlphaLastIndex] = 0.0f;
		return;
	}

	//----Custom tick-----
	//Unwrap with delay frames to prevent flickering of wrap/unwrap cycles.Basically increase point alpha value by 1 each frame, if it's more than custom value then process. Use subtle values for responsive unwrap.
	CablePointUnwrapAlphaArray[cablePointUnwrapAlphaLastIndex] = CablePointUnwrapAlphaArray[cablePointUnwrapAlphaLastIndex] + 1;
	if (CablePointUnwrapAlphaArray[cablePointUnwrapAlphaLastIndex] < CableUnwrapLastFrameDelay)
		return;
		
	//----Destroy and remove Last Cable tick-----
	if(!CableAttachedArray.IsValidIndex(0) || !CableAttachedArray.IsValidIndex(cableAttachedLastIndex) || !CableListArray.IsValidIndex(cableListLastIndex)) return;

	//If attached cables are more than 0, remove the second one (when first cable gets closer to it)
	CableAttachedArray.RemoveAt(cableAttachedLastIndex);	

	//In any case, destroy the second cable (the first one is our main cable)
	CableListArray[cableListLastIndex]->DestroyComponent();

	//----Caps Sphere---
	if(bCanUseSphereCaps && CableCapArray.IsValidIndex(cableListLastIndex))
	{
		CableCapArray[cableListLastIndex]->DestroyComponent();
		CableCapArray.RemoveAt(cableListLastIndex);
	}
	
	//End Unwrap
	CableListArray.RemoveAt(cableListLastIndex);
	CablePointComponents.RemoveAt(cablePointLocationsLastIndex);
	CablePointUnwrapAlphaArray.RemoveAt(cablePointLocationsLastIndex);
	CablePointLocations.RemoveAt(cablePointLocationsLastIndex);
		
	//----Set first cable Loc && Attach----
	cableListLastIndex = CableListArray.Num() - 1;
	if (!CableListArray.IsValidIndex(cableListLastIndex)) return;
	UCableComponent* firstCable = CableListArray[cableListLastIndex];

	//Reset to HookAttach default set
	AttachCableToHookThrower(firstCable);
}

bool UPS_HookComponent::TraceCableUnwrap(const UCableComponent* pastCable, const UCableComponent* currentCable,const bool& bReverseLoc, FHitResult& outHit) const
{
		
	const TArray<AActor*> actorsToIgnore = {GetOwner()};

	const FVector pastCableStartSocketLoc = bReverseLoc ? pastCable->GetSocketLocation(SOCKET_CABLE_END) : pastCable->GetSocketLocation(SOCKET_CABLE_START);
	const FVector pastCableEndSocketLoc = bReverseLoc ? pastCable->GetSocketLocation(SOCKET_CABLE_START) : pastCable->GetSocketLocation(SOCKET_CABLE_END);
	const FVector currentCableSocketLoc = bReverseLoc ? currentCable->GetSocketLocation(SOCKET_CABLE_END) : currentCable->GetSocketLocation(SOCKET_CABLE_START);

	const FVector currentCableDirection = UKismetMathLibrary::GetForwardVector(UKismetMathLibrary::FindLookAtRotation(currentCableSocketLoc, pastCableEndSocketLoc));
	const float currentCableDirectionDistance = UKismetMathLibrary::Vector_Distance(currentCableSocketLoc, pastCableEndSocketLoc);
	
	const FVector pastCableDirection = UKismetMathLibrary::GetForwardVector(UKismetMathLibrary::FindLookAtRotation(pastCableStartSocketLoc, pastCableEndSocketLoc));

	const FVector start = currentCableSocketLoc;
	const FVector endSafeCheck = start + currentCableDirection * (currentCableDirectionDistance * 0.91);
	const FVector end = pastCableStartSocketLoc + pastCableDirection * CableUnwrapDistance;
		
	//----Safety Trace-----
	//This trace is used as a safety checks if there is no blocking towards the past cable loc.
	FHitResult outHitSafeCheck;
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), start, endSafeCheck,UEngineTypes::ConvertToTraceType(ECC_Rope),
		false, actorsToIgnore, false ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None, outHitSafeCheck, true);

	//If no hit, or hit very close to trace end then continue unwrap
	if(outHitSafeCheck.bBlockingHit && !outHitSafeCheck.Location.Equals(outHitSafeCheck.TraceEnd, CableUnwrapErrorMultiplier)) return false;

	UKismetSystemLibrary::LineTraceSingle(GetWorld(), start, end, UEngineTypes::ConvertToTraceType(ECC_Rope),
		false, actorsToIgnore, bDebug ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None, outHit, true, bReverseLoc ? FColor::Cyan : FColor::Blue);
		
	return true;
}

FSCableWarpParams UPS_HookComponent::TraceCableWrap(const UCableComponent* cable, const bool bReverseLoc) const
{
	if(IsValid(cable) && IsValid(GetWorld()))
	{
		FSCableWarpParams out;

		//If reverseLoc is true we trace Wrap by First
		FVector start = cable->GetSocketLocation(SOCKET_CABLE_START);
		FVector end = cable->GetSocketLocation(SOCKET_CABLE_END);

		out.CableStart = bReverseLoc ? end : start;
		out.CableEnd = bReverseLoc ? start : end;
		
		const TArray<AActor*> actorsToIgnore = {GetOwner()};
		UKismetSystemLibrary::LineTraceSingle(GetWorld(), out.CableStart, out.CableEnd, UEngineTypes::ConvertToTraceType(ECC_Rope),
			true, actorsToIgnore, bDebugCable ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None, out.OutHit, true, bReverseLoc ? FColor::Magenta : FColor::Purple);

		return out;
	}
	else
		return FSCableWarpParams();
}

void UPS_HookComponent::AddSphereCaps(const FSCableWarpParams& currentTraceParams, const bool bIsAddByFirst)
{	
	FVector rotatedCapTowardTarget = UKismetMathLibrary::GetUpVector(UKismetMathLibrary::FindLookAtRotation(currentTraceParams.CableStart,currentTraceParams.OutHit.Location));
	const FTransform& capsRelativeTransform = FTransform(rotatedCapTowardTarget.Rotation(),currentTraceParams.OutHit.Location,UKismetMathLibrary::Conv_DoubleToVector(FirstCable->CableWidth * CapsScaleMultiplicator));
	
	//Add new Cap Sphere (sphere size should be like 0.0105 of cable to fit)
	UStaticMeshComponent* newCapMesh = Cast<UStaticMeshComponent>(GetOwner()->AddComponentByClass(UStaticMeshComponent::StaticClass(), true, capsRelativeTransform,false));
	if (!IsValid(newCapMesh))
	{
		UE_LOG(LogTemp, Error, TEXT("PS_HookComponent :: newCapMesh Invalid"));
		return;
	}

	//Set Mesh && Attach Cap to Hitted Object
	if(IsValid(CapsMesh)) newCapMesh->SetStaticMesh(CapsMesh);
	newCapMesh->SetCollisionProfileName(Profile_NoCollision);
	newCapMesh->SetComponentTickEnabled(false);
	newCapMesh->AttachToComponent(currentTraceParams.OutHit.GetComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	//Setup rendering settings
	newCapMesh->SetRenderCustomDepth(true);
	newCapMesh->SetCustomDepthStencilValue(200.0f);
	newCapMesh->SetReceivesDecals(false);
	
	//Get Cable Material and add to Cable
	if(!bDebugMaterialColors)
	 {
	 	if(IsValid(CableCapsMaterialInst))
	 		newCapMesh->CreateDynamicMaterialInstance(0, CableCapsMaterialInst);
	 }

	//Add to list
	if(bIsAddByFirst)
		CableCapArray.Insert(newCapMesh,1);
	else
		CableCapArray.Add(newCapMesh);
	
	//Force update swing params on create caps if currently swinging
	if (IsPlayerSwinging() && _PlayerCharacter->GetCharacterMovement()->IsFalling())
		OnTriggerSwing(true);
}

bool UPS_HookComponent::CheckPointLocation(const TArray<FVector>& pointsLocation, const FVector& targetLoc, const float& errorTolerance)
{
	bool bLocalPointFound = false;
	for (FVector cableElementLoc : pointsLocation)
	{
		if(cableElementLoc.Equals(targetLoc, errorTolerance)) bLocalPointFound = true;
	}
	return !bLocalPointFound;
}

void UPS_HookComponent::AdaptCableTens()
{
	if(bCablePowerPull) return;

	//----Setup and Verify Modifing Tens Condition---
	//Init works Variables
	UCableComponent* currentCable;
	int32 cableListLastIndex = CableListArray.Num()-1;
	FVector forwardCableLoc = AttachedMesh->GetComponentLocation();

	if(!IsValid(FirstCable)) return;
	
	//If Cable Wrap get point Location
	currentCable = FirstCable;
	
	if(CablePointLocations.IsValidIndex(0))
		forwardCableLoc = CablePointLocations[0];
	
	float baseToMeshDist =	FMath::Abs(UKismetMathLibrary::Vector_Distance(HookThrower->GetComponentLocation(),forwardCableLoc));

	//If not enough near exit
	if(baseToMeshDist > CableBreakTensDistance) return;
	
	//----Adapt Tens---
	const float alpha = UKismetMathLibrary::MapRangeClamped(CableBreakTensDistance - baseToMeshDist , 0, CableBreakTensDistance,0 ,1);
	float curveAlpha = alpha;
	
	if(IsValid(CableTensCurve))
		curveAlpha = CableTensCurve->GetFloatValue(alpha);
	
	currentCable->CableLength =  FMath::Lerp(0,MaxForceWeight, curveAlpha);
}


//------------------
#pragma endregion Cable_Wrap_Logic

#pragma region Grapple_Logic
//------------------

void UPS_HookComponent::HookObject()
{
	//If FirstCable is not in CableList return
	if(!IsValid(FirstCable) || !IsValid(HookThrower) || !IsValid(_PlayerCharacter->GetWeaponComponent())) return;
		
	//Break Hook constraint if already exist Or begin Winding
	if(IsValid(GetAttachedMesh()))
	{
		DettachHook();
		return;
	}

	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S"), __FUNCTION__);

	//Get Trace	
	const FVector start = _PlayerCharacter->GetHookComponent()->GetHookThrower()->GetSocketLocation(SOCKET_HOOK);
	const FVector target = _PlayerCharacter->GetWeaponComponent()->GetLaserTarget();
	
	const TArray<AActor*> actorsToIgnore = {_PlayerCharacter};
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), start, target, UEngineTypes::ConvertToTraceType(ECC_Slice),
		false, actorsToIgnore, EDrawDebugTrace::None, CurrentHookHitResult, true);
	
	//If not blocking exit
	if(!CurrentHookHitResult.bBlockingHit || !IsValid(Cast<UMeshComponent>(CurrentHookHitResult.GetComponent())) || !CurrentHookHitResult.GetComponent()->IsA(UPS_SlicedComponent::StaticClass())) return;
		
	//Hook Move Feedback
	bObjectHook = true;

	//Define new attached component
	AttachedMesh = Cast<UMeshComponent>(CurrentHookHitResult.GetComponent());

	//Attach First cable to it
	//----Setup First Cable---
	AttachCableToHookThrower(FirstCable);
	
	FirstCable->SetAttachEndToComponent(AttachedMesh);
	FirstCable->EndLocation = CurrentHookHitResult.GetComponent()->GetComponentTransform().InverseTransformPosition(CurrentHookHitResult.Location);
	FirstCable->bAttachEnd = true;
	FirstCable->SetCollisionProfileName(Profile_NoCollision, true);
	FirstCable->bEnableCollision = false;
	FirstCable->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
	FirstCable->SetVisibility(true);

	//Setup  new attached component
	AttachedMesh->SetGenerateOverlapEvents(true);
	AttachedMesh->SetCollisionProfileName(Profile_GPE);
	AttachedMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	//TODO :: HookedObject rope wraping
	AttachedMesh->SetCollisionResponseToChannel(ECC_Rope, ECollisionResponse::ECR_Ignore);
	//TODO :: Need to define inertia conditioning to false;
	AttachedMesh->SetLinearDamping(1.0f);
	AttachedMesh->SetAngularDamping(1.0f);
	AttachedMesh->WakeRigidBody();
	
	//Determine max distance for Pull
	DistanceOnAttach = FMath::Abs(UKismetMathLibrary::Vector_Distance(HookThrower->GetComponentLocation(), AttachedMesh->GetComponentLocation()));

	//Activate HookThrower collision
	HookCollider->SetCollisionProfileName(Profile_CharacterMesh);
	HookThrower->SetCollisionProfileName(Profile_CharacterMesh);

	//Callback
	OnHookObject.Broadcast(true);
}

void UPS_HookComponent::DettachHook()
{
	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S"), __FUNCTION__);
	
	//If FirstCable is not in CableList return
	if(!IsValid(FirstCable) || !IsValid(AttachedMesh)) return;

	//----Reset Swing---
	OnTriggerSwing(false);

	//----Stop Cable Warping---
	ResetWindeHook();
	AttachedMesh->SetLinearDamping(0.01f);
	AttachedMesh->SetAngularDamping(0.0f);
	AttachedMesh = nullptr;

	//----Hook Move Feedback---
	bObjectHook = false;
		
	//----Clear Cable Warp ---
	//Destroy Cable
	for (UCableComponent* cable : CableListArray)
	{
		if(!IsValid(cable)) continue;
		
		//Prevent to Destroy FirstCable
		if(cable != FirstCable) cable->DestroyComponent();		
	}

	//Destroy Caps
	for (UStaticMeshComponent* currentCap : CableCapArray)
	{
		if(!IsValid(currentCap)) continue;
		
		currentCap->DestroyComponent();
	}

	//reset FirstCable stiffness
	FirstCable->CableLength = 25.0f;
	FirstCable->bUseSubstepping = true; 

	//Reset CableList && CableCapArray
	CableListArray.Empty();
	CableListArray.AddUnique(FirstCable);
	CableCapArray.Empty();
	CableCapArray.Add(nullptr);
	
	//Clear all other Array
	CableAttachedArray.Empty();
	CablePointUnwrapAlphaArray.Empty();
	CablePointLocations.Empty();
	CablePointComponents.Empty();

	
	//----Setup First Cable---
	AttachCableToHookThrower(FirstCable);

	FirstCable->bAttachEnd = false;
	FirstCable->AttachEndTo = FComponentReference();
	FirstCable->SetVisibility(false);
	FirstCable->SetCollisionProfileName(Profile_NoCollision, true);
	
	//Desactivate HookThrower collision
	HookCollider->SetCollisionProfileName(Profile_NoCollision);
	
	//Callback
	OnHookObject.Broadcast(false);

}


void UPS_HookComponent::AttachCableToHookThrower(UCableComponent* cableToAttach) const
{
	if(!IsValid(cableToAttach)) return;
	
	const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepRelative, true);
	
	cableToAttach->SetWorldLocation(HookThrower->GetSocketLocation(SOCKET_HOOK), false, nullptr, ETeleportType::TeleportPhysics);
	cableToAttach->AttachToComponent(HookThrower, AttachmentRule, SOCKET_HOOK);

	//OLD
	//cable->AttachToComponent(HookThrower, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SOCKET_HOOK);
}

void UPS_HookComponent::WindeHook(const FInputActionInstance& inputActionInstance)
{
	//Break Hook constraint if already exist Or begin Winding
	if (!IsValid(GetAttachedMesh()) || !IsValid(GetWorld())) return;

	if (GetWorld()->GetTimerManager().IsTimerActive(CableWindeMouseCooldown)) return;

	//On wheel axis change reset
	if ((FMath::Sign(CableWindeInputValue) != FMath::Sign(inputActionInstance.GetValue().Get<float>())) &&
		CableWindeInputValue != 0.0f)
	{
		CableWindeInputValue = 0.0f;
		bCableWinderPull = false;

		FTimerDelegate timerDelegate;
		GetWorld()->GetTimerManager().SetTimer(CableWindeMouseCooldown, timerDelegate, 0.1, false);
		return;
	}
	
	bCableWinderPull = true;
	CableStartWindeTimestamp = GetWorld()->GetAudioTimeSeconds();
	CableWindeInputValue = inputActionInstance.GetValue().Get<float>();

	if (bDebugPull) UE_LOG(LogTemp, Log, TEXT("%S"), __FUNCTION__);
	
}

void UPS_HookComponent::StopWindeHook()
{
	if(!_PlayerController->bIsUsingGamepad) return;

	if(bDebugPull) UE_LOG(LogTemp, Log, TEXT("%S"), __FUNCTION__);
	
	ResetWindeHook();
}

void UPS_HookComponent::ResetWindeHook()
{
	CableWindeInputValue = 0.0f;
	bCableWinderPull = false;
}

void UPS_HookComponent::PowerCablePull()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetCharacterMovement()) || !IsValid(AttachedMesh) || !CableListArray.IsValidIndex(0) || !IsValid(GetWorld())) return;
	
	//Current dist to attash loc
	float baseToMeshDist =	FMath::Abs(UKismetMathLibrary::Vector_Distance(HookThrower->GetComponentLocation(),AttachedMesh->GetComponentLocation()));
			
	//Activate Swing if not active
	if(!IsPlayerSwinging() && _PlayerCharacter->GetCharacterMovement()->IsFalling()&& AttachedMesh->GetMass() > _PlayerCharacter->GetMesh()->GetMass())
		OnTriggerSwing(true);
	// if(!IsPlayerSwinging() && _PlayerCharacter->GetCharacterMovement()->IsFalling()  && AttachedMesh->GetMass() > _PlayerCharacter->GetMesh()->GetMass())
	// {
	// 	// float baseToAttachDist =  CablePointComponents.IsValidIndex(0) ? FMath::Abs(UKismetMathLibrary::Vector_Distance(HookThrower->GetComponentLocation(),CablePointComponents[0]->GetComponentLocation())) : baseToMeshDist;
	// 	// if(baseToAttachDist > DistanceOnAttach || !AttachedMesh->IsSimulatingPhysics()) OnTriggerSwing(true);
	// }

	//Swing Tick
	if(bPlayerIsSwinging)
	{
		if(!bSwingIsPhysical)
			OnSwingForce();
		else
			OnSwingPhysic();
	}

	//Dettach if falling with his attached actor
	// FVector gravityVelocity = FVector(0.0f, 0.0f,  GetWorld()->GetGravityZ()) * GetWorld()->GetDeltaSeconds();;
	// if(CablePointLocations.Num() == 0 && _PlayerCharacter->GetCharacterMovement()->IsFalling() && AttachedMesh->)
	// {
	// 	DettachHook();
	// 	return;
	// }
	
	//Activate Pull if Winde
	float alpha;
	float DistanceOnAttachByTensorCount = CableCapArray.Num() > 0 ? DistanceOnAttach/CableCapArray.Num() : DistanceOnAttach;
	
	if(bCableWinderPull)
	{
		const float windeAlpha = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetAudioTimeSeconds(), CableStartWindeTimestamp ,CableStartWindeTimestamp + MaxWindePullingDuration,0 ,1);
		const float inputalpha = FMath::Clamp(CableWindeInputValue,-1.0f,1.0f);
		
		alpha = windeAlpha * inputalpha;
		UE_LOG(LogTemp, Error, TEXT("Winder alpha %f"), alpha);
		
		_PlayerCharacter->GetProceduralAnimComponent()->ApplyWindingVibration(FMath::Abs(alpha));
		if(IsValid(WindePullingCurve))
		{
			alpha = WindePullingCurve->GetFloatValue(inputalpha);
		}
		//TODO :: If want to activate Winde during swing don't forget to reactivate SetLinearLimitZ
		// //Winde on swing
		// if(bPlayerIsSwinging)
		// {
		// 	HookPhysicConstraint->SetLinearZLimit(LCM_Limited, FMath::Lerp(SwingMaxDistance, MinLinearLimitZ,alpha));
		// }
	}
	//Else try to Activate Pull On reach Max Distance
	else if(IsValid(AttachedMesh) && AttachedMesh->IsSimulatingPhysics())
	{
		alpha = UKismetMathLibrary::MapRangeClamped(baseToMeshDist - DistanceOnAttachByTensorCount, 0, MaxForcePullingDistance,0 ,1);
		bCablePowerPull = baseToMeshDist > DistanceOnAttach + CablePullSlackDistance;
	}
	
	//Try Auto Break Rope if tense is too high
	if(bCablePowerPull)
	{
		//UE_LOG(LogTemp, Error, ("PhysicLinearVel %f"), AttachedMesh->GetPhysicsLinearVelocity().Length());
		float baseToAttachDist =  CablePointComponents.IsValidIndex(0) ? FMath::Abs(UKismetMathLibrary::Vector_Distance(HookThrower->GetComponentLocation(),CablePointComponents[0]->GetComponentLocation())) : baseToMeshDist;

		_AlphaTense = UKismetMathLibrary::MapRangeClamped(baseToAttachDist, DistanceOnAttach, DistanceOnAttach + CableBreakTensDistance, 0.0f, 1.0f);
		
		if(baseToAttachDist > (DistanceOnAttach + CableBreakTensDistance) || AttachedMesh->GetPhysicsLinearVelocity().Length() > CableMaxTensVelocityThreshold)
		{
			DettachHook();
			return;
		}
	}
	
	//If can't Pull or Swing return
	if(!bCablePowerPull && !bCableWinderPull && !bPlayerIsSwinging) return;
	
	//MaxForceWeight impacted by object pulled mass
	float forceWeight = MaxForceWeight;

	//Common Pull logic
	if(bCableWinderPull || bCablePowerPull)
	{
		// float playerMassScaled = UKismetMathLibrary::SafeDivide(_PlayerCharacter->GetCharacterMovement()->Mass, _PlayerCharacter->GetMesh()->GetMassScale());
		float playerMassScaled = UPSFl::GetObjectUnifiedMass(_PlayerCharacter->GetMesh());
		float objectMassScaled = UPSFl::GetObjectUnifiedMass(AttachedMesh);
		
		// float distAlpha = UKismetMathLibrary::MapRangeClamped(baseToMeshDist - DistanceOnAttachByTensorCount, 0, MaxForcePullingDistance,0 ,1);
		// float massAlpha = UKismetMathLibrary::MapRangeClamped(playerMassScaled,0,objectMassScaled,0,1);

		const float alphaMass = UKismetMathLibrary::MapRangeClamped(objectMassScaled, playerMassScaled, playerMassScaled * MaxPullWeight, 1.0f, 0.0f);
		forceWeight = FMath::Lerp(0.0f, MaxForceWeight, alphaMass);
		
		if(bDebugPull) UE_LOG(LogTemp, Log, TEXT("%S :: playerMassScaled %f, objectMassScaled %f, alphaMass %f, forceWeight %f"), __FUNCTION__, playerMassScaled, objectMassScaled, alphaMass, forceWeight);
		//if(bDebugPull && bDebugTick) UE_LOG(LogTemp, Log, TEXT("%S :: reach Max dist massAlpha %f, distAlpha %f"), __FUNCTION__, massAlpha, distAlpha);
		// alpha = massAlpha * distAlpha;
	}
	
	//Determine force weight
	ForceWeight = FMath::Lerp(0.0f,forceWeight, alpha);

	//Testing dist to lastLoc
	if(!GetWorld()->GetTimerManager().IsTimerActive(_AttachedSameLocTimer))
	{
		if(UKismetMathLibrary::Vector_Distance2DSquared(_LastAttachedActorLoc, AttachedMesh->GetComponentLocation()) < AttachedMaxDistThreshold * AttachedMaxDistThreshold)
		{
			FTimerDelegate timerDelegate;
			timerDelegate.BindUObject(this, &UPS_HookComponent::OnBlockedTimerEndEventRecevied);
			GetWorld()->GetTimerManager().SetTimer(_AttachedSameLocTimer, timerDelegate, AttachedSameLocMaxDuration, false);
		}
		else
		{
			_bAttachObjectIsBlocked = false;
		}	
	}
	
	//Determine Pull Direction
	//1st method for try of unblock object
	FVector start = AttachedMesh->GetComponentLocation();
	FVector end = (CableAttachedArray.IsValidIndex(0) ? CableAttachedArray[0] : CableListArray[0])->GetSocketLocation(SOCKET_CABLE_START);
	FRotator rotMeshCable = UKismetMathLibrary::FindLookAtRotation(start,end);

	//Debug base Pull dir
	if(bDebug)
	{
		DrawDebugDirectionalArrow(GetWorld(), start, start + rotMeshCable.Vector() * 500 , 10.0f, FColor::Orange, false, 0.02f, 10, 3);
	}

	//If attached determine modified trajectory
	if(CableAttachedArray.IsValidIndex(1) && _bAttachObjectIsBlocked)
	{

		//2nd method for try of unblock object
		if(UKismetMathLibrary::RandomBool())
		{
			UE_LOG(LogTemp, Error, TEXT("2nd method"));
			FVector endNextAttached = CableAttachedArray[1]->GetSocketLocation(SOCKET_CABLE_START);
			FRotator rotMeshCableNextAttached = UKismetMathLibrary::FindLookAtRotation(start,endNextAttached);
			rotMeshCable.Yaw = rotMeshCableNextAttached.Yaw;
		
			if(bDebug)
			{
				DrawDebugPoint(GetWorld(), CableAttachedArray[1]->GetSocketLocation(SOCKET_CABLE_START), 30.0f, FColor::Red, false, 0.1f, 10.0f);
				DrawDebugDirectionalArrow(GetWorld(), start, start + rotMeshCableNextAttached.Vector() * 500 , 10.0f,  FColor::Red, false, 0.02f, 10, 3);
			}
		}
		else
		{
			//3rd method for try of unblock object
			UE_LOG(LogTemp, Error, TEXT("3nd method"));
			start = end;
			start.Z = 0.0f;
			end = CableAttachedArray[1]->GetSocketLocation(SOCKET_CABLE_START);
			end.Z = 0.0f;
			rotMeshCable = UKismetMathLibrary::FindLookAtRotation(start,end);
		}		
		
		//If unblocking object is impossible break cable
		// else
		// {
		// 	UE_LOG(LogTemp, Warning, TEXT("%S :: Object too much blocked break cable"), __FUNCTION__);
		// 	DettachHook();
		// 	return;
		// }
	}
	
	//Add a random offset
	rotMeshCable.Yaw = rotMeshCable.Yaw + UKismetMathLibrary::RandomFloatInRange(-50, 50);

	//Stocking current Loc for futur test
	_LastAttachedActorLoc = AttachedMesh->GetComponentLocation();
	
	//Debug modified Pull dir
	if(bDebugPull)
	{
		DrawDebugDirectionalArrow(GetWorld(), start, end, 10.0f, FColor::Yellow, false, 0.02f, 10, 3);
	}
	
	//Pull Force
	if(IsValid(AttachedMesh) && IsValid(GetWorld()))
	{
		FVector newVel = AttachedMesh->GetMass() * rotMeshCable.Vector() * ForceWeight;
		AttachedMesh->AddImpulse((newVel * GetWorld()->DeltaRealTimeSeconds) * _PlayerCharacter->CustomTimeDilation,  NAME_None, false);
		//AttachedMesh->AddForce((newVel * GetWorld()->DeltaRealTimeSeconds) * _PlayerCharacter->CustomTimeDilation,  NAME_None, false);
	}
	
}

void UPS_HookComponent::OnBlockedTimerEndEventRecevied()
{
	if(!IsValid(AttachedMesh)) return;
	
	_bAttachObjectIsBlocked = UKismetMathLibrary::Vector_Distance2DSquared(_LastAttachedActorLoc, AttachedMesh->GetComponentLocation()) < AttachedMaxDistThreshold * AttachedMaxDistThreshold;
	UE_LOG(LogTemp, Error, TEXT("%S :: _bAttachObjectIsBlocked %i"),__FUNCTION__, _bAttachObjectIsBlocked)

	//If stay blocked in secondary methods for multiple time, retry default method && reiterate
	_AttachedSameLocTimerTriggerCount++;
	if(_AttachedSameLocTimerTriggerCount > 5)
	{
		UE_LOG(LogTemp, Warning, TEXT("%S ::  stay blocked in secondary methods for multiple time, retry default method"),__FUNCTION__)
		_bAttachObjectIsBlocked = false;
		_AttachedSameLocTimerTriggerCount = 0;
		return;
	}
	
	//If object stay blocked reiterate secondary methods
	if(_bAttachObjectIsBlocked)
	{
		
		FTimerDelegate timerDelegate;
		timerDelegate.BindUObject(this, &UPS_HookComponent::OnBlockedTimerEndEventRecevied);
		GetWorld()->GetTimerManager().SetTimer(_AttachedSameLocTimer, timerDelegate, AttachedSameLocMaxDuration, false);
	}
}

#pragma region Swing
//------------------

void UPS_HookComponent::OnTriggerSwing(const bool bActivate)
{
	//if(bPlayerIsSwinging == bActivate) return;

	if(bDebugSwing) UE_LOG(LogTemp, Warning, TEXT("%S :: bActivate %i"), __FUNCTION__,bActivate);
	
	if(!IsValid(GetWorld()) || 	!IsValid(_PlayerCharacter->GetCharacterMovement()) || !IsValid(_PlayerCharacter) || !IsValid(AttachedMesh)) return;
		
	bPlayerIsSwinging = bActivate;
	SwingStartTimestamp = GetWorld()->GetTimeSeconds();
	//_PlayerController->SetCanMove(!bActivate);
	//_PlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	//_PlayerCharacter->GetCharacterMovement()->GravityScale = bActivate ? SwingGravityScale : _DefaultGravityScale;
	_PlayerCharacter->GetCharacterMovement()->AirControl = bActivate ? SwingMaxAirControl : _PlayerCharacter->GetDefaultAirControl();
	_PlayerCharacter->GetCharacterMovement()->BrakingDecelerationFalling = 400.0f;
	
	if(bDebugSwing)
		UE_LOG(LogTemp, Warning, TEXT("%S bPlayerIsSwinging: %i \n :: __ System Parameters __ :: \n "), __FUNCTION__, bPlayerIsSwinging);
	
	if(bActivate)
	{
		if(!bSwingIsPhysical)
		{
			_SwingStartLoc = (_PlayerCharacter->GetActorLocation() - AttachedMesh->GetComponentLocation());
			_SwingStartFwd = _PlayerCharacter->GetArrowComponent()->GetForwardVector();
		}
		else
		{
			ConstraintAttachSlave->SetMassOverrideInKg(NAME_None,_PlayerCharacter->GetCharacterMovement()->Mass);
			ConstraintAttachMaster->SetMassOverrideInKg(NAME_None,AttachedMesh->GetMass());
						
			UCableComponent* cableToAdapt = IsValid(CableListArray.Last()) ? CableListArray.Last() : FirstCable;
			AActor* masterAttachActor = CablePointComponents.IsValidIndex(0) ? CablePointComponents[0]->GetOwner() : _PlayerCharacter;
			FVector masterLoc = CablePointComponents.IsValidIndex(0) ? CablePointComponents[0]->GetComponentLocation() : CurrentHookHitResult.Location;
			ConstraintAttachMaster->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

			if(IsValid(cableToAdapt) && IsValid(masterAttachActor))
			{
				//Activate constraint debug
				HookPhysicConstraint->bVisualizeComponent = bDebugSwing;
				
				//Setup Component constrainted
				HookPhysicConstraint->ConstraintActor1 = _PlayerCharacter;
				HookPhysicConstraint->ConstraintActor2 = masterAttachActor;
				
				HookPhysicConstraint->ComponentName1.ComponentName = FName(GetConstraintAttachSlave()->GetName());
				HookPhysicConstraint->ComponentName2.ComponentName = CablePointComponents.IsValidIndex(0) ? FName(CablePointComponents[0]->GetName()) :  FName(GetConstraintAttachMaster()->GetName());
				//HookPhysicConstraint->ComponentName2.ComponentName = FName(ConstraintAttachMaster->GetName());
				
				//Set Linear Limit				
				HookPhysicConstraint->SetLinearXLimit(LCM_Limited, DistanceOnAttach + CablePullSlackDistance);
				HookPhysicConstraint->SetLinearYLimit(LCM_Limited, DistanceOnAttach + CablePullSlackDistance);
				HookPhysicConstraint->SetLinearZLimit(LCM_Limited, DistanceOnAttach + CablePullSlackDistance);

				//Set collision and physic
				ConstraintAttachMaster->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
				ConstraintAttachMaster->SetWorldLocation(masterLoc);

				//Set collision and activate physics				
				GetConstraintAttachSlave()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
				GetConstraintAttachSlave()->SetSimulatePhysics(true);
				GetConstraintAttachSlave()->SetWorldLocation(HookThrower->GetComponentLocation());

				//Init constraint
				HookPhysicConstraint->InitComponentConstraint();
				
				//Impulse Slave for preserve player enter velocity
				_SwingImpulseForce = _PlayerCharacter->GetVelocity().Length();
				ImpulseConstraintAttach();
								
			}
		}
	}
	else
	{
		if(!bSwingIsPhysical)
		{
			_SwingStartLoc = FVector::ZeroVector;
			_SwingStartFwd = FVector::ZeroVector;
		}
		else
		{
			FVector directionToCenterMass = GetConstraintAttachMaster()->GetComponentLocation() - GetConstraintAttachSlave()->GetComponentLocation();
			directionToCenterMass.Normalize();
			_SwingImpulseForce = GetConstraintAttachSlave()->GetComponentVelocity().Length();
			
			GetConstraintAttachMaster()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GetConstraintAttachMaster()->AttachToComponent(HookThrower, FAttachmentTransformRules::SnapToTargetNotIncludingScale,  FName(SOCKET_HOOK));
			
			GetConstraintAttachSlave()->SetSimulatePhysics(false);
			GetConstraintAttachSlave()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GetConstraintAttachSlave()->AttachToComponent(HookThrower, FAttachmentTransformRules::SnapToTargetNotIncludingScale,  FName(SOCKET_HOOK));

			
			HookPhysicConstraint->ConstraintActor1 = nullptr;
			HookPhysicConstraint->ConstraintActor2 = nullptr;
			
			HookPhysicConstraint->ComponentName1.ComponentName = FName("None");
			HookPhysicConstraint->ComponentName2.ComponentName = FName("None");

			HookPhysicConstraint->InitComponentConstraint();
			HookPhysicConstraint->UpdateConstraintFrames();

			ForceWeight = 0.0f;

			//Set Player velocity to slave velocity
			_PlayerCharacter->GetCharacterMovement()->Velocity = ConstraintAttachSlave->GetComponentVelocity();
			
			//Launch if Stop swing by jump			
			if(_PlayerCharacter->GetCharacterMovement()->IsFalling())
			{
				FVector impulseDirection = 	UKismetMathLibrary::GetDirectionUnitVector(_SwingPlayerLastLoc, _PlayerCharacter->GetActorLocation());
				_PlayerCharacter->LaunchCharacter(impulseDirection * _SwingImpulseForce, true, true);
				
				if(bDebugSwing)
					DrawDebugDirectionalArrow(GetWorld(), _PlayerCharacter->GetActorLocation(), _PlayerCharacter->GetActorLocation() + impulseDirection * 500, 10.0f, FColor::Yellow, false, 2, 10, 3);
			}
		}

	}
}


void UPS_HookComponent::OnSwingForce()
{
	if(_PlayerCharacter->GetCharacterMovement()->IsMovingOnGround()
	  || _PlayerCharacter->GetParkourComponent()->IsWallRunning()
	  || _PlayerCharacter->GetParkourComponent()->IsLedging()
	  || _PlayerCharacter->GetParkourComponent()->IsMantling()
	/*|| _PlayerCharacter->GetVelocity().Length() <= _PlayerCharacter->GetCharacterMovement()->MaxWalkSpeedCrouched*/)
	{
		OnTriggerSwing(false);

		//----Auto break hook if velocity too low----
		// if(bCablePowerPull &&  _PlayerCharacter->GetVelocity().Length() <= _SwingStartVelocity.Length())
		// 	DettachHook();
		return;
	}
	
	//Init alpha
	FVector dir = (_PlayerCharacter->GetActorLocation() - AttachedMesh->GetComponentLocation());
	
	const float timeWeightAlpha = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetTimeSeconds(), SwingStartTimestamp, SwingStartTimestamp + SwingMaxDuration, 0.0f, 1.0f);
	//const float alphaCurve = UKismetMathLibrary::MapRangeClamped(dir.Z, 0.0f, _SwingStartLoc.Z * (1 - timeWeightAlpha), 0.0f, (1 - timeWeightAlpha));
	const float alphaCurve = UKismetMathLibrary::MapRangeClamped(dir.Z, _SwingStartLoc.Z, _SwingStartLoc.Z - SwingMaxDistance, 0.0f, (1 - timeWeightAlpha));
	const bool bIsGoingDown = alphaCurve > SwingAlpha;
	SwingAlpha = alphaCurve;
	
	dir.Normalize();
	DrawDebugDirectionalArrow(GetWorld(), _PlayerCharacter->GetActorLocation(),_PlayerCharacter->GetActorLocation() + dir * 600, 20.0f, FColor::Blue, false, 0.2, 10, 3);
	
	FVector swingVelocity = -1 * dir * (_PlayerCharacter->GetActorLocation() - AttachedMesh->GetComponentLocation()).Dot(_PlayerCharacter->GetVelocity());
	swingVelocity = UPSFl::ClampVelocity(swingVelocity, dir * _PlayerCharacter->GetDefaultMaxWalkSpeed(), _PlayerCharacter->GetDefaultMaxWalkSpeed() * 2);

	//Air Control:
	// const float targetAirControl = FMath::Lerp(SwingMaxAirControl, DefaultAirControl, timeWeightAlpha);
	const float airControlAlpha = UKismetMathLibrary::MapRangeClamped(FMath::Abs(_PlayerCharacter->GetVelocity().Length()), 0.0f, _PlayerCharacter->GetCharacterMovement()->GetMaxSpeed(), 0.0f, 1.0f);
	_PlayerCharacter->GetCharacterMovement()->AirControl = FMath::Lerp(_PlayerCharacter->GetDefaultAirControl(), SwingMaxAirControl, airControlAlpha);

	//Braking Deceleration Falling
	float brakingDecAlpha = bIsGoingDown ? alphaCurve : 1 - alphaCurve;
	_PlayerCharacter->GetCharacterMovement()->BrakingDecelerationFalling = FMath::Lerp(400.0f, _PlayerCharacter->GetCharacterMovement()->BrakingDecelerationWalking / 2, brakingDecAlpha);

	//Add Force
	_PlayerCharacter->GetCharacterMovement()->AddForce((swingVelocity) * _PlayerCharacter->CustomTimeDilation);
	//_PlayerCharacter->GetCharacterMovement()->AddForce(_PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector() *  * _PlayerCharacter->CustomTimeDilation);


	_VelocityToAbsFwd = _SwingStartFwd.Dot(_PlayerCharacter->GetVelocity());
	//Force reset swing vel orientation
	if(alphaCurve < 0.2 && !_OrientationIsReset)
	{
		_VelocityToAbsFwd = _VelocityToAbsFwd * -1;
		_OrientationIsReset = true;
	}
	else
	{
		_OrientationIsReset = false;
	}
	const float alphaWeight = UKismetMathLibrary::MapRangeClamped(FMath::Abs(_VelocityToAbsFwd), 0.0f,_PlayerCharacter->GetCharacterMovement()->MaxFlySpeed, 0.1f, 1.0f);
	float fakeInputAlpha = FMath::Sign(_VelocityToAbsFwd) * alphaWeight;	

	if(bDebugSwing)
	{
		DrawDebugDirectionalArrow(GetWorld(), _PlayerCharacter->GetActorLocation(),_PlayerCharacter->GetActorLocation() + _PlayerCharacter->GetArrowComponent()->GetForwardVector() * 400 , 20.0f, FColor::Cyan, false, 0.1, 10, 3);
		DrawDebugDirectionalArrow(GetWorld(), _PlayerCharacter->GetActorLocation(),_PlayerCharacter->GetActorLocation() + _PlayerCharacter->GetVelocity() * 400, 20.0f, FColor::Green, false, 0.1, 10, 3);
	}
	
	//Add movement
	_PlayerCharacter->AddMovementInput( _SwingStartFwd * _PlayerCharacter->CustomTimeDilation, UKismetMathLibrary::SafeDivide(alphaCurve,fakeInputAlpha));
	_PlayerCharacter->AddMovementInput( _PlayerCharacter->GetArrowComponent()->GetForwardVector() * _PlayerCharacter->CustomTimeDilation, _PlayerController->GetMoveInput().Y * (alphaCurve / SwingInputScaleDivider));
	_PlayerCharacter->AddMovementInput( _PlayerCharacter->GetArrowComponent()->GetRightVector() * _PlayerCharacter->CustomTimeDilation, _PlayerController->GetMoveInput().X * (alphaCurve / SwingInputScaleDivider));

	if(bDebugTick && bDebugSwing)
		UE_LOG(LogTemp, Log, TEXT("fakeInputAlpha %f, alphaCurve %f, FwdScale %f, \n airControlAlpha %f,  BrakingDeceleration %f, \n bIsGoingDown %i, velocityToAbsFwd %f"), fakeInputAlpha, (alphaCurve), alphaCurve * fakeInputAlpha, airControlAlpha, _PlayerCharacter->GetCharacterMovement()->BrakingDecelerationFalling, bIsGoingDown, _VelocityToAbsFwd);

	//Stop by time
	if(timeWeightAlpha >= 1)
	{
		DettachHook();
		return;
	}
}

void UPS_HookComponent::OnSwingPhysic()
{
	if(_PlayerCharacter->GetCharacterMovement()->IsMovingOnGround()
	  || _PlayerCharacter->GetParkourComponent()->IsWallRunning()
	  || _PlayerCharacter->GetParkourComponent()->IsLedging()
	  || _PlayerCharacter->GetParkourComponent()->IsMantling()
	  /*|| _PlayerCharacter->GetVelocity().Length() <= (_PlayerCharacter->GetCharacterMovement()->MaxWalkSpeedCrouched / 2) */)
	{
		OnTriggerSwing(false);
		
		//----Auto break hook if velocity too low----
		// if(bCablePowerPull &&  _PlayerCharacter->GetVelocity().Length() <= _SwingStartVelocity.Length())
		// 	DettachHook();
		return;
	}

	//Fake AirControl for influence swing
	if(!_PlayerController->GetMoveInput().IsNearlyZero())
	{
		FVector swingControlDir = UPSFl::GetWorldInputDirection(_PlayerCharacter->GetFirstPersonCameraComponent(), _PlayerController->GetMoveInput());
		swingControlDir.Normalize();
	
		float airControlSpeed = _PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed * _PlayerCharacter->GetCharacterMovement()->AirControl;
		FVector swingAirControlVel = swingControlDir * GetWorld()->DeltaTimeSeconds * airControlSpeed * SwingCustomAirControlMultiplier;

		GetConstraintAttachSlave()->AddForce(swingAirControlVel, NAME_None, true);
	}
	
	//Set actor location to ConstraintAttach
	FTimerDelegate setLastLocDel;
	FTimerHandle setLastLocHandle;
	setLastLocDel.BindUFunction(this,FName("SetSwingPlayerLastLoc"), GetConstraintAttachSlave()->GetComponentLocation());
	GetWorld()->GetTimerManager().SetTimer(setLastLocHandle,setLastLocDel,0.1,false);
	
	//Move physic constraint to match player position (attached element) && //Update constraint position on component
	const float lastIndex = CablePointLocations.Num() - 1;
	//const float timeWeightAlpha = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetTimeSeconds(), SwingStartTimestamp, SwingStartTimestamp + SwingMaxDuration, 0.0f, 1.0f);

	//Update slave, master and constraint physic loc
	_PlayerCharacter->GetRootComponent()->SetWorldLocation(GetConstraintAttachSlave()->GetComponentLocation());
	GetConstraintAttachMaster()->SetWorldLocation(CablePointLocations.IsValidIndex(lastIndex) ? CablePointLocations[lastIndex] : CurrentHookHitResult.Location);

	HookPhysicConstraint->SetWorldLocation(GetConstraintAttachSlave()->GetComponentLocation(), false);
	HookPhysicConstraint->UpdateConstraintFrames();
	
	//Update Linearlimit if update cable point
	if(CablePointLocations.IsValidIndex(lastIndex))
	{
		// const FVector firstCableEndLocation = CurrentHookHitResult.GetComponent()->GetComponentTransform().TransformPosition(FirstCable->EndLocation);
		// float lineartDistByPoint = FMath::Max(DistanceOnAttach - UKismetMathLibrary::Vector_Distance(CablePointLocations[lastIndex], firstCableEndLocation), SwingMinDistanceAttach);
		//
		// HookPhysicConstraint->SetLinearXLimit(LCM_Limited, lineartDistByPoint);
		// HookPhysicConstraint->SetLinearYLimit(LCM_Limited, lineartDistByPoint);
		//if(!bCableWinderPull)#1# HookPhysicConstraint->SetLinearZLimit(LCM_Limited, lineartDistByPoint);

		HookPhysicConstraint->SetLinearXLimit(LCM_Limited, DistanceOnAttach + CablePullSlackDistance);
		HookPhysicConstraint->SetLinearYLimit(LCM_Limited, DistanceOnAttach + CablePullSlackDistance);
		//if(!bCableWinderPull)*/ HookPhysicConstraint->SetLinearZLimit(LCM_Limited, DistanceOnAttach);
	}
	else
	{		
		HookPhysicConstraint->SetLinearXLimit(LCM_Limited, DistanceOnAttach + CablePullSlackDistance);
		HookPhysicConstraint->SetLinearYLimit(LCM_Limited, DistanceOnAttach + CablePullSlackDistance);
		//if(!bCableWinderPull)*/ HookPhysicConstraint->SetLinearZLimit(LCM_Limited, DistanceOnAttach);
	}
	
	//Stop by time
	// if(timeWeightAlpha >= 1)
	// {
	// 	DettachHook();
	// 	return;
	// }

	//Debug
	if(bDebugSwing) DrawDebugPoint(GetWorld(), GetConstraintAttachMaster()->GetComponentLocation(), 20.f, FColor::Red, true);
			
}

void UPS_HookComponent::ImpulseConstraintAttach() const
{
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetCharacterMovement()) || _PlayerCharacter->GetCharacterMovement()->MovementMode == MOVE_Walking) return;
	
	FVector dir = _PlayerCharacter->GetVelocity();
	dir.Normalize();
	GetConstraintAttachSlave()->AddImpulse(dir * _SwingImpulseForce * _PlayerCharacter->CustomTimeDilation,NAME_None, true);

	if(bDebugSwing) UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__);
}

//------------------
#pragma endregion Swing

void UPS_HookComponent::OnParkourDetectorBeginOverlapEventReceived(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComp,int32 otherBodyIndex, bool bFromSweep, const FHitResult& sweepResult)
{
	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S"), __FUNCTION__);
	
	if(bPlayerIsSwinging)
	{
		ForceInvertSwingDirection();
	}
	
}



//------------------
#pragma endregion Grapple_Logic