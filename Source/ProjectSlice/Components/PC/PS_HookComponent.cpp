// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_HookComponent.h"

#include "PS_PlayerCameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "CableComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "ProjectSlice/Data/PS_TraceChannels.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialExpressionSkyAtmosphereLightIlluminance.h"
#include "ProjectSlice/Character/PC/PS_Character.h"
#include "ProjectSlice/Components/GPE/PS_SlicedComponent.h"
#include "ProjectSlice/Data/PS_Constants.h"
#include "ProjectSlice/FunctionLibrary/PSFl.h"
#include "ProjectSlice/FunctionLibrary/PSFL_GeometryScript.h"

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
		GetWorld()->GetTimerManager().SetTimer(_HighSubstepTickHandler, HighTimerDelegate, 1/540.0f, true);
	}
	
	//Constraint display
	GetConstraintAttachMaster()->SetVisibility(bDebugSwing);
	GetConstraintAttachSlave()->SetVisibility(bDebugSwing);
		
}

void UPS_HookComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	//Callback
	if(IsValid(_PlayerCharacter) && IsValid(_PlayerCharacter->GetSlowmoComponent()))
	{
		_PlayerCharacter->GetSlowmoComponent()->OnSlowmoEvent.RemoveDynamic(this, &UPS_HookComponent::OnSlowmoTriggerEventReceived);
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
	
	if(bDebugArm) UE_LOG(LogActorComponent, Log, TEXT("%S"), __FUNCTION__);

	//Set mesh with Arm
	_PlayerCharacter->GetPhysicAnimComponent()->SetSkeletalMeshComponent(GetHookThrower());
}

void UPS_HookComponent::ToggleArmPhysicalAnimation(const bool bActivate)
{
	if(!IsValid(_PhysicAnimComp) || !IsValid(HookThrower) || !IsValid(GetWorld())) return;

	if(bDebugArm) UE_LOG(LogActorComponent, Log, TEXT("%S :: bActivate %i"), __FUNCTION__, bActivate);
	
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
	
}

void UPS_HookComponent::ConfigLastAndSetupNewCable(UCableComponent* lastCable,const FSCableWrapParams& currentTraceCableWrap, UCableComponent*& newCable, const bool bReverseLoc) const
{
	//Init works Variables
	const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);
	
	//Attach Last Cable to Hitted Object, Set his position to it
	lastCable->AttachToComponent(currentTraceCableWrap.OutHit.GetComponent(), AttachmentRule);
	lastCable->SetWorldLocation(currentTraceCableWrap.OutHit.Location, false, nullptr,ETeleportType::TeleportPhysics);
	
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
	newCable->SetAttachEndToComponent(currentTraceCableWrap.OutHit.GetComponent());
	newCable->EndLocation = currentTraceCableWrap.OutHit.GetComponent()->GetComponentTransform().InverseTransformPosition(currentTraceCableWrap.OutHit.Location);
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
	if (_bWrappingByFirst) return;
	
	//-----Add Wrap Logic-----
	//Add By First
	const int32 firstIndex = 0;
	if(!CableListArray.IsValidIndex(firstIndex) || CableListArray.IsEmpty()) return;
	
	UCableComponent* lastCable = CableListArray[firstIndex];
	if(!IsValid(lastCable)) return;

	//Trace cable wrap
	FSCableWrapParams currentTraceCableWrap = FSCableWrapParams();
	constexpr bool bReverseLoc = true;
	TraceCableWrap(lastCable, bReverseLoc, currentTraceCableWrap);
	
	//If Trace Hit nothing or Invalid object return
	if (!currentTraceCableWrap.OutHit.bBlockingHit || !IsValid(currentTraceCableWrap.OutHit.GetComponent())) return;

	//If Location Already Exist return
	if (!CheckPointLocation(currentTraceCableWrap.OutHit.Location, CableWrapErrorTolerance)) return;

	//Create intermediate points
	 // if (CableCapArray.IsValidIndex(firstIndex))
	 // {
	 // 	GenerateIntermediatePoint(CableCapArray[firstIndex]->GetComponentLocation(), currentTraceCableWrap.OutHit.Location, currentTraceCableWrap, bReverseLoc);
	 // 	
	 // }else
	 // {
	 // 	//Create end new point
	 // 	CreateWrapPointByFirst(lastCable, currentTraceCableWrap);
	 // }
	
	//Create new point
	CreateWrapPointByFirst(lastCable, currentTraceCableWrap);
}

void UPS_HookComponent::CreateWrapPointByFirst(UCableComponent* const lastCable,const FSCableWrapParams& currentTraceCableWrap)
{
	//----Last Cable && New Points---
	//Add new Point Loc && Hitted Component to Array
	CablePointComponents.Insert(currentTraceCableWrap.OutHit.GetComponent(),0);
	CablePointUnwrapAlphaArray.Insert(0.0f,0);
		
	//Config lastCable And Setup newCable
	UCableComponent* newCable = nullptr;
	ConfigLastAndSetupNewCable(lastCable, currentTraceCableWrap, newCable, true);
	if (!IsValid(newCable))
	{
		UE_LOG(LogActorComponent, Error, TEXT("%S :: localNewCable Invalid"), __FUNCTION__);
		return;
	}

	_bWrappingByFirst = true;
	
	//----Caps Sphere---
	//Add Sphere on Caps
	if (bDebugCable) UE_LOG(LogTemp, Log, TEXT("%S :: AddSphereCaps"), __FUNCTION__);
	AddSphereCaps(currentTraceCableWrap, true);
	
	//Add latest cable to attached cables array && Add this new cable to "cable list array"
	if(CableAttachedArray.IsEmpty())
		CableAttachedArray.Add(newCable);
	else
		CableAttachedArray.Insert(newCable,1);
	CableListArray.Insert(newCable,1);

	//Attach New Cable to Hitted Object && Set his position to it
	if(!CableCapArray.IsValidIndex(1) || !CablePointComponents.IsValidIndex(1))
	{
		_bWrappingByFirst = false;
		return;
	}
	const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);
	newCable->SetWorldLocation(CableCapArray[1]->GetComponentLocation());
	newCable->AttachToComponent(CablePointComponents[1], AttachmentRule);
	
	//----Set New Cable Params identical to First Cable---
	if (bCableUseSharedSettings) ConfigCableToFirstCableSettings(newCable);

	_bWrappingByFirst = false;
}

void UPS_HookComponent::WrapCableAddByLast()
{
	if(_bArmIsRagdolled || _bWrappingByLast) return;
	
	int32 lastIndexCable = CableListArray.Num()-1;
	
	if(!CableListArray.IsValidIndex(lastIndexCable) || CableListArray.IsEmpty()) return;

	UCableComponent* lastCable = CableListArray[lastIndexCable];
	if(!IsValid(lastCable)) return;

	//Trace cable wrap
	FSCableWrapParams currentTraceCableWrap = FSCableWrapParams();
	constexpr bool bReverseLoc = false;
	TraceCableWrap(lastCable, bReverseLoc, currentTraceCableWrap);

	//If Trace Hit nothing or Invalid object return
	if (!currentTraceCableWrap.OutHit.bBlockingHit || !IsValid(currentTraceCableWrap.OutHit.GetComponent())) return;
	
	// Create intermediate points
	const int32 lastIndexCablePoint = CableCapArray.Num() - 1;
	if (CableCapArray.IsValidIndex(lastIndexCablePoint))
	{		
		GenerateIntermediatePoint(CableCapArray[lastIndexCablePoint]->GetComponentLocation(), currentTraceCableWrap.OutHit.Location, currentTraceCableWrap, bReverseLoc);
	}
	else if (lastIndexCablePoint < 0)
	{
		//Create end point
		CreateNewCablePointByLast(lastCable, currentTraceCableWrap);
	}
	
}

