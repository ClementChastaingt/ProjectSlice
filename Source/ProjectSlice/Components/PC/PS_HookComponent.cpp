// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_HookComponent.h"

#include "PS_PlayerCameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "CableComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ProjectSlice/Data/PS_TraceChannels.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialExpressionSkyAtmosphereLightIlluminance.h"
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
	HookCollider = CreateDefaultSubobject<UBoxComponent>(TEXT("HookCollider"));
	
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
		_FirstCableDefaultLenght = FirstCable->CableLength;

	//Callback
	if(IsValid(_PlayerCharacter->GetSlowmoComponent()))
	{
		_PlayerCharacter->GetSlowmoComponent()->OnSlowmoEvent.AddUniqueDynamic(this, &UPS_HookComponent::OnSlowmoTriggerEventReceived);
	}
	if(IsValid(_PlayerCharacter->GetParkourComponent()))
	{
		_PlayerCharacter->GetParkourComponent()->OnComponentBeginOverlap.AddUniqueDynamic(this, &UPS_HookComponent::OnParkourDetectorBeginOverlapEventReceived);
	}
	if(IsValid(_PlayerCharacter->GetCharacterMovement()))
	{
		_PlayerCharacter->MovementModeChangedDelegate.AddUniqueDynamic(this,&UPS_HookComponent::OnMovementModeChangedEventReceived);
	}
	if(IsValid(HookCollider))
	{
		HookCollider->OnComponentBeginOverlap.AddUniqueDynamic(this, &UPS_HookComponent::OnHookBoxBeginOverlapEvent);
		HookCollider->OnComponentEndOverlap.AddUniqueDynamic(this,  &UPS_HookComponent::OnHookBoxEndOverlapEvent);
	}
	
	//Custom tick - substep to tick at 120 fps (more stable but cable can flicker on unwrap)
	if(bCanUseSubstepTick)
	{
		FTimerDelegate LowTimerDelegate;
        LowTimerDelegate.BindUObject(this, &UPS_HookComponent::SubstepTickLowLatency);
		// 1/90 for extra low latency 
		GetWorld()->GetTimerManager().SetTimer(_LowSubstepTickHandler, LowTimerDelegate, 1/90.0f, true);

		// 1/360 for 120 FPS
		FTimerDelegate HighTimerDelegate;
		HighTimerDelegate.BindUObject(this, &UPS_HookComponent::SubstepTickHighLatency);
		GetWorld()->GetTimerManager().SetTimer(_HighSubstepTickHandler, HighTimerDelegate, 1/360.0f, true);
	}
	
	//Constraint display
	GetConstraintAttachMaster()->SetVisibility(bDebugSwing);
	GetConstraintAttachSlave()->SetVisibility(bDebugSwing);
		
}

void UPS_HookComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	//Callback
	if(IsValid(_PlayerCharacter->GetSlowmoComponent()))
	{
		_PlayerCharacter->GetSlowmoComponent()->OnSlowmoEvent.RemoveDynamic(this, &UPS_HookComponent::OnSlowmoTriggerEventReceived);
	}
	if(IsValid(_PlayerCharacter->GetParkourComponent()))
	{
		_PlayerCharacter->GetParkourComponent()->OnComponentBeginOverlap.RemoveDynamic(this, &UPS_HookComponent::OnParkourDetectorBeginOverlapEventReceived);
	}
	if(IsValid(HookCollider))
	{
		HookCollider->OnComponentBeginOverlap.RemoveDynamic(this, &UPS_HookComponent::OnHookBoxBeginOverlapEvent);
		HookCollider->OnComponentEndOverlap.RemoveDynamic(this, &UPS_HookComponent::OnHookBoxEndOverlapEvent);
	}

}

// Called every frame
void UPS_HookComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//Arm
	ArmTick();
	
	//Cable
	if(!bCanUseSubstepTick)
	{
		CableWraping();

		PowerCablePull();
	}

	//Swing
	SwingTick(DeltaTime);

}


void UPS_HookComponent::OnMovementModeChangedEventReceived(ACharacter* Character, EMovementMode PrevMovementMode,
	uint8 PreviousCustomMode)
{
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetCharacterMovement())) return;

	switch (_PlayerCharacter->GetCharacterMovement()->MovementMode)
	{
		case MOVE_Falling:
			ForceDettachByFall();
			break;
		
		default:
			break;
	}
}

#pragma region Event_Receiver
//-----------------

void UPS_HookComponent::InitHookComponent()
{
	//Setup HookThrower
	HookThrower->SetupAttachment(this);
	HookCollider->SetupAttachment(this);
	
	//Setup Cable
	FirstCable->SetupAttachment(HookThrower);
	FirstCable->SetVisibility(false);

	//Rendering Custom Depth stencil
	FirstCable->SetRenderCustomDepth(true);
	FirstCable->SetCustomDepthStencilValue(200);
	FirstCable->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
	
	CableListArray.AddUnique(FirstCable);

	//Setup Physic contraint
	HookPhysicConstraint->SetupAttachment(this);
	ConstraintAttachSlave->SetupAttachment(this);
	ConstraintAttachMaster->SetupAttachment(this);
	
	// //Setup HookMesh
	// HookMesh->SetCollisionProfileName(FName("NoCollision"), true);
	// HookMesh->SetupAttachment(CableMesh, FName("RopeEnd"));
}

void UPS_HookComponent::OnSlowmoTriggerEventReceived(const bool bIsSlowed)
{
	//If slowmo is in use
	const UPS_SlowmoComponent* slowmoComp = _PlayerCharacter->GetSlowmoComponent();
	if(IsValid(slowmoComp) && IsValid(_AttachedMesh))
	{
		if(!IsValid(_AttachedMesh->GetOwner())) return;
		_AttachedMesh->GetOwner()->CustomTimeDilation = bIsSlowed ? (slowmoComp->GetGlobalTimeDilationTarget() / slowmoComp->GetPlayerTimeDilationTarget()) : 1.0f;
	}
}

void UPS_HookComponent::OnParkourDetectorBeginOverlapEventReceived(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComp,int32 otherBodyIndex, bool bFromSweep, const FHitResult& sweepResult)
{	
	// if(_bPlayerIsSwinging)
	// {
	// 	if(bDebugSwing) UE_LOG(LogTemp, Log, TEXT("%S :: ForceInvertSwingDirection"), __FUNCTION__);
	// 	//TODO :: OnParkourDetectorBeginOverlapEventReceived seems to be trigger when collide player things
	// 	ForceInvertSwingDirection();
	// }
	
}

//------------------
#pragma endregion Event_Receiver

#pragma region Arm
//------------------

void UPS_HookComponent::OnHookBoxBeginOverlapEvent(UPrimitiveComponent* overlappedComponent, AActor* otherActor,
	UPrimitiveComponent* otherComp, int32 otherBodyIndex, bool bFromSweep, const FHitResult& sweepResult)
{
	if(otherActor == _PlayerCharacter) return;
	
	InitArmPhysicalAnimation();
	ToggleArmPhysicalAnimation(true);
}

void UPS_HookComponent::OnHookBoxEndOverlapEvent(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex)
{
	if(otherActor == _PlayerCharacter) return;
	
	ToggleArmPhysicalAnimation(false);
}

void UPS_HookComponent::InitArmPhysicalAnimation()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(HookThrower)) return;

	_PhysicAnimComp = _PlayerCharacter->GetPhysicAnimComponent();
	
	if(!IsValid(_PhysicAnimComp)) return;
	
	if(bDebugArm) UE_LOG(LogTemp, Log, TEXT("%S"), __FUNCTION__);

	//Set mesh with Arm
	_PlayerCharacter->GetPhysicAnimComponent()->SetSkeletalMeshComponent(GetHookThrower());
}