void UPS_HookComponent::CreateNewCablePointByLast(UCableComponent* const lastCable,const FSCableWrapParams& currentTraceCableWrap)
{
	//If Location Already Exist return
	if (!CheckPointLocation(currentTraceCableWrap.OutHit.Location, CableWrapErrorTolerance))
	{
		return;
	}
	
	//----Last Cable && New Points---
	//Add new Point Loc && Hitted Component to Array
	CableAttachedArray.Add(lastCable);
	CablePointComponents.Add(currentTraceCableWrap.OutHit.GetComponent());
	CablePointUnwrapAlphaArray.Add(0.0f);
	
	//Config lastCable And Setup newCable
	UCableComponent* newCable = nullptr;
	ConfigLastAndSetupNewCable(lastCable, currentTraceCableWrap, newCable, false);
	if (!IsValid(newCable))
	{
		UE_LOG(LogActorComponent, Error, TEXT("%S :: localNewCable Invalid"),__FUNCTION__);
		return;
	}

	_bWrappingByLast = true;
	
	//----Caps Sphere---
	//Add Sphere on Caps
	if (bDebugCable) UE_LOG(LogTemp, Log, TEXT("%S :: AddSphereCaps"), __FUNCTION__);
	AddSphereCaps(currentTraceCableWrap, false);
	
	//Add newCable to list
	CableListArray.Add(newCable);
	
	//Attach New Cable to Hitted Object && Set his position to it
	AttachCableToHookThrower(newCable);

	//----Set New Cable Params identical to First Cable---
	if (bCableUseSharedSettings) ConfigCableToFirstCableSettings(newCable);

	_bWrappingByLast = false;
	
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
		UE_LOG(LogActorComponent, Error, TEXT("%S :: CablePointUnwrapAlphaArray[%i] Invalid"),__FUNCTION__, CablePointUnwrapAlphaArray.Num()-1);
		return;
	}

	//Trace
	FHitResult outHit;
	if(TraceCableUnwrap(pastCable, currentCable, true, outHit))
	{
		if(outHit.bBlockingHit && !outHit.Location.Equals(outHit.TraceEnd, CableUnwrapErrorMultiplier))
		{
			CablePointUnwrapAlphaArray[0] = 0.0f;
			return;
		}
	}


	//----Custom tick-----
	//Unwrap with delay frames to prevent flickering of wrap/unwrap cycles.Basically increase point alpha value by 1 each frame, if it's more than custom value then process. Use subtle values for responsive unwrap.
	CablePointUnwrapAlphaArray[0] = CablePointUnwrapAlphaArray[0] + 1;
	if(CablePointUnwrapAlphaArray[0] < CableUnwrapFirstFrameDelay)
	{
		return;
	}
	
	//Destroy and remove Last Cable tick
	if(!CableListArray.IsValidIndex(1)
		|| !CableCapArray.IsValidIndex(0)) return;
	
	//If attached cables are more than 0, remove the second one (when first cable gets closer to it)
	CableAttachedArray.RemoveAt(CableAttachedArray.IsValidIndex(1) ? 1 : 0);
	
	//In any case, destroy the second cable (the first one is our main cable)
	CableListArray[1]->DestroyComponent();

	//Caps Sphere
	CableCapArray[0]->DestroyComponent();
	CableCapArray.RemoveAt(0);

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
		UE_LOG(LogActorComponent, Error, TEXT("%S :: CablePointUnwrapAlphaArray[%i] Invalid"),__FUNCTION__, CablePointUnwrapAlphaArray.Num()-1);
		return;
	}
	
	//Trace Unwrap
	FHitResult outHit;
	
	if(TraceCableUnwrap(pastCable, currentCable, false, outHit))
	{
		if(outHit.bBlockingHit && !outHit.Location.Equals(outHit.TraceEnd, CableUnwrapErrorMultiplier))
		{
			CablePointUnwrapAlphaArray[cablePointUnwrapAlphaLastIndex] = 0.0f;
			return;
		}

		_UnwrapLastLocation = outHit.Location;
	}
	
	//----Custom tick-----
	//Unwrap with delay frames to prevent flickering of wrap/unwrap cycles.Basically increase point alpha value by 1 each frame, if it's more than custom value then process. Use subtle values for responsive unwrap.
	CablePointUnwrapAlphaArray[cablePointUnwrapAlphaLastIndex] = CablePointUnwrapAlphaArray[cablePointUnwrapAlphaLastIndex] + 1;
	if (CablePointUnwrapAlphaArray[cablePointUnwrapAlphaLastIndex] < CableUnwrapLastFrameDelay)
		return;
		
	//----Destroy and remove Last Cable tick-----
	if(!CableAttachedArray.IsValidIndex(0)
		|| !CableAttachedArray.IsValidIndex(cableAttachedLastIndex)
		|| !CableListArray.IsValidIndex(cableListLastIndex)
		|| !CableCapArray.IsValidIndex(cablePointLocationsLastIndex)
		|| !CablePointComponents.IsValidIndex(cablePointLocationsLastIndex)
		|| !IsValid(CableCapArray[cablePointLocationsLastIndex])) return;
	
	//If attached cables are more than 0, remove the second one (when first cable gets closer to it)
	CableAttachedArray.RemoveAt(cableAttachedLastIndex);	

	//In any case, destroy the second cable (the first one is our main cable)
	CableListArray[cableListLastIndex]->DestroyComponent();

	//Caps Sphere	
	CableCapArray[cablePointLocationsLastIndex]->DestroyComponent();
	CableCapArray.RemoveAt(cablePointLocationsLastIndex);
	
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

bool UPS_HookComponent::TraceCableUnwrap(const UCableComponent* pastCable, const UCableComponent* currentCable, const bool& bReverseLoc, FHitResult& outHit) const
{
	const TArray<AActor*> actorsToIgnore = {GetOwner()};

	const FVector pastCableStartSocketLoc = bReverseLoc ? pastCable->GetSocketLocation(SOCKET_CABLE_END) : pastCable->GetSocketLocation(SOCKET_CABLE_START);
	const FVector pastCableEndSocketLoc = bReverseLoc ? pastCable->GetSocketLocation(SOCKET_CABLE_START) : pastCable->GetSocketLocation(SOCKET_CABLE_END);

	const FVector pastCableDirection = UKismetMathLibrary::GetForwardVector(UKismetMathLibrary::FindLookAtRotation(pastCableStartSocketLoc, pastCableEndSocketLoc));
	
	const FVector start = bReverseLoc ? currentCable->GetSocketLocation(SOCKET_CABLE_END) : currentCable->GetSocketLocation(SOCKET_CABLE_START);
	const FVector end = pastCableStartSocketLoc + pastCableDirection * CableUnwrapDistance;

	UKismetSystemLibrary::LineTraceSingle(GetWorld(), start, end, UEngineTypes::ConvertToTraceType(ECC_Rope),
		false, actorsToIgnore, false ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outHit, true, bReverseLoc ? FColor::Cyan : FColor::Blue, FLinearColor::Green, 0.01f);

	if (!outHit.bBlockingHit || !IsValid(outHit.GetActor())) return false;
	
	//Find the closest loc on the actor hit collision
	FVector outClosestPoint, outClosestTraceEnd;

	UPSFl::FindClosestPointOnActor(outHit.GetActor(),outHit.Location, outClosestPoint);
	if(!outClosestPoint.IsZero())
	{
		outHit.Location = outClosestPoint;
	}

	UPSFl::FindClosestPointOnActor(outHit.GetActor(),outHit.TraceEnd, outClosestTraceEnd);
	if(!outClosestTraceEnd.IsZero())
	{
		outHit.TraceEnd = outClosestTraceEnd;
	}
	
	return true;
}

void UPS_HookComponent::TraceCableWrap(const UCableComponent* cable, const bool bReverseLoc, FSCableWrapParams& outCableWarpParams) const
{
	if (!IsValid(cable) || !IsValid(GetWorld())) return;

	//If reverseLoc is true we trace Wrap by First
	UpdateCableWrapExtremityLoc(cable, bReverseLoc, outCableWarpParams);

	//Trace
	FHitResult  outHit;
	const TArray<AActor*> actorsToIgnore = {GetOwner()};
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), outCableWarpParams.CableStart, outCableWarpParams.CableEnd,
		UEngineTypes::ConvertToTraceType(ECC_Rope),
		true, actorsToIgnore, bDebugCable && !bReverseLoc ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None,
		outHit, true, bReverseLoc ? FColor::Magenta : FColor::Purple, FLinearColor::Green, 0.01f);
	
	outCableWarpParams.OutHit = outHit;
		
	//Find the closest loc on the actor hit collision
	// if (IsValid(outCableWarpParams.OutHit.GetActor()))
	// {		
		// FVector outClosestPoint;
		// UPSFl::FindClosestPointOnActor(outCableWarpParams.OutHit.GetActor(), outCableWarpParams.OutHit.Location, outClosestPoint);
		// if (!outClosestPoint.IsZero())
		// {
		// 	outCableWarpParams.OutHit.Location = outClosestPoint;
		// 	if (!bReverseLoc) DrawDebugPoint(GetWorld(), outClosestPoint, 20.f, FColor::Orange, false, 1.0f);
		// }
	// }
	
	//if (outHit.bBlockingHit) _PlayerController->SetPause(true);
}

void UPS_HookComponent::UpdateCableWrapExtremityLoc(const UCableComponent* cable, bool bReverseLoc,
	FSCableWrapParams& outCableWarpParams)
{
	FVector start = cable->GetSocketLocation(SOCKET_CABLE_START);
	FVector end = cable->GetSocketLocation(SOCKET_CABLE_END);

	outCableWarpParams.CableStart = bReverseLoc ? end : start;
	outCableWarpParams.CableEnd = bReverseLoc ? start : end;
}

void UPS_HookComponent::AddSphereCaps(const FSCableWrapParams& currentTraceParams, const bool bIsAddByFirst)
{	
	FVector rotatedCapTowardTarget = UKismetMathLibrary::GetUpVector(UKismetMathLibrary::FindLookAtRotation(currentTraceParams.CableStart,currentTraceParams.OutHit.Location));
	const FTransform& capsRelativeTransform = FTransform(rotatedCapTowardTarget.Rotation(),currentTraceParams.OutHit.Location,UKismetMathLibrary::Conv_DoubleToVector(FirstCable->CableWidth * CapsScaleMultiplicator));
	
	//Add new Cap Sphere (sphere size should be like 0.0105 of cable to fit)
	UStaticMeshComponent* newCapMesh = Cast<UStaticMeshComponent>(GetOwner()->AddComponentByClass(UStaticMeshComponent::StaticClass(), true, capsRelativeTransform,false));
	if (!IsValid(newCapMesh))
	{
		UE_LOG(LogActorComponent, Error, TEXT("PS_HookComponent :: newCapMesh Invalid"));
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

void UPS_HookComponent::GenerateIntermediatePoint(const FVector& lastPointLoc, const FVector& newPointLoc, FSCableWrapParams& currentTraceCableWrap, const bool bReverseLoc)
{
	UMeshComponent* meshComp = Cast<UMeshComponent>(currentTraceCableWrap.OutHit.GetComponent());
	if (!IsValid(meshComp)) return;

	//Compute Path
	TArray<FVector> outPoints;
	//UPSFL_GeometryScript::ComputeGeodesicPath(meshComp, newPointLoc, lastPointLoc, outPoints, false, bDebugGeodesic);
	UPSFL_GeometryScript::ComputeGeodesicPathWithVelocity(meshComp, newPointLoc, lastPointLoc, _PlayerCharacter->GetCapsuleVelocity(), outPoints, 0.5, false, bDebugGeodesic);
	
	if (outPoints.Num() == 	currentTraceCableWrap.outPoints.Num()) return;

	if (bDebugCable)
	{
		DrawDebugPoint(GetWorld(), lastPointLoc, 20.f,bReverseLoc ? FColor::Yellow : FColor::Orange, false, 1.0f);
		DrawDebugPoint(GetWorld(), newPointLoc, 20.f,bReverseLoc ? FColor::FromHex(FString("#990000")) : FColor::Red, false, 1.0f);
	}

	if (bDebugCable) UE_LOG(LogTemp, Log, TEXT("%S"), __FUNCTION__);
	
	//Geodesic intermediate points
	FSCableWrapParams intermediateCableWrapParams = currentTraceCableWrap;
	for (int i = outPoints.Num() - 1; i >= 0; i--)
	{
		FVector intermediatePoint = outPoints[i];
		
		//Update index and LastCable
		const int32 indexCable = bReverseLoc ? 0 : CableListArray.Num()-1;
		
		if(!CableListArray.IsValidIndex(indexCable)) return;
	
		UCableComponent* cable = CableListArray[indexCable];
	
		if(!IsValid(cable)) return;
		
		//Update wrap params
		UpdateCableWrapExtremityLoc(cable, bReverseLoc, intermediateCableWrapParams);
		intermediateCableWrapParams.OutHit.Location = intermediatePoint;
		
		//Create intermediate point
		if (bReverseLoc)
			CreateWrapPointByFirst(cable, intermediateCableWrapParams);
		else
			CreateNewCablePointByLast(cable, intermediateCableWrapParams);
		
	}
	currentTraceCableWrap.outPoints = outPoints;
	
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

		//UE_LOG(LogActorComponent, Error, TEXT("%S :: index %i, distBetCable %f, max %f min %f, newLenght %f, solverIterations %i"),__FUNCTION__, i,distBetCable, max,min, newLenght, newSolverIterations); 
		
		CableListArray[i]->CableLength = newLenght;

		//Iterate
		index++;
	}; 

	if (bDebugPull) UE_LOG(LogActorComponent, Log, TEXT("%S :: alphaTense %f"),__FUNCTION__, alphaTense); 
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

	if(bDebug) UE_LOG(LogActorComponent, Log, TEXT("%S"), __FUNCTION__);

	//Get Trace	
	const FVector start = HookThrower->GetSocketLocation(SOCKET_HOOK);
	const FVector target = _PlayerCharacter->GetWeaponComponent()->GetLaserTarget();
	
	const TArray<AActor*> actorsToIgnore = {_PlayerCharacter};
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), start, target, UEngineTypes::ConvertToTraceType(ECC_Slice),
		false, actorsToIgnore, EDrawDebugTrace::None, _CurrentHookHitResult, true);
		
	//If not blocking exit
	if(!_CurrentHookHitResult.bBlockingHit || !IsValid(_CurrentHookHitResult.GetActor()) || !IsValid(_CurrentHookHitResult.GetComponent()) || !_CurrentHookHitResult.GetComponent()->IsA(UMeshComponent::StaticClass()))
	{
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

	//Check if it's a destructible and use Chaos logic if it is;
	if(_CurrentHookHitResult.GetComponent()->IsA(UGeometryCollectionComponent::StaticClass()))
	{
		if(!IsValid(_ImpactField)) GenerateImpactField(_CurrentHookHitResult,  FVector::One());
	}
	//Else setup new attached component and collision
	else
	{
		_AttachedMesh->SetCollisionProfileName(Profile_GPE);
		_AttachedMesh->SetCollisionResponseToChannel(ECC_Rope,  ECollisionResponse::ECR_Ignore);
		_AttachedMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		_AttachedMesh->SetCollisionResponseToChannel(ECC_Rope, ECollisionResponse::ECR_Ignore);
		//TODO :: Need to define inertia conditioning to false;
		_AttachedMesh->SetLinearDamping(1.0f);
		_AttachedMesh->SetAngularDamping(1.0f);
		_AttachedMesh->WakeRigidBody();
	}
	_AttachedMesh->SetGenerateOverlapEvents(true);
	
	//Determine max distance for Pull
	_DistanceOnAttach = FMath::Abs(UKismetMathLibrary::Vector_Distance(_PlayerCharacter->GetActorLocation(), _AttachedMesh->GetComponentLocation()));
	
	//Callback
	OnHookObject.Broadcast(true);
}