void UPS_HookComponent::ToggleArmPhysicalAnimation(const bool bActivate)
{
	if(!IsValid(_PhysicAnimComp) || !IsValid(HookThrower) || !IsValid(GetWorld())) return;

	if(bDebugArm) UE_LOG(LogTemp, Log, TEXT("%S :: bActivate %i"), __FUNCTION__, bActivate);
	
	//Tweak PhysicAnimComp
	if(bActivate)
	{
		HookThrower->SetAllBodiesBelowSimulatePhysics(BONE_SPINE, true, false);
		_PhysicAnimComp->ApplyPhysicalAnimationProfileBelow(BONE_SPINE, FName("StayAtLoc"), false, false);
	}
	
	//Setup work var
	_bArmIsRagdolled = bActivate;
	_bBlendOutToUnphysicalized = !bActivate;
	_ArmTogglePhysicAnimTimestamp = GetWorld()->GetTimeSeconds();
	
}

void UPS_HookComponent::ArmTick()
{
	if(!IsValid(GetWorld())) return;

	//Blend out during time when _bArmIsUsingPhysicAnim
	if(_bBlendOutToUnphysicalized)
	{
		const float alpha = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetTimeSeconds(), _ArmTogglePhysicAnimTimestamp, _ArmTogglePhysicAnimTimestamp + ArmPhysicAnimRecoveringDuration, 0.0f, 1.0f);
		HookThrower->SetAllBodiesBelowPhysicsBlendWeight(BONE_SPINE,1 - alpha, false, true);

		//Stop 
		if(alpha >= 1.0f)
		{
			HookThrower->SetAllBodiesBelowSimulatePhysics(BONE_SPINE, false, false);
			_bBlendOutToUnphysicalized = false;
		}
	}
}

//------------------
#pragma endregion Arm

#pragma region Cable
//------------------

void UPS_HookComponent::SubstepTickLowLatency()
{
	if(!bCanUseSubstepTick) return;

	PowerCablePull();
}

void UPS_HookComponent::SubstepTickHighLatency()
{
	if(!bCanUseSubstepTick) return;
	
	CableWraping();
}

void UPS_HookComponent::CableWraping()
{
	//Try Wrap only if attached
	if(!IsValid(_AttachedMesh) || !IsValid(GetOwner()) || !IsValid(HookThrower)) return;

	if(bDisableCableCodeLogic) return;

	//Wrap Logics
	WrapCableAddByLast();
	WrapCableAddByFirst();
	
	UnwrapCableByLast();
	UnwrapCableByFirst();

	//Update location if object is physical with caps loc 
	//UpdatePointLocation();
	
	//Rope Adaptation
	//AdaptCableTens();
}

void UPS_HookComponent::ConfigLastAndSetupNewCable(UCableComponent* lastCable,const FSCableWarpParams& currentTraceCableWarp, UCableComponent*& newCable, const bool bReverseLoc) const
{
	//Init works Variables
	const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);
	
	//Attach Last Cable to Hitted Object, Set his position to it
	lastCable->AttachToComponent(currentTraceCableWarp.OutHit.GetComponent(), AttachmentRule);
	lastCable->SetWorldLocation(currentTraceCableWarp.OutHit.Location, false, nullptr,ETeleportType::TeleportPhysics);
	
	newCable = Cast<UCableComponent>(GetOwner()->AddComponentByClass(UCableComponent::StaticClass(), false, FTransform(), false));
	if(!IsValid(newCable)) return;
	//Necessary for appear in BP
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

	//Tense
	newCable->CableLength = _FirstCableDefaultLenght;
	newCable->SolverIterations = 1;
	//newCable->SolverIterations = 8;
	newCable->bEnableStiffness = FirstCable->bEnableStiffness;
	
	newCable->NumSegments = 10; // BE CAREFULL: num segment is involved in CableSocketEnd loc calculation need to stay superior of 1;
	//newCable->MarkRenderStateDirty(); // Necessary for Update cable, otherwise can cause crash
	
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
	newCable->CableWidth = FirstCable->CableWidth / 2.85;
	newCable->NumSides = FirstCable->NumSides;
	newCable->TileMaterial = FirstCable->TileMaterial;

	//Tense
	//TODO :: Review this thing for Pull by Tens func*
	newCable->CableLength = _FirstCableDefaultLenght;
	newCable->bEnableStiffness = FirstCable->bEnableStiffness;

	//Force 
	newCable->CableGravityScale = FirstCable->CableGravityScale;
	
}