void UPS_HookComponent::DettachHook()
{	
	//If FirstCable is not in CableList return
	if(!IsValid(FirstCable) || !IsValid(_AttachedMesh)) return;

	if(bDebug) UE_LOG(LogActorComponent, Log, TEXT("%S"), __FUNCTION__);

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

	//Chaos field system rest
	if (!GetWorld()->GetTimerManager().IsTimerActive(_ResetFieldTimerHandler))ResetImpactField();
		
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
	
	if (bDebugPull) UE_LOG(LogActorComponent, Log, TEXT("%S :: _CableWindeInputValue %f"), __FUNCTION__, _CableWindeInputValue);
	
}

void UPS_HookComponent::ResetWinde(const FInputActionInstance& inputActionInstance)
{
	if(bDebugPull) UE_LOG(LogActorComponent, Log, TEXT("%S"),__FUNCTION__);
	
	//On wheel axis change reset
	if (FMath::Sign(inputActionInstance.GetValue().Get<float>()) != FMath::Sign(_WindeInputAxis1DValue) && _bCableWinderIsActive)
	{
		ResetWindeHook();
	}
}

void UPS_HookComponent::ResetWindeHook()
{
	//Reset Input var
	_WindeInputAxis1DValue = WindeMaxInputWeight;
	_CableWindeInputValue = 0.0f;

	//ResetSlack distance to Minimum
	_CablePullSlackDistance = CablePullSlackMaxDistanceRange;

	//Reset Arm anim
	_PlayerCharacter->GetProceduralAnimComponent()->ApplyWindingVibration(0);

	//Reset Swing var
	_bCableWinderIsActive = false;
	_SwingWindeSmoothedOffsetZ = 0.0f;
	_SwingWindeLastOffsetZ = 0.0f;
	_AlphaWinde = 0.0f;
}


//------------------
#pragma endregion Winde

#pragma region Unblock
//------------------

void UPS_HookComponent::Unblock(FVector& outEnd)
{
	if(_bAttachObjectIsBlocked)
	{
		if (CableListArray.IsValidIndex(1) && CablePointComponents.IsValidIndex(1))
		{
			FVector outClosestPoint;
			UPSFl::FindClosestPointOnActor(CablePointComponents[1]->GetOwner(), _UnwrapLastLocation, outClosestPoint);
			if (!outClosestPoint.IsZero())
			{
				outEnd = outClosestPoint;
				DrawDebugPoint(GetWorld(), outClosestPoint, 10.f, FColor::Turquoise, false, 1.0f, 100);
			}
		}
	}

	//If attached determine additional Unblock push
	// if(_bAttachObjectIsBlocked)
	// {
	// 	//---Using bound origin version---
	// 	int i = 0;
	// 	for(UCableComponent* cableAttachedElement : CableAttachedArray)
	// 	{
	// 		//Iteration exception General
	// 		if(	i > (UnblockMaxIterationCount - 1)
	// 			|| !CableAttachedArray.IsValidIndex(i)
	// 			|| !IsValid(CableAttachedArray[i])
	// 			|| _UnblockTimerTimerArray.IsValidIndex(i))
	// 		{
	// 			i++;
	// 			continue;
	// 		}
	//
	// 		//Iteration exception PointComp
	// 		if(!CablePointComponents.IsValidIndex(i)
	// 			|| !IsValid(CablePointComponents[i])
	// 			|| !IsValid(CablePointComponents[0])
	// 			|| CablePointComponents[0]->GetOwner() != CablePointComponents[i]->GetOwner())
	// 		{
	// 			i++;
	// 			continue;
	// 		}
	// 		
	// 		//Setup variable
	// 		FVector origin, boxExtent;
	// 		CablePointComponents[i]->GetOwner()->GetActorBounds(true,origin,boxExtent);
	// 							
	// 		if(bDebugPull) UE_LOG(LogActorComponent, Log, TEXT("%S :: Use additional trajectory (%f)"), __FUNCTION__, GetWorld()->GetTimeSeconds());
	// 		
	// 		//Setup start && endd loc
	//
	// 		FVector start = origin;
	// 		start.Z =  cableAttachedElement->GetSocketLocation(SOCKET_CABLE_START).Z;
	// 		FVector end = cableAttachedElement->GetSocketLocation(SOCKET_CABLE_END);
	//
	// 		//Inverse if in exception
	// 		// const bool bInverse = false;
	// 		// if(bInverse)
	// 		// {
	// 		// 	end = start;
	// 		// 	start = cableAttachedElement->GetSocketLocation(SOCKET_CABLE_END);
	// 		// }
	// 		
	// 		FRotator rotCable = UKismetMathLibrary::FindLookAtRotation(start, end);
	// 		FVector pushDir = rotCable.Vector();
	// 		pushDir.Z *= -1;
	// 		//pushDir.Z = FMath::Abs(pushDir.Z);
	// 				
	// 		//Push accel by iteraction
	// 		const float alphaUnblock = UKismetMathLibrary::MapRangeClamped(baseToMeshDist, 0, _DistanceOnAttach + _CablePullSlackDistance,0 ,1);
	// 	 	currentPushAccel = FMath::Lerp(_ForceWeight, MaxForceWeight, alphaUnblock);
	// 		// if(i > 0) currentPushAccel /= i;
	// 		//currentPushAccel *= 1.5f;
	// 		
	// 		//Push one after other
	// 		FTimerHandle unblockPushTimerHandle;
	// 		FTimerDelegate unblockPushTimerDelegate;
	// 		unblockPushTimerDelegate.BindUFunction(this, FName("OnPushTimerEndEventReceived"), unblockPushTimerHandle, pushDir, currentPushAccel);
	// 		GetWorld()->GetTimerManager().SetTimer(unblockPushTimerHandle, unblockPushTimerDelegate, (i == 0 ? 1 : i) * UnblockPushLatency, false);
	// 		_UnblockTimerTimerArray.AddUnique(unblockPushTimerHandle);
	// 		
	// 		if (bDebugPull)
	// 		{
	// 			DrawDebugDirectionalArrow(GetWorld(), start, start + pushDir * UKismetMathLibrary::Vector_Distance(start, end), 10.0f, FColor::Red, false, 0.5f, 10, 3);
	// 		}
	//
	// 		//Increment 
	// 		i++;
	// 	}
	// }
}