void UPS_HookComponent::SetupCableMaterial(UCableComponent* newCable) const
{
	if (bDebugCable && bDebugMaterialColors)
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
	if(_bArmIsRagdolled) return;
	
	//-----Add Wrap Logic-----
	//Add By First
	if(!CableListArray.IsValidIndex(0) || CableListArray.IsEmpty()) return;
	
	UCableComponent* lastCable = CableListArray[0];
	if(!IsValid(lastCable)) return;
	FSCableWarpParams currentTraceCableWarp = TraceCableWrap(lastCable, true);
	
	//If Trace Hit nothing or Invalid object return
	if (!currentTraceCableWarp.OutHit.bBlockingHit || !IsValid(currentTraceCableWarp.OutHit.GetComponent())) return;
		
	//If Location Already Exist return
	if (!CheckPointLocation(currentTraceCableWarp.OutHit.Location, CableWrapErrorTolerance)) return;
	
	//----Last Cable && New Points---
	//Add new Point Loc && Hitted Component to Array
	CablePointComponents.Insert(currentTraceCableWarp.OutHit.GetComponent(),0);
	CablePointUnwrapAlphaArray.Insert(0.0f,0);
		
	//Config lastCable And Setup newCable
	UCableComponent* newCable = nullptr;
	ConfigLastAndSetupNewCable(lastCable, currentTraceCableWarp, newCable, true);
	if (!IsValid(newCable))
	{
		UE_LOG(LogTemp, Error, TEXT("%S :: localNewCable Invalid"), __FUNCTION__);
		return;
	}

	//----Caps Sphere---
	//Add Sphere on Caps
	AddSphereCaps(currentTraceCableWarp, true);
	
	//Add latest cable to attached cables array && Add this new cable to "cable list array"
	if(CableAttachedArray.IsEmpty())
		CableAttachedArray.Add(newCable);
	else
		CableAttachedArray.Insert(newCable,1);
	CableListArray.Insert(newCable,1);

	//Attach New Cable to Hitted Object && Set his position to it
	if(!CableCapArray.IsValidIndex(1) || !CablePointComponents.IsValidIndex(1)) return;

	const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);
	newCable->SetWorldLocation(CableCapArray[1]->GetComponentLocation());
	newCable->AttachToComponent(CablePointComponents[1], AttachmentRule);
	
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
	if (!CheckPointLocation(currentTraceCableWarp.OutHit.Location, CableWrapErrorTolerance)) return;
	
	
	//----Last Cable && New Points---
	//Add new Point Loc && Hitted Component to Array
	CableAttachedArray.Add(lastCable);
	CablePointComponents.Add(currentTraceCableWarp.OutHit.GetComponent());
	CablePointUnwrapAlphaArray.Add(0.0f);
	
	//Config lastCable And Setup newCable
	UCableComponent* newCable = nullptr;
	ConfigLastAndSetupNewCable(lastCable, currentTraceCableWarp, newCable, false);
	if (!IsValid(newCable))
	{
		UE_LOG(LogTemp, Error, TEXT("%S :: localNewCable Invalid"),__FUNCTION__);
		return;
	}

	//----Caps Sphere---
	//Add Sphere on Caps
	AddSphereCaps(currentTraceCableWarp, false);
	
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
 
	if(!IsValid(currentCable) || !IsValid(pastCable) || currentCable == pastCable) return;
	
	//----Unwrap Trace-----
	//If no hit, or hit very close to trace end then process unwrap
	
	if(!CablePointUnwrapAlphaArray.IsValidIndex(0))
	{
		UE_LOG(LogTemp, Error, TEXT("%S :: CablePointUnwrapAlphaArray[%i] Invalid"),__FUNCTION__, CablePointUnwrapAlphaArray.Num()-1);
		return;
	}

	//Trace
	FHitResult outHit, outInboundCheck;

	if(!TraceCableUnwrap(pastCable, currentCable, true, outHit)) return;
	if(CableAttachedArray.IsValidIndex(2) && CableAttachedArray.IsValidIndex(1)) TraceCableUnwrap(pastCable,CableListArray[2], true, outInboundCheck);
	
	if(outHit.bBlockingHit && !outHit.Location.Equals(outHit.TraceEnd, CableUnwrapErrorMultiplier) && !outInboundCheck.bBlockingHit)
	{
		CablePointUnwrapAlphaArray[0] = 0.0f;
		return;
	}

	//----Custom tick-----
	//Unwrap with delay frames to prevent flickering of wrap/unwrap cycles.Basically increase point alpha value by 1 each frame, if it's more than custom value then process. Use subtle values for responsive unwrap.
	CablePointUnwrapAlphaArray[0] = CablePointUnwrapAlphaArray[0] + 1;
	if(CablePointUnwrapAlphaArray[0] < CableUnwrapFirstFrameDelay)
	{
		return;
	}


	//----Destroy and remove Last Cable tick-----
	if(!CableListArray.IsValidIndex(1)) return;
	
	//If attached cables are more than 0, remove the second one (when first cable gets closer to it)
	CableAttachedArray.RemoveAt(CableAttachedArray.IsValidIndex(1) ? 1 : 0);
	
	//In any case, destroy the second cable (the first one is our main cable)
	CableListArray[1]->DestroyComponent();

	//----Caps Sphere---
	if(CableCapArray.IsValidIndex(0))
	{
		CableCapArray[0]->DestroyComponent();
		CableCapArray.RemoveAt(0);
	}

	//End Unwrap
	CableListArray.RemoveAt(1);
	CablePointComponents.RemoveAt(0);
	CablePointUnwrapAlphaArray.RemoveAt(0);
	
	//----Set first cable Loc && Attach----
	if(!CableListArray.IsValidIndex(0)) return;
	UCableComponent* firstCable = CableListArray[0];
	
	if(CableCapArray.IsValidIndex(0) && IsValid(CableCapArray[0]))
	{
		const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);
		
		//Set the latest oldest point as cable active point && attach cable to the latest component point
		firstCable->SetWorldLocation(CableCapArray[0]->GetComponentLocation(), false,nullptr, ETeleportType::TeleportPhysics);
		firstCable->AttachToComponent(CablePointComponents[0],AttachmentRule);
	}
	else
	{
		//Reset to base stiffness preset
		firstCable->CableLength = _FirstCableDefaultLenght;
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
	const int32 cablePointLocationsLastIndex = CablePointComponents.Num()-1;
	
	//Remove By Last
	if(!CableAttachedArray.IsValidIndex(cableAttachedLastIndex) || !CableListArray.IsValidIndex(cableListLastIndex)) return;

	UCableComponent* pastCable = CableAttachedArray[cableAttachedLastIndex];
	UCableComponent* currentCable = CableListArray[cableListLastIndex];
	
	if(!IsValid(currentCable) || !IsValid(pastCable) || currentCable == pastCable) return;
	
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
	if(CableCapArray.IsValidIndex(cablePointLocationsLastIndex))
	{
		CableCapArray[cablePointLocationsLastIndex]->DestroyComponent();
		CableCapArray.RemoveAt(cablePointLocationsLastIndex);
	}
	
	//End Unwrap
	CableListArray.RemoveAt(cableListLastIndex);
	CablePointComponents.RemoveAt(cablePointLocationsLastIndex);
	CablePointUnwrapAlphaArray.RemoveAt(cablePointLocationsLastIndex);

	//If Swinging reset linearZ
	if (IsPlayerSwinging() && _PlayerCharacter->GetCharacterMovement()->IsFalling())
		ForceUpdateMasterConstraint();
	
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
		false, actorsToIgnore, bDebugCable ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None, outHitSafeCheck, true);

	//If no hit, or hit very close to trace end then continue unwrap
	if(outHitSafeCheck.bBlockingHit && !outHitSafeCheck.Location.Equals(outHitSafeCheck.TraceEnd, CableUnwrapErrorMultiplier)) return false;

	UKismetSystemLibrary::LineTraceSingle(GetWorld(), start, end, UEngineTypes::ConvertToTraceType(ECC_Rope),
		false, actorsToIgnore, bDebugCable ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None, outHit, true, bReverseLoc ? FColor::Cyan : FColor::Blue);

	//Find the closest loc on the actor hit collision
	if(IsValid(outHit.GetActor()))
	{
		FVector outClosestPoint;
		UPSFl::FindClosestPointOnActor(outHit.GetActor(),outHit.Location,outClosestPoint);
		if(!outClosestPoint.IsZero()){outHit.Location = outClosestPoint;}
	}
		
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
		
		//Find the closest loc on the actor hit collision
		if(IsValid(out.OutHit.GetActor()))
		{
			FVector outClosestPoint;
			UPSFl::FindClosestPointOnActor(out.OutHit.GetActor(),out.OutHit.Location,outClosestPoint);
			if(!outClosestPoint.IsZero())
			{
				out.OutHit.Location = outClosestPoint;
			}
		}
		
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

	//Set loc and scale
	newCapMesh->SetWorldLocation(currentTraceParams.OutHit.Location);
	newCapMesh->AttachToComponent(currentTraceParams.OutHit.GetComponent(), FAttachmentTransformRules::KeepWorldTransform);

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
		CableCapArray.Insert(newCapMesh,0);
	else
		CableCapArray.Add(newCapMesh);
	
	//Force update swing params on create caps if currently swinging
	if (IsPlayerSwinging() && _PlayerCharacter->GetCharacterMovement()->IsFalling())
		ForceUpdateMasterConstraint();
}

bool UPS_HookComponent::CheckPointLocation(const FVector& targetLoc, const float& errorTolerance)
{
	bool bLocalPointFound = false;
	for (UStaticMeshComponent* cableCapElement : CableCapArray)
	{
		if(!IsValid(cableCapElement)) continue;
		
		if(cableCapElement->GetComponentLocation().Equals(targetLoc, errorTolerance)) bLocalPointFound = true;
	}
	return !bLocalPointFound;
}


void UPS_HookComponent::AdaptCableTense(const float alphaTense)
{
	const int32 lastIndex = CableListArray.Num() - 1;

	if(!CableListArray.IsValidIndex(lastIndex)) return;
		
	//CableLength iterate from last(characterCable) to last +
	int32 index = 1;
	for (int i = lastIndex; i >= 0; i--)
	{
		if(!CableListArray.IsValidIndex(i) || !IsValid(CableListArray[i]) )
		{
			index++;
			continue;
		}
		
		//Solver
		const float start = CableSolverRange.Max;
		const float end = CableSolverRange.Min;
		const int32 newSolverIterations = FMath::InterpStep(start, end, alphaTense / index, CableSolverRange.Max);
		if(CableListArray[i]->SolverIterations != newSolverIterations) CableListArray[i]->SolverIterations = newSolverIterations;

		//Lenght
		const float distBetCable = FMath::Abs(UKismetMathLibrary::Vector_Distance(CableListArray[i]->GetSocketLocation(SOCKET_CABLE_START),CableListArray[i]->GetSocketLocation(SOCKET_CABLE_END)));
		
		const float min = distBetCable / CableMinLengthDivider;
		const float max = distBetCable * CableMaxLengthMultiplicator;
		const float newLenght = FMath::Lerp(max, min, (alphaTense / index));

		//UE_LOG(LogTemp, Error, TEXT("%S :: index %i, distBetCable %f, max %f min %f, newLenght %f, solverIterations %i"),__FUNCTION__, i,distBetCable, max,min, newLenght, newSolverIterations); 
		
		CableListArray[i]->CableLength = newLenght;

		//Iterate
		index++;
	}; 

	//UE_LOG(LogTemp, Log, TEXT("%S :: alphaTense %f"),__FUNCTION__, alphaTense); 
}

//------------------
#pragma endregion Cable

#pragma region Grapple
//------------------

void UPS_HookComponent::HookObject()
{
	//If FirstCable is not in CableList return
	if(!IsValid(_PlayerCharacter) || !IsValid(FirstCable) || !IsValid(HookThrower) || !IsValid(_PlayerCharacter->GetWeaponComponent())) return;
		
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
		false, actorsToIgnore, EDrawDebugTrace::None, _CurrentHookHitResult, true);
	
	//If not blocking exit
	if(!_CurrentHookHitResult.bBlockingHit || !IsValid(Cast<UMeshComponent>(_CurrentHookHitResult.GetComponent())) || !_CurrentHookHitResult.GetComponent()->IsA(UPS_SlicedComponent::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("Error"));
		OnHookObjectFailed.Broadcast();
		return;
	}
	
	
	//Define new attached component
	_AttachedMesh = Cast<UMeshComponent>(_CurrentHookHitResult.GetComponent());

	//Attach First cable to it
	//----Setup First Cable---
	AttachCableToHookThrower(FirstCable);
	
	FirstCable->SetAttachEndToComponent(_AttachedMesh);
	FirstCable->EndLocation = _CurrentHookHitResult.GetComponent()->GetComponentTransform().InverseTransformPosition(_CurrentHookHitResult.Location);
	FirstCable->bAttachEnd = true;
	FirstCable->SetCollisionProfileName(Profile_NoCollision, true);
	FirstCable->bEnableCollision = false;
	FirstCable->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
	FirstCable->SetVisibility(true);

	//Setup  new attached component
	_AttachedMesh->SetGenerateOverlapEvents(true);
	_AttachedMesh->SetCollisionProfileName(Profile_GPE);
	_AttachedMesh->SetCollisionResponseToChannel(ECC_Rope,  ECollisionResponse::ECR_Ignore);
	_AttachedMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	_AttachedMesh->SetCollisionResponseToChannel(ECC_Rope, ECollisionResponse::ECR_Ignore);
	//TODO :: Need to define inertia conditioning to false;
	_AttachedMesh->SetLinearDamping(1.0f);
	_AttachedMesh->SetAngularDamping(1.0f);
	_AttachedMesh->WakeRigidBody();
	
	//Determine max distance for Pull
	_DistanceOnAttach = FMath::Abs(UKismetMathLibrary::Vector_Distance(HookThrower->GetComponentLocation(), _AttachedMesh->GetComponentLocation()));
	
	//Callback
	OnHookObject.Broadcast(true);
}

void UPS_HookComponent::DettachHook()
{	
	//If FirstCable is not in CableList return
	if(!IsValid(FirstCable) || !IsValid(_AttachedMesh)) return;

	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S"), __FUNCTION__);

	//----Reset Var---
	bHasTriggerBreakByFall = false;

	//----Reset Swing---
	OnTriggerSwing(false);

	//----Stop Cable Warping---
	ResetWindeHook();
	_AttachedMesh->SetLinearDamping(0.01f);
	_AttachedMesh->SetAngularDamping(0.0f);
	_AttachedMesh->SetCollisionProfileName(Profile_GPE, false);
	_AttachedMesh = nullptr;
			
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
	FirstCable->CableLength = _FirstCableDefaultLenght;
	FirstCable->SolverIterations = 1.0f;
	FirstCable->bUseSubstepping = true; 

	//Reset CableList && CableCapArray
	CableListArray.Empty();
	CableListArray.AddUnique(FirstCable);
	CableCapArray.Empty();
	
	//Clear all other Array
	CableAttachedArray.Empty();
	CablePointUnwrapAlphaArray.Empty();
	CablePointComponents.Empty();

	
	//----Setup First Cable---
	AttachCableToHookThrower(FirstCable);

	FirstCable->bAttachEnd = false;
	FirstCable->AttachEndTo = FComponentReference();
	FirstCable->SetVisibility(false);
	FirstCable->SetCollisionProfileName(Profile_NoCollision, true);
		
	//Callback
	OnHookObject.Broadcast(false);

}

void UPS_HookComponent::AttachCableToHookThrower(UCableComponent* cableToAttach) const
{
	if(!IsValid(cableToAttach) || !IsValid(HookThrower)) return;

	const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);
	cableToAttach->AttachToComponent(HookThrower, AttachmentRule, SOCKET_HOOK);
}

#pragma region Pull
//------------------

#pragma region Winde
//------------------

void UPS_HookComponent::WindeHook(const FInputActionInstance& inputActionInstance)
{
	//Break Hook constraint if already exist Or begin Winding
	if (!IsValid(GetAttachedMesh()) || !IsValid(GetWorld())) return;
	
	//Update Winde vars
	_bCableWinderIsActive = true;
	_WindeInputAxis1DValue += inputActionInstance.GetValue().Get<float>();
	_WindeInputAxis1DValue = FMath::Clamp(_WindeInputAxis1DValue,-WindeMaxInputWeight, WindeMaxInputWeight);
	_CableWindeInputValue = UKismetMathLibrary::MapRangeClamped(_WindeInputAxis1DValue,-WindeMaxInputWeight,WindeMaxInputWeight, -1.0f,1.0f);

	
	UE_LOG(LogTemp, Warning, TEXT(" _CableWindeInputValue %f, _UnclampedWindeInputValue %f"), _CableWindeInputValue, _WindeInputAxis1DValue);
	
	if (bDebugPull) UE_LOG(LogTemp, Log, TEXT("%S :: _CableWindeInputValue %f"), __FUNCTION__, _CableWindeInputValue);
	
}

void UPS_HookComponent::ResetWinde(const FInputActionInstance& inputActionInstance)
{
	if(bDebugPull) UE_LOG(LogTemp, Log, TEXT("%S"),__FUNCTION__);
	
	//On wheel axis change reset
	if (FMath::Sign(inputActionInstance.GetValue().Get<float>()) != FMath::Sign(_WindeInputAxis1DValue) && _bCableWinderIsActive)
	{
		ResetWindeHook();
	}
}