void UPS_HookComponent::CheckingIfObjectIsBlocked()
{
	if(!GetWorld()->GetTimerManager().IsTimerActive(_AttachedAtSameLocTimer))
	{
		if(UKismetMathLibrary::Vector_Distance2DSquared(_LastAttachedActorLoc, _AttachedMesh->GetComponentLocation()) < AttachedMaxDistThreshold * AttachedMaxDistThreshold)
		{
			if(bDebugPull) UE_LOG(LogActorComponent, Log, TEXT("%S :: AttachedObject is under Dist2D Threshold"), __FUNCTION__);
			
			FTimerDelegate timerDelegate;
			timerDelegate.BindUObject(this, &UPS_HookComponent::OnAttachedSameLocTimerEndEventReceived);
			GetWorld()->GetTimerManager().SetTimer(_AttachedAtSameLocTimer, timerDelegate, AttachedSameLocMaxDuration, false);
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

void UPS_HookComponent::OnAttachedSameLocTimerEndEventReceived()
{
	if(!IsValid(_AttachedMesh)) return;

	_bAttachObjectIsBlocked = UKismetMathLibrary::Vector_Distance2DSquared(_LastAttachedActorLoc, _AttachedMesh->GetComponentLocation()) < AttachedMaxDistThreshold * AttachedMaxDistThreshold;
	// const bool newbAttachObjectIsBlocked = UKismetMathLibrary::Vector_Distance2DSquared(_LastAttachedActorLoc, _AttachedMesh->GetComponentLocation()) < AttachedMaxDistThreshold * AttachedMaxDistThreshold;
	// if (_bAttachObjectIsBlocked == newbAttachObjectIsBlocked) _AttachedAtSameLocTimerCount++;
	// else _AttachedAtSameLocTimerCount = 0;
	//
	// if (_AttachedAtSameLocTimerCount > UnblockMaxIterationCount)
	// {
	// 	_bAttachObjectIsBlocked = false;
	// 	_AttachedAtSameLocTimerCount = 0;
	// }
	// else
	// {
	// 	_bAttachObjectIsBlocked = newbAttachObjectIsBlocked;
	// }
	
		
	if(bDebugPull) UE_LOG(LogActorComponent, Log, TEXT("%S :: _bAttachObjectIsBlocked %i, _AttachedAtSameLocTimerCount %i"),__FUNCTION__, _bAttachObjectIsBlocked, _AttachedAtSameLocTimerCount);
	
}

//------------------
#pragma endregion Unblock

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
	//Winde Alpha
	if(_CableWindeInputValue != 0.0f && FMath::Abs(_CableWindeInputValue) != _AlphaWinde)
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
		_CablePullSlackDistance = FMath::Lerp(0.0f, CablePullSlackMaxDistanceRange, _AlphaWinde) * FMath::Sign(_CableWindeInputValue);			
	}
	
	//Override _DistanceOnAttach
	if(_DistanceOnAttach + _CablePullSlackDistance < _DistanceOnAttach - CablePullSlackMaxDistanceRange
		&& FMath::Sign(_DistanceOnAttach - CablePullSlackMaxDistanceRange) == FMath::Sign(_DistanceOnAttach + _CablePullSlackDistance)
		&& !_bPlayerIsSwinging)
	{
		_DistanceOnAttach = baseToMeshDist;
	}
	
	//Distance On Attach By point number weight
	const float max = FMath::Max(_DistanceOnAttach + CablePullSlackMaxDistanceRange, _DistanceOnAttach - CablePullSlackMaxDistanceRange);
	float distanceOnAttachByTensorWeight = CableCapArray.Num() > 1 ? FMath::Clamp(UKismetMathLibrary::SafeDivide(_DistanceOnAttach + _CablePullSlackDistance, CableCapArray.Num()), 0.0f, max) : 0.0f;
	
	//Calculate Pull alpha && activate pull
	const float alpha = UKismetMathLibrary::MapRangeClamped(baseToMeshDist + distanceOnAttachByTensorWeight, 0, max, 0 ,1);
	_bCablePowerPull = baseToMeshDist + distanceOnAttachByTensorWeight > _DistanceOnAttach + _CablePullSlackDistance;

	//Debug
	if(bDebugPull) UE_LOG(LogActorComponent, Log, TEXT("%S :: baseToMeshDist %f, _DistanceOnAttach %f, _DistOnAttachWithRange %f, distanceOnAttachByTensorWeight %f, alpha %f"),__FUNCTION__, baseToMeshDist, _DistanceOnAttach, _DistanceOnAttach + _CablePullSlackDistance, distanceOnAttachByTensorWeight, alpha);

	//Out
	return alpha; 
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
		|| !CableListArray.IsValidIndex(0)
		|| !CableListArray.IsValidIndex(CableListArray.Num() - 1)
		|| !IsValid(GetWorld()))
		return;

	//Determine Alphas
	//Current dist to attach loc
	float baseToMeshDist = FMath::Abs(UKismetMathLibrary::Vector_Distance(HookThrower->GetSocketLocation(SOCKET_HOOK), CableListArray[CableListArray.Num() - 1]->GetSocketLocation(SOCKET_CABLE_END)));
	
	//Calculate current pull alpha (Winde && Distance Pull)
	_AlphaPull = CalculatePullAlpha(baseToMeshDist);

	//Try Auto Break Rope if tense is too high else Adapt Cable tense render
	_AlphaTense = UKismetMathLibrary::MapRangeClamped((_DistanceOnAttach + _CablePullSlackDistance) - baseToMeshDist, -CablePullSlackMaxDistanceRange, CablePullSlackMaxDistanceRange,1.0f,0.0f);
	AdaptCableTense(_AlphaTense);

	//Check if it's destructible and use Chaos logic if it is
	if(_bCanMoveField)
	{
		UpdateImpactField();
		return;
	}
	
	//Check if we can start default Pull logic
	//Check if object using physic
	if (!_AttachedMesh->IsSimulatingPhysics()) return;
	
	//If can't Pull or Swing return
	if(!_bCablePowerPull || _bPlayerIsSwinging) return;

	//Init for the first time _LastAttachedActorLc
	if(_LastAttachedActorLoc.IsZero()) _LastAttachedActorLoc = _AttachedMesh->GetComponentLocation();

	//Setup ForceWeight value
	DetermineForceWeight(_AlphaPull);

	//Testing dist to lastLoc
	CheckingIfObjectIsBlocked();
	
	//Pull Object
	// const float radiusSquared = _AttachedMesh->GetLocalBounds().ComputeSquaredDistanceFromBoxToPoint(_AttachedMesh->GetComponentLocation());
	// UE_LOG(LogTemp, Error, TEXT("radius %f"), FMath::Sqrt(radiusSquared));
	float currentPushAccel = _ForceWeight;
	
	//Pull direction calculation 
	FVector start = CableListArray[0]->GetSocketLocation(SOCKET_CABLE_END);
	FVector end =  CableListArray[0]->GetSocketLocation(SOCKET_CABLE_START);

	//Blocked Pull Force
	//Unblock(end);
	
	//Default Pull Force
	FRotator rotMeshCable = UKismetMathLibrary::FindLookAtRotation(start,end);		
	
	//Object isn't blocked add a random range offset
	rotMeshCable.Yaw = rotMeshCable.Yaw + UKismetMathLibrary::RandomFloatInRange(-PullingMaxRandomYawOffset, PullingMaxRandomYawOffset);
	//if(_bAttachObjectIsBlocked) currentPushAccel = (currentPushAccel / UnblockDefaultPullforceDivider);
	
	FVector defaultNewVel = _AttachedMesh->GetMass() *  rotMeshCable.Vector() * currentPushAccel;
	_AttachedMesh->AddImpulse((defaultNewVel * GetWorld()->DeltaRealTimeSeconds) * _PlayerCharacter->CustomTimeDilation, NAME_None, false);

	//Debug base Pull dir
	if(bDebugPull)
	{
		DrawDebugDirectionalArrow(GetWorld(), start, start + rotMeshCable.Vector() * UKismetMathLibrary::Vector_Distance(start, end) , 10.0f, _bAttachObjectIsBlocked ? FColor::Red : FColor::Orange, false, 0.02f, 10, 3);
	}
	
}

void UPS_HookComponent::OnPushTimerEndEventReceived(const FTimerHandle selfHandler, const FVector& currentPushDir, const float pushAccel)
{
	if(!IsValid(_AttachedMesh) || !IsValid(GetWorld())) return;
	
	if(bDebugPull) UE_LOG(LogActorComponent, Log, TEXT("%S (%f)"),__FUNCTION__,GetWorld()->GetTimeSeconds());
	
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
	
	if (bDebugSwing) UE_LOG(LogActorComponent, Log, TEXT("%S :: bActivate %i, movemode %s "), __FUNCTION__, bActivate, *UEnum::GetValueAsString(_PlayerCharacter->GetCharacterMovement()->MovementMode));


	_bPlayerIsSwinging = bActivate;
	
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
		
		HookPhysicConstraint->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

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
			const float limit = _DistanceOnAttach + _CablePullSlackDistance;
			HookPhysicConstraint->SetLinearXLimit(LCM_Limited, limit);
			HookPhysicConstraint->SetLinearYLimit(LCM_Limited, limit);
			HookPhysicConstraint->SetLinearZLimit(LCM_Limited, limit);
			
			//Setup AttachMaster
			ConstraintAttachMaster->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
			if(bMustAttachtoLastPoint) ConstraintAttachMaster->AttachToComponent(CableCapArray[0], FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			ConstraintAttachMaster->SetWorldLocation(masterLoc);
			ConstraintAttachMaster->SetWorldRotation(FRotator::ZeroRotator);

			//Setup AttachSlave
			ConstraintAttachSlave->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
			ConstraintAttachSlave->SetSimulatePhysics(true);
			ConstraintAttachSlave->SetWorldLocation(HookThrower->GetComponentLocation());
			
			//Setup hookconstraint Loc && Rot
			if(bMustAttachtoLastPoint) HookPhysicConstraint->AttachToComponent(CableCapArray[0], FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			HookPhysicConstraint->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.0f)));
			HookPhysicConstraint->SetWorldLocation(GetConstraintAttachMaster()->GetComponentLocation(), false);
			
			
			//Init constraint
			HookPhysicConstraint->InitComponentConstraint();
			HookPhysicConstraint->UpdateConstraintFrames();

			//Impulse Slave for preserve player enter velocity
			ImpulseConstraintAttach();

			//Debug
			DrawDebugSphere(GetWorld(), masterLoc, limit , 10.f, FColor::Green, false, 2.0f);
			
		}
	}
	else
	{
		FVector directionToCenterMass = ConstraintAttachSlave->GetComponentLocation() - ConstraintAttachSlave->GetComponentLocation();
		directionToCenterMass.Normalize();

		ConstraintAttachMaster->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ConstraintAttachMaster->AttachToComponent(HookThrower, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName(SOCKET_HOOK));

		ConstraintAttachSlave->SetSimulatePhysics(false);
		ConstraintAttachSlave->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ConstraintAttachSlave->AttachToComponent(HookThrower, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName(SOCKET_HOOK));

		HookPhysicConstraint->ConstraintActor1 = nullptr;
		HookPhysicConstraint->ConstraintActor2 = nullptr;
		HookPhysicConstraint->AttachToComponent(HookThrower, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName(SOCKET_HOOK));
		HookPhysicConstraint->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.0f)));
		HookPhysicConstraint->ComponentName1.ComponentName = FName("None");
		HookPhysicConstraint->ComponentName2.ComponentName = FName("None");

		HookPhysicConstraint->InitComponentConstraint();
		HookPhysicConstraint->UpdateConstraintFrames();

		_ForceWeight = 0.0f;

		//Set Player velocity to slave velocity
		_PlayerCharacter->GetCharacterMovement()->Velocity = _PlayerCharacter->GetCapsuleVelocity();

		//Launch if Stop swing by jump			
		if (_PlayerCharacter->GetCharacterMovement()->IsFalling())
		{
			FVector vel = (_PlayerCharacter->GetCapsuleVelocity() * _PlayerCharacter->CustomTimeDilation).GetClampedToSize(0.0f, _PlayerCharacter->GetCharacterMovement()->GetMaxSpeed());;
			_PlayerCharacter->LaunchCharacter(vel, false, false);
		
			if (bDebugSwing)
				DrawDebugDirectionalArrow(GetWorld(), _PlayerCharacter->GetActorLocation(), _PlayerCharacter->GetActorLocation() + _PlayerCharacter->GetCapsuleVelocity(), 10.0f, FColor::Yellow, false, 2, 10, 3);
		}
	}
}

void UPS_HookComponent::SwingTick(const float deltaTime)
{
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetCharacterMovement()) || !IsValid(_AttachedMesh) || !CableListArray.IsValidIndex(0) || !IsValid(GetWorld())) return;
			
	//Activate Swing if not active
	if(!IsPlayerSwinging() && _PlayerCharacter->GetCharacterMovement()->IsFalling() && _AttachedMesh->GetMass() > _PlayerCharacter->GetMesh()->GetMass())
	{
		OnTriggerSwing(true);
	}
	
	//Swing Tick
	if(_bPlayerIsSwinging)
	{
		OnSwingPhysic(deltaTime);
	}
	
}