void UPS_HookComponent::ResetWindeHook()
{
	_WindeInputAxis1DValue = 0.0f;
	_CableWindeInputValue = 0.0f;
	
	_CablePullSlackDistance = CablePullSlackDistanceRange.Min;
	_PlayerCharacter->GetProceduralAnimComponent()->ApplyWindingVibration(0);

	_bCableWinderIsActive = false;

	//TODO :: add reset for swing logic too
}

//------------------
#pragma endregion Winde

void UPS_HookComponent::DetermineForceWeight(const float alpha)
{
	//Common Pull logic*
	float playerMassScaled = UPSFl::GetObjectUnifiedMass(_PlayerCharacter->GetMesh());
	float objectMassScaled = UPSFl::GetObjectUnifiedMass(_AttachedMesh);

	//CableCaps num PullForce bonus
	const float alphaMass = UKismetMathLibrary::MapRangeClamped(objectMassScaled, playerMassScaled, playerMassScaled * MaxPullWeight, 1.0f, 0.0f);
	const float capsBonus = ForceCapsWeight * CableCapArray.Num();
	float forceWeight = FMath::Lerp(0.0f, MaxForceWeight + capsBonus, alphaMass);
	forceWeight = FMath::Clamp(forceWeight, 0.0f,ForceWeightMaxThreshold);
	
	//Determine force weight
	_ForceWeight = FMath::Lerp(0.0f,forceWeight, alpha);
}

float UPS_HookComponent::CalculatePullAlpha(const float baseToMeshDist)
{
	if(_CableWindeInputValue != 0.0f)
	{
		//Applicate curve to winde alpha input
		_AlphaWinde = FMath::Abs(_CableWindeInputValue);
		if(IsValid(WindePullingCurve))
		{
			_AlphaWinde = WindePullingCurve->GetFloatValue(_AlphaWinde);
		}

		//Winde anim only in pull not slack
		if(_CableWindeInputValue < 0) _PlayerCharacter->GetProceduralAnimComponent()->ApplyWindingVibration(_AlphaWinde);

		//Determine current slack distance
		const float dist = FMath::Lerp(CablePullSlackDistanceRange.Min, CablePullSlackDistanceRange.Max, _AlphaWinde);
		_CablePullSlackDistance = dist * FMath::Sign(_CableWindeInputValue);
	}

	//Override _DistanceOnAttach
	if(_DistanceOnAttach + _CablePullSlackDistance + _CablePullSlackDistance < _DistanceOnAttach - CablePullSlackDistanceRange.Min && !_bPlayerIsSwinging)
		_DistanceOnAttach = baseToMeshDist;
	
	//Distance On Attach By point number weight
	float distanceOnAttachByTensorWeight = UKismetMathLibrary::SafeDivide(_DistanceOnAttach + _CablePullSlackDistance, CableCapArray.Num());
	
	//Calculate Pull alpha && activate pull
	const float alpha = UKismetMathLibrary::MapRangeClamped(baseToMeshDist + distanceOnAttachByTensorWeight, 0, _DistanceOnAttach + _CablePullSlackDistance,0 ,1);
	_bCablePowerPull = baseToMeshDist + distanceOnAttachByTensorWeight > _DistanceOnAttach + _CablePullSlackDistance;

	if(bDebugPull) UE_LOG(LogTemp, Log, TEXT("%S :: baseToMeshDist %f, _DistanceOnAttach %f, _DistOnAttachWithRange %f, distanceOnAttachByTensorWeight %f, alpha %f"),__FUNCTION__, baseToMeshDist, _DistanceOnAttach, _DistanceOnAttach + _CablePullSlackDistance, distanceOnAttachByTensorWeight, alpha);
	
	return alpha; 
}

void UPS_HookComponent::CheckingIfObjectIsBlocked()
{
	if(!GetWorld()->GetTimerManager().IsTimerActive(_AttachedSameLocTimer))
	{
		if(UKismetMathLibrary::Vector_Distance2DSquared(_LastAttachedActorLoc, _AttachedMesh->GetComponentLocation()) < AttachedMaxDistThreshold * AttachedMaxDistThreshold)
		{
			if(bDebugPull) UE_LOG(LogTemp, Log, TEXT("%S :: AttachedObject is under Dist2D Threshold"), __FUNCTION__);
			
			FTimerDelegate timerDelegate;
			timerDelegate.BindUObject(this, &UPS_HookComponent::OnAttachedSameLocTimerEndEventReceived);
			GetWorld()->GetTimerManager().SetTimer(_AttachedSameLocTimer, timerDelegate, AttachedSameLocMaxDuration, false);
		}
		else
		{
			_bAttachObjectIsBlocked = false;
			
			//Stocking current Loc for futur test
			_LastAttachedActorLoc = _AttachedMesh->GetComponentLocation();
		}
		_UnblockTimerTimerArray.Empty();
	}
}

void UPS_HookComponent::ForceDettachByFall()
{
	if(!IsValid(_PlayerCharacter)
		|| !IsValid(_AttachedMesh)
		|| !_PlayerCharacter->GetCharacterMovement()->IsFalling()) return;

	//If First time trigger timer
	if(!bHasTriggerBreakByFall)
	{
		FTimerHandle timerHandle;
		FTimerDelegate timerDelegate;

		timerDelegate.BindUObject(this, &UPS_HookComponent::ForceDettachByFall);
		GetWorld()->GetTimerManager().SetTimer(timerHandle, timerDelegate, BreakByFallCheckLatency, false);

		bHasTriggerBreakByFall = true;
				
		return;
	}

	//If ti's second time DettachHook
	if(UKismetMathLibrary::Max(_PlayerCharacter->GetVelocity().Z, _AttachedMesh->GetComponentVelocity().Z) < VelocityZToleranceToBreak)
	{
		DettachHook();
	}
	else
	{
		FTimerHandle timerHandle;
		FTimerDelegate timerDelegate;

		timerDelegate.BindUObject(this, &UPS_HookComponent::ForceDettachByFall);
		GetWorld()->GetTimerManager().SetTimer(timerHandle, timerDelegate, BreakByFallCheckLatency, false);

		bHasTriggerBreakByFall = false;
	}
}