void UPS_HookComponent::OnSwingPhysic(const float deltaTime)
{
	if(_PlayerCharacter->GetCharacterMovement()->IsMovingOnGround()
	  || _PlayerCharacter->GetParkourComponent()->IsWallRunning()
	  || _PlayerCharacter->GetParkourComponent()->IsLedging()
	  || _PlayerCharacter->GetParkourComponent()->IsMantling()
	  || !HookPhysicConstraint->ConstraintInstance.IsValidConstraintInstance())
	{
		OnTriggerSwing(false);
		
		//----Auto break hook if velocity too low----
		// if(bCablePowerPull &&  _PlayerCharacter->GetVelocity().Length() <= _SwingStartVelocity.Length())
		// 	DettachHook();
		return;
	}

	//Setup work var
	const float distOnAttachWithRange = _DistanceOnAttach + _CablePullSlackDistance;
	const FVector dirToMaster = UKismetMathLibrary::GetDirectionUnitVector(ConstraintAttachSlave->GetComponentLocation(), ConstraintAttachMaster->GetComponentLocation());
	const FVector dirToConstInst = UKismetMathLibrary::GetDirectionUnitVector(ConstraintAttachMaster->GetComponentLocation(), HookPhysicConstraint->ConstraintInstance.GetConstraintLocation());
	const float distBetConstraintComp = UKismetMathLibrary::Vector_Distance(ConstraintAttachMaster->GetComponentLocation(), ConstraintAttachSlave->GetComponentLocation());

	//Check dist to MaxAuthorized during swing
	// if(UKismetMathLibrary::Vector_Distance(ConstraintAttachMaster->GetComponentLocation(), ConstraintAttachSlave->GetComponentLocation()) > FMath::Max(_DistanceOnAttach - CablePullSlackDistanceRange.Max, _DistanceOnAttach + CablePullSlackDistanceRange.Max) + OffsetToMaximumDistAuthorized)
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
	 // Maybe usefull for air control HookPhysicConstraint->GetConstraintForce()
	 // }
		
	//Move physic constraint to match player position (attached element) && //Update constraint position on component
	const float lastIndex = CableCapArray.Num() - 1;

	//Update Attach and Constraint physic loc
	const bool lastCablePointIsValid = CableCapArray.IsValidIndex(lastIndex);
	ConstraintAttachMaster->SetWorldLocation(lastCablePointIsValid ? CableCapArray[lastIndex]->GetComponentLocation() : _CurrentHookHitResult.Location);
	FVector constraintLoc = GetConstraintAttachMaster()->GetComponentLocation();
	if(!FMath::IsNearlyEqual(HookPhysicConstraint->GetComponentLocation().Length(),constraintLoc.Length())) HookPhysicConstraint->SetWorldLocation(constraintLoc, false);

	//Update player to slave loc
	float distMasterToConstraintInstLoc = FMath::Sqrt(UKismetMathLibrary::Vector_DistanceSquared(ConstraintAttachMaster->GetComponentLocation(), HookPhysicConstraint->ConstraintInstance.GetConstraintLocation()));
	const float switchLocAlpha = distOnAttachWithRange < 0 ? 1.0f : UKismetMathLibrary::MapRangeClamped(distMasterToConstraintInstLoc, 0.0f, distOnAttachWithRange / 2.0f, 0.0f, 1.0f);
	const float targetDist = UKismetMathLibrary::FInterpTo(_SwingLastDistOnAttachWithRange, distOnAttachWithRange < 0 ? 0.0f : distOnAttachWithRange, deltaTime, SwingWindeTargetLocInterpSpeed);

	FVector newPlayerLoc = FMath::Lerp(ConstraintAttachSlave->GetComponentLocation(), HookPhysicConstraint->ConstraintInstance.GetConstraintLocation() + dirToConstInst * targetDist, switchLocAlpha);
	_PlayerCharacter->SetActorLocation(newPlayerLoc);
	DrawDebugPoint(GetWorld(), HookPhysicConstraint->ConstraintInstance.GetConstraintLocation() + dirToConstInst * distOnAttachWithRange, 20.f, FColor::Cyan, false, 1.0f);
		
	//Winde
	if(_bCableWinderIsActive)
	{ 
		//Setup var
		const float offsetZ = (distOnAttachWithRange - _SwingLastDistOnAttachWithRange) * -1;

		//Interp Offset Move
		_SwingWindeSmoothedOffsetZ = UKismetMathLibrary::FInterpTo(_SwingWindeSmoothedOffsetZ, _SwingWindeLastOffsetZ, deltaTime, SwingWindeOffsetInterpSpeed);
		if(!FMath::IsNearlyEqual(_SwingWindeSmoothedOffsetZ, _SwingWindeLastOffsetZ, 1.0f))
		{
			const FVector newLocAfterWinde = UKismetMathLibrary::VInterpTo(ConstraintAttachSlave->GetComponentLocation(), ConstraintAttachSlave->GetComponentLocation() + dirToMaster * _SwingWindeSmoothedOffsetZ, deltaTime, SwingWindeOffsetInterpSpeed);
			ConstraintAttachSlave->SetWorldLocation(newLocAfterWinde);
			
			if(bDebugSwing)UE_LOG(LogActorComponent,Log, TEXT("%S :: _SwingWindeLastOffsetZ %f, offsetZ %f, smoothedOffset %f"),__FUNCTION__, _SwingWindeLastOffsetZ, offsetZ, _SwingWindeSmoothedOffsetZ);
		}

		//Feedback to movement 
		if(offsetZ != 0.0f)
		{
			//Move slave constraint comp
			//const FVector newLocAfterWinde = UKismetMathLibrary::VInterpTo(ConstraintAttachSlave->GetComponentLocation(), ConstraintAttachSlave->GetComponentLocation() + dirToMaster * offsetZ, deltaTime, SwingWindeLocInterpSpeed);
			//ConstraintAttachSlave->SetWorldLocation(newLocAfterWinde);
			//ConstraintAttachSlave->SetWorldLocation(ConstraintAttachSlave->GetComponentLocation() + dirToMaster * offsetZ);
			//ConstraintAttachSlave->AddWorldOffset(dirToMaster * offsetZ);
			
			//Impulse
			const FVector windeVel = (_PlayerCharacter->GetCapsuleVelocity().GetSafeNormal() + dirToMaster) * offsetZ;
			const FVector gravityVel = _PlayerCharacter->GetGravityDirection().GetSafeNormal() * _PlayerCharacter->GetCharacterMovement()->GetGravityZ();
			ConstraintAttachSlave->AddImpulse(windeVel + gravityVel, NAME_None, false);

			if(bDebugSwing) UE_LOG(LogActorComponent,Log, TEXT("%S :: windeVel %f, gravityVel %f"),__FUNCTION__, windeVel.Length(), gravityVel.Length());
			DrawDebugDirectionalArrow(GetWorld(), ConstraintAttachSlave->GetComponentLocation(),  ConstraintAttachSlave->GetComponentLocation() - windeVel, 5.0f, FColor::Blue, false, 2, 10, 2);
			DrawDebugDirectionalArrow(GetWorld(), ConstraintAttachSlave->GetComponentLocation(),  ConstraintAttachSlave->GetComponentLocation() - gravityVel, 5.0f, FColor::Cyan, false, 2, 10, 2);
			DrawDebugDirectionalArrow(GetWorld(), ConstraintAttachSlave->GetComponentLocation(),  ConstraintAttachSlave->GetComponentLocation() - (windeVel + gravityVel), 5.0f, FColor::Purple, false, 2, 10, 2);
			
			//Update LinearLimit
			//const float newLimit = FMath::Abs(UKismetMathLibrary::Vector_Distance(ConstraintAttachMaster->GetComponentLocation(), ConstraintAttachSlave->GetComponentLocation()));
			const float newLimit = distMasterToConstraintInstLoc;
			HookPhysicConstraint->SetLinearXLimit(LCM_Limited, newLimit);
			HookPhysicConstraint->SetLinearYLimit(LCM_Limited, newLimit);
			HookPhysicConstraint->SetLinearZLimit(LCM_Limited, newLimit);

			//Stock last non zero offsetZ
			_SwingWindeLastOffsetZ = offsetZ;
		}
	}

	//Debug
	if(bDebugSwing)
	{
		//DrawDebugDirectionalArrow(GetWorld(), ConstraintAttachSlave->GetComponentLocation() ,ConstraintAttachSlave->GetComponentLocation() + dirToMaster * distOnAttachWithRange * _CableWindeInputValue , 5.0f, FColor::Blue, false, 0.2f, 10, 2);
		DrawDebugPoint(GetWorld(), HookPhysicConstraint->GetComponentLocation(), 30.f, FColor::Red, false, 1.0f);
		DrawDebugPoint(GetWorld(), HookPhysicConstraint->ConstraintInstance.GetConstraintLocation(), 20.f, FColor::Orange, false, 1.0f, 10.0f);
		DrawDebugPoint(GetWorld(), ConstraintAttachSlave->GetComponentLocation(), 22.f, FColor::Yellow, false, 1.0f);
		DrawDebugPoint(GetWorld(), HookPhysicConstraint->ConstraintInstance.GetConstraintLocation() + dirToConstInst * distOnAttachWithRange, 23.f, FColor::Blue, false, 1.0f);
		UE_LOG(LogActorComponent, Log, TEXT("%S :: distBetConstraintComp %f, distMasterToConstraintInstLoc %f, distOnAttachWithRange %f, switchLocAlpha %f"),__FUNCTION__, distBetConstraintComp, distMasterToConstraintInstLoc, distOnAttachWithRange, switchLocAlpha);
	}

	//Setyp last var
	_SwingLastDistOnAttachWithRange = distOnAttachWithRange;
	
	//Update constraint
	HookPhysicConstraint->UpdateConstraintFrames();
			
}

void UPS_HookComponent::ForceUpdateMasterConstraint()
{
	if(bDebugSwing) UE_LOG(LogActorComponent, Log, TEXT("%S"),__FUNCTION__);
	const bool bMustAttachtoLastPoint = CablePointComponents.IsValidIndex(0);
	_bUpdateMasterContraintByTime = !bMustAttachtoLastPoint;
	
	//Attachment
	ConstraintAttachMaster->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	HookPhysicConstraint->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	
	if(bMustAttachtoLastPoint)
	{
		ConstraintAttachMaster->AttachToComponent(CableCapArray[0], FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		HookPhysicConstraint->AttachToComponent(CableCapArray[0], FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}

	//Update dist && Move
	FVector masterLoc = bMustAttachtoLastPoint && CableCapArray.IsValidIndex(0) ? CableCapArray[0]->GetComponentLocation() : _CurrentHookHitResult.Location;
	UpdateMasterConstraint(masterLoc);

	//Activate Custom tick Update
	if(_bUpdateMasterContraintByTime)
	{
		FTimerHandle timerHandle;
		FTimerDelegate timerDelegate;

		timerDelegate.BindUFunction(this, FName("UpdateMasterConstraint"),masterLoc, GetWorld()->GetDeltaSeconds());
		GetWorld()->GetTimerManager().SetTimer(timerHandle, timerDelegate, UpdateMasterConstraintRate, true);
	}
}

void UPS_HookComponent::UpdateMasterConstraint(const FVector& targetLoc, const float deltaTime)
{
	FVector newLoc;
	if(deltaTime == 0.0f) newLoc = targetLoc;
	else newLoc = UKismetMathLibrary::VInterpTo(ConstraintAttachMaster->GetComponentLocation(), targetLoc, deltaTime, UpdateMasterConstrainInterpSpeed);
	
	ConstraintAttachMaster->SetWorldLocation(newLoc);
	ConstraintAttachMaster->SetWorldRotation(FRotator::ZeroRotator);
	ConstraintAttachMaster->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);

	HookPhysicConstraint->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.0f)));
	HookPhysicConstraint->SetWorldLocation(newLoc);
	HookPhysicConstraint->UpdateConstraintFrames();

	_bUpdateMasterContraintByTime = !FMath::IsNearlyZero(UKismetMathLibrary::Vector_DistanceSquared(newLoc, targetLoc), 0.2);
}