void UPS_HookComponent::PowerCablePull()
{
	if (!IsValid(_PlayerCharacter)
		|| !IsValid(_AttachedMesh)
		|| !_AttachedMesh->IsSimulatingPhysics()
		|| !CableListArray.IsValidIndex(0)
		|| !IsValid(GetWorld()))
		return;
	
	//Current dist to attach loc
	float baseToMeshDist =	FMath::Abs(UKismetMathLibrary::Vector_Distance(HookThrower->GetComponentLocation(),_AttachedMesh->GetComponentLocation()));
	
	//Calculate current pull alpha (Winde && Distance Pull)
	const float alpha = CalculatePullAlpha(baseToMeshDist);

	//Try Auto Break Rope if tense is too high else Adapt Cable tense render
	_AlphaTense = UKismetMathLibrary::MapRangeClamped((_DistanceOnAttach + _CablePullSlackDistance) - baseToMeshDist, 0.0f, FMath::Abs(CablePullSlackDistanceRange.Max),1.0f,0.0f);
	AdaptCableTense(_AlphaTense);
	
	//If can't Pull or Swing return
	if(!_bCablePowerPull || _bPlayerIsSwinging) return;

	//Init for the first time _LastAttachedActorLc
	if(_LastAttachedActorLoc.IsZero()) _LastAttachedActorLoc = _AttachedMesh->GetComponentLocation();

	//Setup ForceWeight value
	DetermineForceWeight(alpha);

	//Testing dist to lastLoc
	CheckingIfObjectIsBlocked();
	
	//Pull Object
	float currentPushAccel = _ForceWeight;
	
	//If attached determine additional Unblock push
	if(_bAttachObjectIsBlocked)
	{
		//---Using bound origin version---
		int i = 0;
		for(UCableComponent* cableAttachedElement : CableAttachedArray)
		{
			//Iteration exception General
			if(	i > (UnblockMaxIterationCount - 1)
				|| !CableAttachedArray.IsValidIndex(i)
				|| !IsValid(CableAttachedArray[i])
				|| _UnblockTimerTimerArray.IsValidIndex(i))
			{
				i++;
				continue;
			}

			//Iteration exception PointComp
			if(!CablePointComponents.IsValidIndex(i)
				|| !IsValid(CablePointComponents[i])
				|| !IsValid(CablePointComponents[0])
				|| CablePointComponents[0]->GetOwner() != CablePointComponents[i]->GetOwner())
			{
				i++;
				continue;
			}
			
			//Setup variable
			FVector origin, boxExtent;
			CablePointComponents[i]->GetOwner()->GetActorBounds(true,origin,boxExtent);
								
			if(bDebugPull) UE_LOG(LogTemp, Log, TEXT("%S :: Use additional trajectory (%f)"), __FUNCTION__, GetWorld()->GetTimeSeconds());
			
			//Setup start && endd loc

			FVector start = origin;
			start.Z =  cableAttachedElement->GetSocketLocation(SOCKET_CABLE_START).Z;
			FVector end = cableAttachedElement->GetSocketLocation(SOCKET_CABLE_END);

			//Inverse if in exception
			// const bool bInverse = false;
			// if(bInverse)
			// {
			// 	end = start;
			// 	start = cableAttachedElement->GetSocketLocation(SOCKET_CABLE_END);
			// }
			
			FRotator rotCable = UKismetMathLibrary::FindLookAtRotation(start, end);
			FVector pushDir = rotCable.Vector();
			pushDir.Z *= -1;
			//pushDir.Z = FMath::Abs(pushDir.Z);
					
			//Push accel by iteraction
			const float alphaUnblock = UKismetMathLibrary::MapRangeClamped(baseToMeshDist, 0, _DistanceOnAttach + _CablePullSlackDistance,0 ,1);
		 	currentPushAccel = FMath::Lerp(_ForceWeight, MaxForceWeight, alphaUnblock);
			// if(i > 0) currentPushAccel /= i;
			//currentPushAccel *= 1.5f;
			
			//Push one after other
			FTimerHandle unblockPushTimerHandle;
			FTimerDelegate unblockPushTimerDelegate;
			unblockPushTimerDelegate.BindUFunction(this, FName("OnPushTimerEndEventReceived"), unblockPushTimerHandle, pushDir, currentPushAccel);
			GetWorld()->GetTimerManager().SetTimer(unblockPushTimerHandle, unblockPushTimerDelegate, (i == 0 ? 1 : i) * UnblockPushLatency, false);
			_UnblockTimerTimerArray.AddUnique(unblockPushTimerHandle);
			
			if (bDebugPull)
			{
				DrawDebugDirectionalArrow(GetWorld(), start, start + pushDir * UKismetMathLibrary::Vector_Distance(start, end), 10.0f, FColor::Red, false, 0.5f, 10, 3);
			}

			//Increment 
			i++;
		}
	}
	
	//Default Pull Force
	FVector start = _AttachedMesh->GetComponentLocation();
	FVector end =  CableListArray[0]->GetSocketLocation(SOCKET_CABLE_START);
	FRotator rotMeshCable = UKismetMathLibrary::FindLookAtRotation(start,end);
	
	//Object isn't blocked add a random range offset
	rotMeshCable.Yaw = rotMeshCable.Yaw + UKismetMathLibrary::RandomFloatInRange(-PullingMaxRandomYawOffset, PullingMaxRandomYawOffset);
	if(_bAttachObjectIsBlocked) currentPushAccel = (currentPushAccel / UnblockDefaultPullforceDivider);
	
	FVector defaultNewVel = _AttachedMesh->GetMass() * rotMeshCable.Vector() * currentPushAccel;
	_AttachedMesh->AddImpulse((defaultNewVel * GetWorld()->DeltaRealTimeSeconds) * _PlayerCharacter->CustomTimeDilation, NAME_None, false);

	//Debug base Pull dir
	if(bDebugPull)
	{
		DrawDebugDirectionalArrow(GetWorld(), start, start + rotMeshCable.Vector() * UKismetMathLibrary::Vector_Distance(start, end) , 10.0f, FColor::Orange, false, 0.02f, 10, 3);
	}
	
}

void UPS_HookComponent::OnAttachedSameLocTimerEndEventReceived()
{
	if(!IsValid(_AttachedMesh)) return;
	
	_bAttachObjectIsBlocked = UKismetMathLibrary::Vector_Distance2DSquared(_LastAttachedActorLoc, _AttachedMesh->GetComponentLocation()) < AttachedMaxDistThreshold * AttachedMaxDistThreshold;
	if(bDebugPull) UE_LOG(LogTemp, Log, TEXT("%S :: _bAttachObjectIsBlocked %i"),__FUNCTION__, _bAttachObjectIsBlocked);
	
}

void UPS_HookComponent::OnPushTimerEndEventReceived(const FTimerHandle selfHandler, const FVector& currentPushDir, const float pushAccel)
{
	if(!IsValid(_AttachedMesh) || !IsValid(GetWorld())) return;
	
	if(bDebugPull) UE_LOG(LogTemp, Log, TEXT("%S (%f)"),__FUNCTION__,GetWorld()->GetTimeSeconds());
	
	FVector inverseNewVel = _AttachedMesh->GetMass() * currentPushDir * pushAccel;
	_AttachedMesh->AddImpulse((inverseNewVel * GetWorld()->DeltaRealTimeSeconds) * _PlayerCharacter->CustomTimeDilation,  NAME_None, false);

	if(selfHandler.IsValid())_UnblockTimerTimerArray.Remove(selfHandler);
}

#pragma endregion Pull

//------------------
#pragma endregion Grapple

#pragma region Swing
//------------------

void UPS_HookComponent::OnTriggerSwing(const bool bActivate)
{
	//if(bPlayerIsSwinging == bActivate) return;
	
	if (!IsValid(GetWorld()) || !IsValid(_PlayerCharacter->GetCharacterMovement()) || !IsValid(_PlayerCharacter) || !
		IsValid(_AttachedMesh)) return;

	
	if (bDebugSwing) UE_LOG(LogTemp, Warning, TEXT("%S :: bActivate %i, movemode %s "), __FUNCTION__, bActivate, *UEnum::GetValueAsString(_PlayerCharacter->GetCharacterMovement()->MovementMode));


	_bPlayerIsSwinging = bActivate;
	_SwingStartTimestamp = GetWorld()->GetTimeSeconds();
	
	//_PlayerController->SetCanMove(!bActivate);
	//_PlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	//_PlayerCharacter->GetCharacterMovement()->GravityScale = bActivate ? SwingGravityScale : _DefaultGravityScale;
	_PlayerCharacter->GetCharacterMovement()->AirControl = bActivate ? SwingMaxAirControl : _PlayerCharacter->GetDefaultAirControl();
	_PlayerCharacter->GetCharacterMovement()->BrakingDecelerationFalling = 400.0f;
	
	if (bActivate)
	{
		const bool bMustAttachtoLastPoint = CablePointComponents.IsValidIndex(0);
		
		ConstraintAttachSlave->SetMassOverrideInKg(NAME_None, _PlayerCharacter->GetCharacterMovement()->Mass);
		ConstraintAttachMaster->SetMassOverrideInKg(NAME_None, _AttachedMesh->GetMass());

		UCableComponent* cableToAdapt = IsValid(CableListArray.Last()) ? CableListArray.Last() : FirstCable;
		AActor* masterAttachActor = bMustAttachtoLastPoint ? CablePointComponents[0]->GetOwner() : _PlayerCharacter;
		FVector masterLoc = bMustAttachtoLastPoint && CableCapArray.IsValidIndex(0) ? CableCapArray[0]->GetComponentLocation() : _CurrentHookHitResult.Location;
		ConstraintAttachMaster->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

		if (IsValid(cableToAdapt) && IsValid(masterAttachActor))
		{
			//Activate constraint debug
			HookPhysicConstraint->bVisualizeComponent = bDebugSwing;

			//Setup Component constrainted
			HookPhysicConstraint->ConstraintActor1 = _PlayerCharacter;
			HookPhysicConstraint->ConstraintActor2 = masterAttachActor;

			HookPhysicConstraint->ComponentName1.ComponentName = FName(GetConstraintAttachSlave()->GetName());
			HookPhysicConstraint->ComponentName2.ComponentName = CablePointComponents.IsValidIndex(0) ?FName(CablePointComponents[0]->GetName()) : FName(GetConstraintAttachMaster()->GetName());
			//HookPhysicConstraint->ComponentName2.ComponentName = FName(ConstraintAttachMaster->GetName());

			//Set Linear Limit
			HookPhysicConstraint->SetLinearXLimit(LCM_Limited, _DistanceOnAttach);
			HookPhysicConstraint->SetLinearYLimit(LCM_Limited, _DistanceOnAttach);
			HookPhysicConstraint->SetLinearZLimit(LCM_Limited, _DistanceOnAttach + _CablePullSlackDistance);
			
			//Setup AttachMaster
			ConstraintAttachMaster->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
			if(bMustAttachtoLastPoint) ConstraintAttachMaster->AttachToComponent(CableCapArray[0], FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			ConstraintAttachMaster->SetWorldLocation(masterLoc);
			ConstraintAttachMaster->SetWorldRotation(FRotator::ZeroRotator);
			
			//Setup AttachSlave
			GetConstraintAttachSlave()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
			GetConstraintAttachSlave()->SetSimulatePhysics(true);
			GetConstraintAttachSlave()->SetWorldLocation(HookThrower->GetComponentLocation());
			
			//Setup hookconstraint Loc && Rot
			HookPhysicConstraint->SetWorldLocation(GetConstraintAttachMaster()->GetComponentLocation(), false);
			FTransform newTransform = FTransform(FRotator::ZeroRotator, GetConstraintAttachMaster()->GetComponentLocation(), FVector::OneVector);
			HookPhysicConstraint->SetConstraintReferenceFrame(EConstraintFrame::Frame1, newTransform);
			const FRotator newSlaveRot = UKismetMathLibrary::FindLookAtRotation(ConstraintAttachSlave->GetComponentLocation(), ConstraintAttachMaster->GetComponentLocation());
			const FRotator newMasterRot = UKismetMathLibrary::FindLookAtRotation(ConstraintAttachMaster->GetComponentLocation(), ConstraintAttachSlave->GetComponentLocation());
			HookPhysicConstraint->SetConstraintReferenceOrientation(EConstraintFrame::Frame1, newSlaveRot.Vector(), newMasterRot.Vector());

			//Init constraint
			HookPhysicConstraint->InitComponentConstraint();
			HookPhysicConstraint->UpdateConstraintFrames();

			//Impulse Slave for preserve player enter velocity
			_SwingImpulseForce = _PlayerCharacter->GetVelocity().Length() * SwingImpulseMultiplier;

			_DistOnAttachMasterConstraint = UKismetMathLibrary::Vector_Distance(ConstraintAttachMaster->GetComponentLocation(), ConstraintAttachSlave->GetComponentLocation());

			//TODO :: Chack if ImpulseConstraintAttach is still use
			//ImpulseConstraintAttach();
		}
	}
	else
	{
		FVector directionToCenterMass = GetConstraintAttachMaster()->GetComponentLocation() - GetConstraintAttachSlave()->GetComponentLocation();
		directionToCenterMass.Normalize();
		_SwingImpulseForce = GetConstraintAttachSlave()->GetComponentVelocity().Length() * SwingImpulseMultiplier;

		GetConstraintAttachMaster()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetConstraintAttachMaster()->AttachToComponent(HookThrower, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName(SOCKET_HOOK));

		GetConstraintAttachSlave()->SetSimulatePhysics(false);
		GetConstraintAttachSlave()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetConstraintAttachSlave()->AttachToComponent(HookThrower, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName(SOCKET_HOOK));

		HookPhysicConstraint->ConstraintActor1 = nullptr;
		HookPhysicConstraint->ConstraintActor2 = nullptr;

		HookPhysicConstraint->ComponentName1.ComponentName = FName("None");
		HookPhysicConstraint->ComponentName2.ComponentName = FName("None");

		HookPhysicConstraint->InitComponentConstraint();
		HookPhysicConstraint->UpdateConstraintFrames();

		_ForceWeight = 0.0f;

		//Set Player velocity to slave velocity
		_PlayerCharacter->GetCharacterMovement()->Velocity = ConstraintAttachSlave->GetComponentVelocity();

		//Launch if Stop swing by jump			
		if (_PlayerCharacter->GetCharacterMovement()->IsFalling())
		{
			FVector impulseDirection = UKismetMathLibrary::GetDirectionUnitVector(_SwingPlayerLastLoc, _PlayerCharacter->GetActorLocation());
			_PlayerCharacter->LaunchCharacter(impulseDirection * _SwingImpulseForce, true, true);

			if (bDebugSwing)
				DrawDebugDirectionalArrow(GetWorld(), _PlayerCharacter->GetActorLocation(), _PlayerCharacter->GetActorLocation() + impulseDirection * 500, 10.0f, FColor::Yellow, false, 2, 10, 3);
		}
	}
}

void UPS_HookComponent::SwingTick(const float deltaTime)
{
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetCharacterMovement()) || !IsValid(_AttachedMesh) || !CableListArray.IsValidIndex(0) || !IsValid(GetWorld())) return;
			
	//Activate Swing if not active
	if(!IsPlayerSwinging() && _PlayerCharacter->GetCharacterMovement()->IsFalling()&& _AttachedMesh->GetMass() > _PlayerCharacter->GetMesh()->GetMass())
	{
		OnTriggerSwing(true);
	}
	
	//Swing Tick
	if(_bPlayerIsSwinging)
	{
		OnSwingPhysic(deltaTime);
	}
	
	//Dettach if falling with his attached actor
	// FVector gravityVelocity = FVector(0.0f, 0.0f,  GetWorld()->GetGravityZ()) * GetWorld()->GetDeltaSeconds();;
	// if(CablePointLocations.Num() == 0 && _PlayerCharacter->GetCharacterMovement()->IsFalling() && AttachedMesh->)
	// {
	// 	DettachHook();
	// 	return;
	// }

}