void UPS_HookComponent::ImpulseConstraintAttach() const
{
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetCharacterMovement()) || _PlayerCharacter->GetCharacterMovement()->MovementMode == MOVE_Walking) return;
	GetConstraintAttachSlave()->AddImpulse(_PlayerCharacter->GetCapsuleVelocity() * _PlayerCharacter->CustomTimeDilation,NAME_None, true);
	
	if(bDebugSwing) UE_LOG(LogActorComponent, Warning, TEXT("%S"), __FUNCTION__);
}

//------------------
#pragma endregion Swing

#pragma region Destruction
//------------------

#pragma region CanGenerateImpactField
//------------------
void UPS_HookComponent::GenerateImpactField(const FHitResult& targetHit, const FVector extent)
{
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(GetWorld()) || !targetHit.bBlockingHit) return;

	//Check geometrycollection validity && stock it
	UGeometryCollectionComponent* currentChaosComponent = Cast<UGeometryCollectionComponent>(targetHit.GetComponent());
	if (!IsValid(currentChaosComponent) || !targetHit.GetComponent()->GetPhysicsLinearVelocity().IsNearlyZero()) return;
	_CurrentGeometryCollection = currentChaosComponent;
		
	if(!IsValid(FieldSystemActor.Get()))
	{
		UE_LOG(LogActorComponent, Warning, TEXT("%S :: SpawnActor failed because no class was specified"),__FUNCTION__);
		return;
	}
	
	//Work var
	const bool bMustAttachtoLastPoint = CablePointComponents.IsValidIndex(0);
	FVector masterLoc = bMustAttachtoLastPoint && CableCapArray.IsValidIndex(0) ? CableCapArray[0]->GetComponentLocation() : _CurrentHookHitResult.Location;
	
	//Spawn param
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = _PlayerCharacter;
	SpawnInfo.Instigator = _PlayerCharacter;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	//Spawn Rotation
	const FVector dir = _PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector() * 100;
	FRotator rot = UKismetMathLibrary::FindLookAtRotation(targetHit.ImpactPoint, targetHit.ImpactPoint - dir);
	rot.Pitch = rot.Pitch - 90.0f;
		
	_ImpactField = GetWorld()->SpawnActor<APS_FieldSystemActor>(FieldSystemActor.Get(), masterLoc, rot, SpawnInfo);
	
	if(!IsValid(_ImpactField) || !IsValid(_ImpactField->GetCollider())) return;

	//Setup spawned elements
	_ImpactField->SetActorScale3D(extent);
	
	//Bind to EndOverlap && BreakEvent for destroying
	_ImpactField->GetCollider()->OnComponentEndOverlap.AddUniqueDynamic(this, &UPS_HookComponent::OnChaosFieldEndOverlapEventReceived);
	_CurrentGeometryCollection->OnChaosBreakEvent.AddUniqueDynamic(this, &UPS_HookComponent::OnGeometryCollectBreakEventReceived);
	
	//Active move logic 
	_bCanMoveField = true;
	
	//Debug
	if(bDebugChaos) UE_LOG(LogActorComponent, Log, TEXT("%S :: success %i, scale %s"), __FUNCTION__, IsValid(_ImpactField), *extent.ToString());

	//Callback
	OnHookChaosFieldGeneratedEvent.Broadcast(_ImpactField);
}

void UPS_HookComponent::ResetImpactField(const bool bForce)
{
	if(!IsValid(_ImpactField) || !IsValid(_ImpactField->GetCollider())) return;

	if (bDebugChaos) UE_LOG(LogActorComponent, Log, TEXT("%S"),__FUNCTION__);
	
	//Remove callback
	if (IsValid(_CurrentGeometryCollection))
	{
		_CurrentGeometryCollection->OnChaosBreakEvent.RemoveDynamic(this, &UPS_HookComponent::OnGeometryCollectBreakEventReceived);
	}
	_ImpactField->GetCollider()->OnComponentEndOverlap.RemoveDynamic(this, &UPS_HookComponent::OnChaosFieldEndOverlapEventReceived);

	//Reset variables
	_bCanMoveField = false;

	//And for end destroy current impact field
	_ImpactField->Destroy();

	//DettachHook if still attach
	if(IsValid(_AttachedMesh))DettachHook();
}

void UPS_HookComponent::UpdateImpactField()
{
	if(_bPlayerIsSwinging) return;
		
	//Update rot
	FRotator rot = UKismetMathLibrary::FindLookAtRotation(_ImpactField->GetActorLocation(), _PlayerCharacter->GetFirstPersonCameraComponent()->GetComponentLocation());
	rot.Pitch = rot.Pitch - 90.0f;
	_ImpactField->SetActorRotation(rot);
	
	//Update Scale
	const float radius = FMath::Lerp(1.0f, FieldRadiusMulitiplicator, GetAlphaTense());
	FVector scale = FVector::One() * radius;
	_ImpactField->SetActorScale3D(scale);
	
	if (bDebugChaos) UE_LOG(LogTemp, Log, TEXT("%S :: radius %f"), __FUNCTION__, radius);
	
	//Callback use for Velocities variation done in BP
	OnHookChaosFieldMovingEvent.Broadcast();
}

void UPS_HookComponent::OnGeometryCollectBreakEventReceived(const FChaosBreakEvent& BreakEvent)
{	
	if (!IsValid(_CurrentGeometryCollection) || !IsValid(_AttachedMesh)) return;

	if (bDebugChaos) UE_LOG(LogTemp, Log, TEXT("%S"), __FUNCTION__);
	
	const float radius = FMath::Min((_ImpactField->GetActorScale3D() * 1.25f).Length(), FieldRadiusMulitiplicator);
	_ImpactField->SetActorScale3D(FVector::One() * radius);

	FTimerDelegate timerDelegate;
	timerDelegate.BindUObject(this, &UPS_HookComponent::ResetImpactField, true);
	GetWorld()->GetTimerManager().SetTimer(_ResetFieldTimerHandler, timerDelegate, FieldResetDelay, false);

	DettachHook();
}

void UPS_HookComponent::OnChaosFieldEndOverlapEventReceived(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (bDebugChaos) UE_LOG(LogTemp, Log, TEXT("%S :: OtherActor %s, OtherComp %s"), __FUNCTION__, *OtherActor->GetName(), *OtherComp->GetName());

	//Check if Collider have still actor who's overlapped him
	// TArray<AActor*> actorsOverlapped;
	// OverlappedComponent->GetOverlappingActors(actorsOverlapped);
	// if (!actorsOverlapped.IsEmpty()) return;
		
	FTimerDelegate timerDelegate;
	timerDelegate.BindUObject(this, &UPS_HookComponent::ResetImpactField, true);
	GetWorld()->GetTimerManager().SetTimer(_ResetFieldTimerHandler, timerDelegate, FieldResetDelay, false);

	DettachHook();
}

//------------------

#pragma endregion CanGenerateImpactField

//------------------
#pragma endregion Destruction