void UPS_HookComponent::OnSwingPhysic(const float deltaTime)
{
	if(_PlayerCharacter->GetCharacterMovement()->IsMovingOnGround()
	  || _PlayerCharacter->GetParkourComponent()->IsWallRunning()
	  || _PlayerCharacter->GetParkourComponent()->IsLedging()
	  || _PlayerCharacter->GetParkourComponent()->IsMantling())
	{
		OnTriggerSwing(false);
		
		//----Auto break hook if velocity too low----
		// if(bCablePowerPull &&  _PlayerCharacter->GetVelocity().Length() <= _SwingStartVelocity.Length())
		// 	DettachHook();
		return;
	}

	//Check dist to MaxAuthorized during swing
	// float baseToMeshDist =	FMath::Abs(UKismetMathLibrary::Vector_Distance(HookThrower->GetComponentLocation(),_AttachedMesh->GetComponentLocation()));
	// if(baseToMeshDist  > (_DistanceOnAttach + _CablePullSlackDistance) * AutoBreakByDistMultiplicator)
	// {
	// 	DettachHook();
	// 	return;
	// }
	

	//Fake AirControl for influence swing
	//TODO :: Not working properly TO RETAKE
	 // if(!_PlayerController->GetMoveInput().IsNearlyZero())
	 // {
	 // 	FVector swingControlDir = UPSFl::GetWorldInputDirection(_PlayerCharacter->GetFirstPersonCameraComponent(), _PlayerController->GetMoveInput());
	 // 	swingControlDir.Normalize();
	 //
	 // 	float airControlSpeed = _PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed * _PlayerCharacter->GetCharacterMovement()->AirControl;
	 // 	FVector swingAirControlVel = swingControlDir * GetWorld()->DeltaTimeSeconds * airControlSpeed * SwingCustomAirControlMultiplier;
	 //
	 // 	GetConstraintAttachSlave()->AddForce(swingAirControlVel, NAME_None, true);
	 // }
	
	//Set actor location to ConstraintAttach
	FTimerDelegate setLastLocDel;
	FTimerHandle setLastLocHandle;
	setLastLocDel.BindUFunction(this,FName("SetSwingPlayerLastLoc"), GetConstraintAttachSlave()->GetComponentLocation());
	GetWorld()->GetTimerManager().SetTimer(setLastLocHandle,setLastLocDel,0.1,false);
	
	//Move physic constraint to match player position (attached element) && //Update constraint position on component
	const float lastIndex = CableCapArray.Num() - 1;
	
	//Update player to slave loc
	//TODO :: Determine good relative offset for HookThrower good position player take real position of socket
	const bool lastCablePointIsValid = CableCapArray.IsValidIndex(lastIndex);
	const FVector newPlayerLoc = ConstraintAttachSlave->GetComponentLocation()/*+ UKismetMathLibrary::InverseTransformLocation(_PlayerCharacter->GetTransform(),HookThrower->GetSocketLocation(FName(SOCKET_HOOK))) */;
	_PlayerCharacter->GetRootComponent()->SetWorldLocation(newPlayerLoc);

	//Update Attach and Constraint physic loc
	ConstraintAttachMaster->SetWorldLocation(lastCablePointIsValid ? CableCapArray[lastIndex]->GetComponentLocation() : _CurrentHookHitResult.Location);
	FVector constraintLoc = GetConstraintAttachMaster()->GetComponentLocation();
	HookPhysicConstraint->SetWorldLocation(constraintLoc, false);

	//Update Attach and Constraint physic rot
	const FRotator newSlaveRot = UKismetMathLibrary::FindLookAtRotation(ConstraintAttachSlave->GetComponentLocation(), ConstraintAttachMaster->GetComponentLocation());
	const FRotator newMasterRot = UKismetMathLibrary::FindLookAtRotation(ConstraintAttachMaster->GetComponentLocation(), ConstraintAttachSlave->GetComponentLocation());
	HookPhysicConstraint->SetConstraintReferenceOrientation(EConstraintFrame::Frame1, newSlaveRot.Vector(), newMasterRot.Vector());
	
	//Setup custom center of mass
	FVector newCenterOfMass = ConstraintAttachMaster->GetComponentLocation();
	newCenterOfMass.Z =	ConstraintAttachSlave->GetComponentLocation().Z;
	ConstraintAttachSlave->SetCenterOfMass(newCenterOfMass);
	
	//Winde
	if(_bCableWinderIsActive)
	{
		//Determine newZ smoothed
		const float distOnAttachWithRange = _DistanceOnAttach + _CablePullSlackDistance;
		// const float alpha = UKismetMathLibrary::MapRangeClamped(_CableWindeInputValue, -1.0f, 1.0f, 0.0f ,1.0f);
		const float newSwingZ = _CablePullSlackDistance;
		// const float newSwingZ = UKismetMathLibrary::Lerp(_DistanceOnAttach, _DistOnAttachMasterConstraint, alpha);
		
		//Linear Limit update
		float currentZLinearLimit = FMath::Lerp(_DistanceOnAttach, FMath::Clamp(distOnAttachWithRange, CablePullSlackDistanceRange.Min, _DistanceOnAttach + CablePullSlackDistanceRange.Max), _AlphaWinde);
		//float currentZLinearLimit = _DistanceOnAttach + _CablePullSlackDistance;
		HookPhysicConstraint->SetLinearZLimit(LCM_Limited,currentZLinearLimit);

		//Frame offset update
		//FTransform currentFrame = HookPhysicConstraint->ConstraintInstance.GetRefFrame(EConstraintFrame::Frame1);
		FTransform currentFrame = FTransform();

		 FVector newOffset = GetConstraintAttachMaster()->GetComponentLocation();
		 newOffset.Z =+ _CablePullSlackDistance;
		 currentFrame.SetLocation(newOffset);

		//TODO :: not working properly 
		//currentFrame.SetLocation(FVector(0.0f,0.0f,newSwingZ));
		currentFrame.SetScale3D(FVector(1.0f));
		
		HookPhysicConstraint->ConstraintInstance.SetRefFrame(EConstraintFrame::Frame1, currentFrame);

		DrawDebugPoint(GetWorld(), 	HookPhysicConstraint->ConstraintInstance.GetConstraintLocation(), 20.f, FColor::Orange, true);

		UE_LOG(LogTemp, Error, TEXT("%S :: newSwingZ %f, currentZLinearLimit %f, _CableWindeInputValue %f"),__FUNCTION__, newSwingZ, currentZLinearLimit, _CableWindeInputValue);
	}
	
	//Update constraint
	HookPhysicConstraint->UpdateConstraintFrames();
	
	//Debug
	if(bDebugSwing)
	{
		DrawDebugPoint(GetWorld(), GetConstraintAttachMaster()->GetComponentLocation(), 20.f, FColor::Red, true);
		UE_LOG(LogTemp, Error, TEXT("%S :: _CablePullSlackDistance %f, _DistanceOnAttach %f "),__FUNCTION__, _CablePullSlackDistance, _DistanceOnAttach);
	}
	

			
}

void UPS_HookComponent::ForceUpdateMasterConstraint()
{
	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S"),__FUNCTION__);
	const bool bMustAttachtoLastPoint = CablePointComponents.IsValidIndex(0);

	ConstraintAttachMaster->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	
	if(bMustAttachtoLastPoint) ConstraintAttachMaster->AttachToComponent(CableCapArray[0], FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	
	FVector masterLoc = bMustAttachtoLastPoint && CableCapArray.IsValidIndex(0) ? CableCapArray[0]->GetComponentLocation() : _CurrentHookHitResult.Location;
	ConstraintAttachMaster->SetWorldLocation(masterLoc);
	ConstraintAttachMaster->SetWorldRotation(FRotator::ZeroRotator);
	ConstraintAttachMaster->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);

	HookPhysicConstraint->UpdateConstraintFrames();
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

