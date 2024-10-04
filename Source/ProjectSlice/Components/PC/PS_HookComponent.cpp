// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_HookComponent.h"

#include "PS_PlayerCameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectSlice/PC/PS_Character.h"
#include "../../../../Runtime/CableComponent/Source/CableComponent/Classes/CableComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "Image/ImageBuilder.h"
#include "ProjectSlice/Data/PS_TraceChannels.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/FunctionLibrary/PSFl.h"

class UCableComponent;

// Sets default values for this component's properties
UPS_HookComponent::UPS_HookComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	HookThrower = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookThrower"));
	HookThrower->SetCollisionProfileName(Profile_NoCollision, true);
	HookThrower->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	FirstCable = CreateDefaultSubobject<UCableComponent>(TEXT("FirstCable"));
	FirstCable->SetCollisionProfileName(Profile_NoCollision, true);
	
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
	
	_PlayerCharacter->GetWeaponComponent()->OnWeaponInit.AddUniqueDynamic(this, &UPS_HookComponent::OnInitWeaponEventReceived);

	if(IsValid(FirstCable))
		FirstCableDefaultLenght = FirstCable->CableLength;

	if(IsValid(_PlayerCharacter->GetSlowmoComponent()))
	{
		_PlayerCharacter->GetSlowmoComponent()->OnSlowmoEvent.AddUniqueDynamic(this, &UPS_HookComponent::OnSlowmoTriggerEventReceived);
	}
	
	if(IsValid(_PlayerCharacter->GetCharacterMovement()))
	{
		DefaultAirControl = _PlayerCharacter->GetCharacterMovement()->AirControl;
		DefaultGravityScale = _PlayerCharacter->GetCharacterMovement()->GravityScale;
	}
}

// Called every frame
void UPS_HookComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	CableWraping();
	PowerCablePull();
}

#pragma region Weapon_Event_Receiver
//------------------

void UPS_HookComponent::OnAttachWeapon()
{
	//Setup HookThrower
	HookThrower->SetupAttachment(this);
	
	//Setup Cable
	FirstCable->SetupAttachment(this);
	FirstCable->SetVisibility(false);
	CableListArray.AddUnique(FirstCable);

	CableCapArray.Add(nullptr);
			
	// //Setup HookMesh
	// HookMesh->SetCollisionProfileName(FName("NoCollision"), true);
	// HookMesh->SetupAttachment(CableMesh, FName("RopeEnd"));	
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

#pragma region Cable_Wrap_Logic
//------------------


void UPS_HookComponent::CableWraping()
{
	//Try Wrap only if attached
	if(!IsValid(AttachedMesh) || !IsValid(GetOwner()) || !IsValid(HookThrower)) return;

	//Wrap Logics
	WrapCable();
	UnwrapCableByFirst();
	UnwrapCableByLast();

	//Rope Adaptation
	//AdaptCableTens();
}

void UPS_HookComponent::WrapCable()
{
	if(!IsValid(FirstCable)) return;
	
	//-----Add Wrap Logic-----
	//Init works Variables
	UCableComponent* latestCable = nullptr;
	const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, true);
	FSCableWarpParams currentTraceCableWarp;

	//Add By First
	if(CableListArray.IsValidIndex(0) && CableListArray.Num() > 0)
	{
		latestCable = CableListArray[0];
		currentTraceCableWarp = TraceCableWrap(latestCable, true);

		//If hit nothing return;
		if (currentTraceCableWarp.OutHit.bBlockingHit && IsValid(currentTraceCableWarp.OutHit.GetComponent()))
			bIsAddByFirst = true;
	}

	//Add By Last
	if(CableListArray.IsValidIndex(CableListArray.Num()-1))
	{
		latestCable = CableListArray[CableListArray.Num() - 1];
		currentTraceCableWarp = TraceCableWrap(latestCable, false);
		
		//If hit nothing return;
		if (currentTraceCableWarp.OutHit.bBlockingHit && IsValid(currentTraceCableWarp.OutHit.GetComponent()))
			bIsAddByFirst = false;
	}

	//If Trace Hit nothing or Invalid object return
	if (!currentTraceCableWarp.OutHit.bBlockingHit || !IsValid(currentTraceCableWarp.OutHit.GetComponent())) return;
		
	//If Location Already Exist return
	if (!IsValid(latestCable)|| !CheckPointLocation(currentTraceCableWarp.OutHit.Location, CableWrapErrorTolerance)) return;
	

	//----Last Cable && New Points---
	//Add new Point Loc && Hitted Component to Array
	if(!bIsAddByFirst)
	{
		CableAttachedArray.AddUnique(latestCable);
		CablePointLocations.Add(currentTraceCableWarp.OutHit.Location);
		CablePointComponents.Add(currentTraceCableWarp.OutHit.GetComponent());
		CablePointUnwrapAlphaArray.Add(0.0f);
	}
	else
	{
		CablePointLocations.Insert(currentTraceCableWarp.OutHit.Location, 0);
		CablePointComponents.Insert(currentTraceCableWarp.OutHit.GetComponent(), 0);
		CablePointUnwrapAlphaArray.Insert(0.0f, 0);
	}
	
	//Attach Last Cable to Hitted Object && Set his position to it
	latestCable->CableLength = 1;
	latestCable->AttachToComponent(currentTraceCableWarp.OutHit.GetComponent(), AttachmentRule);
	latestCable->SetWorldLocation(currentTraceCableWarp.OutHit.Location, false, nullptr,ETeleportType::TeleportPhysics);
	
	//----New Cable---
	//Add new cable component 
	UCableComponent* newCable = Cast<UCableComponent>(GetOwner()->AddComponentByClass(UCableComponent::StaticClass(), false, FTransform(), false));
	if (!IsValid(newCable))
	{
		UE_LOG(LogTemp, Error, TEXT("PS_HookComponent :: localNewCable Invalid"));
		return;
	}

	//For avoid collide in next tick with New Cable
	newCable->SetCollisionResponseToChannel(ECC_Rope, ECR_Ignore);

	//Use zero length and single segment to make it tense
	newCable->CableLength = 0;
	newCable->NumSegments = 1;
	
	//GetOwner()->AddInstanceComponent(localNewCable);

	//Attach New Cable to Hitted Object && Set his position to it
	if(bIsAddByFirst && CablePointLocations.IsValidIndex(1) && CablePointComponents.IsValidIndex(1))
	{
		newCable->SetWorldLocation(CablePointLocations[1]);
		newCable->AttachToComponent(CablePointComponents[1], AttachmentRule);
	}
	else
	{
		newCable->SetWorldLocation(HookThrower->GetComponentLocation());
		newCable->AttachToComponent(HookThrower, AttachmentRule);
	}
	
	//Attach End to Last Cable
	newCable->SetAttachEndToComponent(currentTraceCableWarp.OutHit.GetComponent());
	newCable->EndLocation = currentTraceCableWarp.OutHit.GetComponent()->GetComponentTransform().InverseTransformPosition(currentTraceCableWarp.OutHit.Location);
	newCable->bAttachEnd = true;

	if(bIsAddByFirst)
	{
		CableListArray.Insert(newCable,1);
		CableAttachedArray.Insert(newCable,1);

	}else
		CableListArray.Add(newCable);
	
	//----Caps Sphere---
	//Add Sphere on Caps
	if(bCanUseSphereCaps)
		AddSphereCaps(currentTraceCableWarp);
		
	//----Set New Cable Params identical to First Cable---
	if (bCableUseSharedSettings)
	{
		newCable->CableWidth = FirstCable->CableWidth;

		//TODO :: Review this thing for Pull by Tens func
		newCable->CableLength = FirstCableDefaultLenght;
		newCable->CableGravityScale = FirstCable->CableGravityScale;
		newCable->SolverIterations = FirstCable->CableLength;

		newCable->TileMaterial = FirstCable->TileMaterial;
		newCable->CollisionFriction = FirstCable->CollisionFriction;
		newCable->bEnableCollision = FirstCable->bEnableCollision;
		newCable->bEnableStiffness = FirstCable->bEnableStiffness;
		
	}

	//----Debug Cable Color---
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

void UPS_HookComponent::UnwrapCableByFirst()
{	
	//-----Unwrap Logic-----
	//Init works Variables
	UCableComponent* pastCable = nullptr;
	UCableComponent* currentCable = nullptr;
	const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, true);

	//Remove By First
	if(CableListArray.IsValidIndex(1) && CableListArray.IsValidIndex(0))
	{
		pastCable = CableListArray[1];
		currentCable = CableListArray[0];
	}

	if(!IsValid(currentCable) || !IsValid(pastCable)) return;
	
	//----Unwrap Test-----
	const TArray<AActor*> actorsToIgnore;

	const FVector pastCableStartSocketLoc = pastCable->GetSocketLocation(FName("CableStart"));
	const FVector pastCableEndSocketLoc = pastCable->GetSocketLocation(FName("CableEnd"));
	const FVector currentCableEndSocketLoc = currentCable->GetSocketLocation(FName("CableEnd"));

	const FVector currentCableDirection = UKismetMathLibrary::FindLookAtRotation(currentCableEndSocketLoc, pastCableStartSocketLoc).Vector();
	const float  currentCableDirectionDistance = UKismetMathLibrary::Vector_Distance(currentCableEndSocketLoc, pastCableStartSocketLoc);
	
	const FVector pastCableDirection = UKismetMathLibrary::FindLookAtRotation(pastCableEndSocketLoc, pastCableStartSocketLoc).Vector();

	const FVector start = currentCableEndSocketLoc;
	const FVector endSafeCheck = currentCableEndSocketLoc + currentCableDirection * (currentCableDirectionDistance * 0.91);
	const FVector end = currentCableEndSocketLoc + pastCableDirection * CableUnwrapDistance;

	//----Safety Trace-----
	//This trace is used as a safety checks if there is no blocking towards the past cable loc.
	FHitResult outHitSafeCheck;
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), start, endSafeCheck,  UEngineTypes::ConvertToTraceType(ECC_Rope),
	false, actorsToIgnore, false ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None, outHitSafeCheck, true);

	//If no hit, or hit very close to trace end then continue unwrap
	if(outHitSafeCheck.bBlockingHit && !outHitSafeCheck.Location.Equals(outHitSafeCheck.TraceEnd, CableUnwrapErrorMultiplier)) return;
	//----Main Trace-----
	/*This trace is the main one, basically checks from last cable to past cable but slightly forward by cable path. so if cable is wrapped,
	then the target will be slightly on other side, to unwrap we should either get no hit, or hit very close to target location.*/
	FHitResult outHit;
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), start, end,  UEngineTypes::ConvertToTraceType(ECC_Rope),
false, actorsToIgnore, bDebugTick ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None, outHit, true, FColor::Cyan);
	

	//If no hit, or hit very close to trace end then process unwrap
	float currentUnwrapAlpha = CablePointUnwrapAlphaArray[0];
	if(outHit.bBlockingHit && !outHit.Location.Equals(outHit.TraceEnd, CableUnwrapErrorMultiplier))
	{
		CablePointUnwrapAlphaArray[0] = 0.0f;
		return;
	}

	//----Custom tick-----
	//Unwrap with delay frames to prevent flickering of wrap/unwrap cycles.Basically increase point alpha value by 1 each frame, if it's more than custom value then process. Use subtle values for responsive unwrap.
	CablePointUnwrapAlphaArray[0] = CablePointUnwrapAlphaArray[0] + 1;
	if(currentUnwrapAlpha < CableUnwrapFirstFrameDelay) return;

	//----Destroy and remove Last Cable tick-----
	//If attached cables are more than 0 remove the second one (when first cable gets closer to it)
	CableAttachedArray.RemoveAt(CableAttachedArray.IsValidIndex(1) ? 1 : 0);
	//In any case, destroy the second cable (the first one is our main cable)
	CableListArray[1]->DestroyComponent();

	//----Caps Sphere---
	if(bCanUseSphereCaps)
	{
		CableCapArray[1]->DestroyComponent();
		CableCapArray.RemoveAt(1);
	}
	
	CableListArray.RemoveAt(1);
	CablePointLocations.RemoveAt(0);
	CablePointComponents.RemoveAt(0);
	CablePointUnwrapAlphaArray.RemoveAt(0);
	

	//----Set first cable Loc && Attach----
	if(!CableListArray.IsValidIndex(0)) return;
	UCableComponent* firstCable = CableListArray[0];
	
	if(CablePointLocations.IsValidIndex(0))
	{
		//Set the latest oldest point as cable active point && attach cable to the latest component point
		firstCable->SetWorldLocation(CablePointLocations[0], false,nullptr, ETeleportType::TeleportPhysics);
		firstCable->AttachToComponent(CablePointComponents[0], AttachmentRule);
	}
	else
	{
		//Reset to HookAttach default set
		firstCable->SetWorldLocation(HookThrower->GetComponentLocation() + CableHookOffset, false,nullptr, ETeleportType::TeleportPhysics);
		firstCable->AttachToComponent(HookThrower, AttachmentRule);
	}
}

void UPS_HookComponent::UnwrapCableByLast()
{
	if(!CableListArray.IsValidIndex(0)) return;

	//Try Unwrap only if attached
	if(!IsValid(AttachedMesh)) return;
	
	//-----Unwrap Logic-----
	//Init works Variables
	UCableComponent* pastCable;
	UCableComponent* currentCable;
	int32 cableListLastIndex = CableListArray.Num()-1;
	int32 cableAttachedLastIndex = CableAttachedArray.Num()-1;
	int32 cablePointLocationsLastIndex = CablePointLocations.Num()-1;
	const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, true);
	
	//Remove By Last
	if(CableAttachedArray.IsValidIndex(cableAttachedLastIndex) && CableListArray.IsValidIndex(cableListLastIndex))
	{
		pastCable = CableAttachedArray[cableAttachedLastIndex];
		currentCable = CableListArray[cableListLastIndex];
	}
	else
	{
		pastCable = nullptr;
		currentCable = nullptr;
	}
	
	if(!IsValid(currentCable) || !IsValid(pastCable)) return;


	//----Unwrap Test-----
	const TArray<AActor*> actorsToIgnore;

	const FVector pastCableStartSocketLoc = pastCable->GetSocketLocation(FName("CableStart"));
	const FVector pastCableEndSocketLoc = pastCable->GetSocketLocation(FName("CableEnd"));
	const FVector currentCableStartSocketLoc = currentCable->GetSocketLocation(FName("CableStart"));
	const FVector currentCableEndSocketLoc = currentCable->GetSocketLocation(FName("CableEnd"));

	const FVector currentCableDirection = UKismetMathLibrary::FindLookAtRotation(currentCableStartSocketLoc, pastCableEndSocketLoc).Vector();
	const float  currentCableDirectionDistance = UKismetMathLibrary::Vector_Distance(currentCableStartSocketLoc, pastCableEndSocketLoc);
	
	const FVector pastCableDirection = UKismetMathLibrary::FindLookAtRotation(pastCableStartSocketLoc, pastCableEndSocketLoc).Vector();

	const FVector start = currentCableStartSocketLoc;
	const FVector endSafeCheck = start + currentCableDirection * (currentCableDirectionDistance * 0.91);
	const FVector end = pastCableStartSocketLoc + pastCableDirection * CableUnwrapDistance;

	//----Safety Trace-----
	//This trace is used as a safety checks if there is no blocking towards the past cable loc.
	FHitResult outHitSafeCheck;
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), start, endSafeCheck,  UEngineTypes::ConvertToTraceType(ECC_Rope),
	false, actorsToIgnore, false ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None, outHitSafeCheck, true);

	//If no hit, or hit very close to trace end then continue unwrap
	if(outHitSafeCheck.bBlockingHit && !outHitSafeCheck.Location.Equals(outHitSafeCheck.TraceEnd, CableUnwrapErrorMultiplier)) return;

	//----Main Trace-----
	/*This trace is the main one, basically checks from last cable to past cable but slightly forward by cable path. so if cable is wrapped,
	then the target will be slightly on other side, to unwrap we should either get no hit, or hit very close to target location.*/
	FHitResult outHit;
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), start, end,  UEngineTypes::ConvertToTraceType(ECC_Rope),
false, actorsToIgnore, bDebugTick ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None, outHit, true, FColor::Blue);

	//----Custom tick-----
	float cablePointUnwrapAlphaLastIndex = CablePointUnwrapAlphaArray.Num()-1;;
	
	//If no hit, or hit very close to trace end then process unwrap
	if(outHit.bBlockingHit && !outHit.Location.Equals(outHit.TraceEnd, CableUnwrapErrorMultiplier) && CablePointUnwrapAlphaArray.IsValidIndex(cablePointUnwrapAlphaLastIndex))
	{
		CablePointUnwrapAlphaArray[cablePointUnwrapAlphaLastIndex] = 0.0f;
		return;
	}

	//Unwrap with delay frames to prevent flickering of wrap/unwrap cycles.Basically increase point alpha value by 1 each frame, if it's more than custom value then process. Use subtle values for responsive unwrap.

	if(CablePointUnwrapAlphaArray.IsValidIndex(cablePointUnwrapAlphaLastIndex))
	{
		CablePointUnwrapAlphaArray[cablePointUnwrapAlphaLastIndex] = CablePointUnwrapAlphaArray[cablePointUnwrapAlphaLastIndex] + 1;
		if(CablePointUnwrapAlphaArray[cablePointUnwrapAlphaLastIndex] < CableUnwrapLastFrameDelay)
		{
			return;
		}
	}
	else
	{
		CablePointUnwrapAlphaArray[0] = CablePointUnwrapAlphaArray[0] + 1;
	}
	
	//----Destroy and remove Last Cable tick-----
	if(!CableAttachedArray.IsValidIndex(0) || !CableAttachedArray.IsValidIndex(cableAttachedLastIndex) || !CableListArray.IsValidIndex(cableListLastIndex)) return;

	//If attached cables are more than 0 remove the second one (when first cable gets closer to it)
	CableAttachedArray.RemoveAt(cableAttachedLastIndex);	
	//In any case, destroy the second cable (the first one is our main cable)
	CableListArray[cableListLastIndex]->DestroyComponent();

	//----Caps Sphere---
	if(bCanUseSphereCaps)
	{
		if(CableCapArray.IsValidIndex(cableListLastIndex))
		{
			CableCapArray[cableListLastIndex]->DestroyComponent();
			CableCapArray.RemoveAt(cableListLastIndex);
		}
	}

	//End Unwrap
	CableListArray.RemoveAt(cableListLastIndex);
	CablePointComponents.RemoveAt(cablePointLocationsLastIndex);
	CablePointUnwrapAlphaArray.RemoveAt(cablePointLocationsLastIndex);
	CablePointLocations.RemoveAt(cablePointLocationsLastIndex);

	//----Set first cable Loc && Attach----
	// if(CablePointLocations.IsValidIndex(0))
	// {
	// 	UCableComponent* firstCable;
	// 	cableListLastIndex = CableListArray.Num()-1;
	// 	cablePointLocationsLastIndex = CablePointLocations.Num()-1;
	// 	float cablePointComponentsLastIndex = CablePointComponents.Num()-1;
	// 	
	// 	if(!CableListArray.IsValidIndex(cableListLastIndex) || !CablePointLocations.IsValidIndex(cablePointLocationsLastIndex) || !CablePointComponents.IsValidIndex(cablePointComponentsLastIndex)) return;
	// 	
	// 	firstCable = CableListArray[cableListLastIndex];
	//
	// 	//Set the latest oldest point as cable active point && attach cable to the latest component point
	// 	firstCable->SetWorldLocation(CablePointLocations[cablePointLocationsLastIndex], false,nullptr, ETeleportType::TeleportPhysics);
	// 	firstCable->AttachToComponent(CablePointComponents[cablePointComponentsLastIndex], AttachmentRule);
	// }
	// else
	// {
	
	cableListLastIndex = CableListArray.Num()-1;
	if(!CableListArray.IsValidIndex(cableListLastIndex)) return;
	UCableComponent* firstCable = CableListArray[cableListLastIndex];

	//Reset to HookAttach default set
	firstCable->SetWorldLocation(HookThrower->GetComponentLocation() + CableHookOffset, false,nullptr, ETeleportType::TeleportPhysics);
	firstCable->AttachToComponent(HookThrower, AttachmentRule);

	// }
	
}

FSCableWarpParams UPS_HookComponent::TraceCableWrap(const USceneComponent* cable, const bool bReverseLoc) const
{
	if(IsValid(cable) && IsValid(GetWorld()))
	{
		FSCableWarpParams out;
		
		FVector start = cable->GetSocketLocation(FName("CableStart"));
		FVector end = cable->GetSocketLocation(FName("CableEnd"));

		out.CableStart = bReverseLoc ? end : start;
		out.CableEnd = bReverseLoc ? start : end;
		
		const TArray<AActor*> actorsToIgnore;
		UKismetSystemLibrary::LineTraceSingle(GetWorld(), out.CableStart, out.CableEnd,  UEngineTypes::ConvertToTraceType(ECC_Rope),
			true, actorsToIgnore, bDebugTick ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None, out.OutHit, true);

		return out;
	}
	else
		return FSCableWarpParams();
}

void UPS_HookComponent::AddSphereCaps(const FSCableWarpParams& currentTraceParams)
{
	const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, true);
	
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
	newCapMesh->AttachToComponent(currentTraceParams.OutHit.GetComponent(), AttachmentRule);

	//Get Cable Material and add to Cable
	 if(!bDebugMaterialColors)
	 {
	 	if(IsValid(CableCapsMaterialInst))
	 		newCapMesh->CreateDynamicMaterialInstance(0, CableCapsMaterialInst);
	 }

	//Add to list
    !bIsAddByFirst ? CableCapArray.AddUnique(newCapMesh) : CableCapArray.Insert(newCapMesh,1);
}

bool UPS_HookComponent::CheckPointLocation(const FVector& targetLoc, const float& errorTolerance)
{
	bool bPointLocEqual = false;
	for (auto cableElementLoc : CablePointLocations)
	{
		bPointLocEqual = cableElementLoc.Equals(targetLoc, errorTolerance);
		if(bPointLocEqual) break;
	}
	return !bPointLocEqual;
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
	if(baseToMeshDist > CableMaxTensDistance) return;
	
	//----Adapt Tens---
	const float alpha = UKismetMathLibrary::MapRangeClamped(CableMaxTensDistance - baseToMeshDist , 0, CableMaxTensDistance,0 ,1);
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
	if(!IsValid(FirstCable) || !IsValid(HookThrower)) return;
		
	//Break Hook constraint if already exist Or begin Winding
	if(IsValid(GetAttachedMesh()))
	{
		DettachHook();
		return;
	}

	//Trace config
	//TODO :: Make a TraceType for Hook Object
	UStaticMeshComponent* sightMesh = _PlayerCharacter->GetWeaponComponent()->GetSightMeshComponent();
	if(!IsValid(sightMesh)) return;
	
	const TArray<AActor*> ActorsToIgnore{_PlayerCharacter, GetOwner()};
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), sightMesh->GetComponentLocation(),
										  sightMesh->GetComponentLocation() + sightMesh->GetForwardVector() * HookingMaxDistance,
										  UEngineTypes::ConvertToTraceType(ECC_Slice), false, ActorsToIgnore,
										  bDebugTick ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, CurrentHookHitResult, true, FColor::Blue, FColor::Cyan);
	
	if (!CurrentHookHitResult.bBlockingHit || !IsValid( Cast<UMeshComponent>(CurrentHookHitResult.GetComponent()))) return;

	//Hook Move Feedback
	bObjectHook = true;

	//Define new attached component
	AttachedMesh = Cast<UMeshComponent>(CurrentHookHitResult.GetComponent());
	//Attach First cable to it
	FirstCable->SetAttachEndToComponent(AttachedMesh);
	FirstCable->EndLocation = CurrentHookHitResult.GetComponent()->GetComponentTransform().InverseTransformPosition(bIsPullingSphericalObject ? CurrentHookHitResult.Location : CurrentHookHitResult.GetComponent()->GetComponentLocation());
	FirstCable->bAttachEnd = true;
	FirstCable->SetCollisionProfileName(Profile_PhysicActor, true);
	FirstCable->SetVisibility(true);

	//Setup  new attached component
	AttachedMesh->SetGenerateOverlapEvents(true);
	AttachedMesh->SetCollisionProfileName(Profile_GPE);
	AttachedMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	AttachedMesh->SetCollisionResponseToChannel(ECC_Rope, ECollisionResponse::ECR_Ignore);
	//TODO :: Need to define inertia conditioning to false;
	AttachedMesh->SetLinearDamping(1.0f);
	AttachedMesh->SetAngularDamping(1.0f);
	AttachedMesh->SetSimulatePhysics(true);
	AttachedMesh->WakeRigidBody();
	
	//Determine max distance for Pull
	DistanceOnAttach = FMath::Abs(UKismetMathLibrary::Vector_Distance(HookThrower->GetComponentLocation(), AttachedMesh->GetComponentLocation()));
	
	if (GEngine && bDebug) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, FString::Printf(TEXT("SetConstraint")));
}

void UPS_HookComponent::WindeHook()
{
	//Break Hook constraint if already exist Or begin Winding
	if(IsValid(GetAttachedMesh()) && IsValid(GetWorld()))
	{
		bCableWinderPull = true;
		CableStartWindeTimestamp = GetWorld()->GetAudioTimeSeconds();
	}
		
}

void UPS_HookComponent::StopWindeHook()
{
	bCableWinderPull = false;
}


void UPS_HookComponent::DettachHook()
{
	//If FirstCable is not in CableList return
	if(!IsValid(FirstCable) || !IsValid(AttachedMesh)) return;

	//----Stop Cable Warping---
	bCableWinderPull = false;
	AttachedMesh = nullptr;

	//----Reset Swing---
	OnTriggerSwing(false);

	//----Hook Move Feedback---
	bObjectHook = false;
		
	//----Clear Cable Warp ---
	int i = 1;
	for (auto cable : CableAttachedArray)
	{
		if(!IsValid(cable))
		{
			i++;
			continue;
		}
		
		//Remove and Destroy cable
		//Prevent to Destroy FirstCable
		CableListArray.Remove(cable);
		if(cable != FirstCable)
			cable->DestroyComponent();

		//Remove and Destroy Cable Cap
		if(CableCapArray.IsValidIndex(i))
		{
			UStaticMeshComponent* currentCap = CableCapArray[i];
			if(!IsValid(currentCap))
			{
				i++;
				continue;
			}
			
			CableCapArray.Remove(currentCap);
			currentCap->DestroyComponent();
		}
		
	}

	//Reset Cable List
	if(CableListArray[0] != FirstCable) CableListArray[0]->DestroyComponent();
	CableListArray[0] = FirstCable;

	//Clear Array
	CableAttachedArray.Empty();
	CablePointUnwrapAlphaArray.Empty();
	CablePointLocations.Empty();
	CablePointComponents.Empty();

	
	//----Setup First Cable---
	const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, true);

	FirstCable->SetWorldLocation(HookThrower->GetComponentLocation() + CableHookOffset, false,nullptr, ETeleportType::TeleportPhysics);
	FirstCable->AttachToComponent(HookThrower, AttachmentRule);
	FirstCable->bAttachEnd = false;
	FirstCable->AttachEndTo = FComponentReference();
	FirstCable->SetVisibility(false);
	FirstCable->SetCollisionProfileName(Profile_NoCollision, true);

	if (GEngine && bDebug) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, FString::Printf(TEXT("BreakConstraint")));
	
}

void UPS_HookComponent::PowerCablePull()
{
	if(!IsValid(AttachedMesh) || !CableListArray.IsValidIndex(0) || !IsValid(GetWorld())) return;
	
	//Activate or desactivate Swing
	OnTriggerSwing(_PlayerCharacter->GetCharacterMovement()->IsFalling() && AttachedMesh->GetMass() > _PlayerCharacter->GetMesh()->GetMass() * 100);

	//Activate Pull if Winde
	float alpha;
	if(bCableWinderPull)
	{
		const float windeAlpha = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetAudioTimeSeconds(), CableStartWindeTimestamp ,CableStartWindeTimestamp + MaxWindePullingDuration,0 ,1);
		alpha = windeAlpha;
		if(IsValid(WindePullingCurve))
		{
			alpha = WindePullingCurve->GetFloatValue(windeAlpha);
		}

		if(bPlayerIsPulled)
		{
			FVector dir = (_PlayerCharacter->GetActorLocation() - AttachedMesh->GetComponentLocation());
			dir.Normalize();
			
			const FVector velocity = dir * FMath::Lerp(0.0f,MaxForceWeight,alpha);
			UE_LOG(LogTemp, Warning, TEXT("Winde Player velocity %f"), velocity.Length());
			_PlayerCharacter->GetCharacterMovement()->AddForce(velocity * _PlayerCharacter->CustomTimeDilation);
		}

	}
	//Activate Pull On reach Max Distance
	else/* if(!bPlayerIsPulled)*/
	{
		float baseToMeshDist =	FMath::Abs(UKismetMathLibrary::Vector_Distance(HookThrower->GetComponentLocation(),AttachedMesh->GetComponentLocation()));
		float DistanceOnAttachByTensorCount = CableCapArray.Num() > 0 ? DistanceOnAttach/CableCapArray.Num() : DistanceOnAttach;

		alpha = UKismetMathLibrary::MapRangeClamped(baseToMeshDist - DistanceOnAttachByTensorCount, 0, MaxForcePullingDistance,0 ,1);
		
		bCablePowerPull = baseToMeshDist > DistanceOnAttach;
	}

	if(!bCablePowerPull && !bCableWinderPull && !bPlayerIsPulled)  return;
	
	//Pull Attached Object
	ForceWeight = FMath::Lerp(0,MaxForceWeight, alpha);
	
	UCableComponent* firstCable = CableListArray[0];
	
	FRotator rotMeshCable = UKismetMathLibrary::FindLookAtRotation(AttachedMesh->GetComponentLocation(), firstCable->GetSocketLocation(FName("CableStart")));
	rotMeshCable.Yaw = rotMeshCable.Yaw + UKismetMathLibrary::RandomFloatInRange(-50,50);

	if(bDebugTick) DrawDebugPoint(GetWorld(), firstCable->GetSocketLocation(FName("CableStart")), 20.f, FColor::Orange, false);
	
	//Use Force
	if(bPlayerIsPulled)
	{
		OnSwing(alpha);
	}
	// else
	// {
	// 	FVector newVel = AttachedMesh->GetMass() * rotMeshCable.Vector() * ForceWeight;
	// 	AttachedMesh->AddImpulse((newVel * GetWorld()->DeltaRealTimeSeconds) * _PlayerCharacter->CustomTimeDilation);
	// }
	//
	FVector newVel = AttachedMesh->GetMass() * rotMeshCable.Vector() * ForceWeight;
	AttachedMesh->AddImpulse((newVel * GetWorld()->DeltaRealTimeSeconds) * _PlayerCharacter->CustomTimeDilation);
	
}

void UPS_HookComponent::OnTriggerSwing(const bool bActivate)
{
	if(bPlayerIsPulled == bActivate) return;
	
	if(!IsValid(GetWorld()) || 	!IsValid(_PlayerCharacter->GetCharacterMovement()) || !IsValid(_PlayerCharacter)) return;
		
	bPlayerIsPulled = bActivate;
	SwingStartTimestamp = GetWorld()->GetTimeSeconds();
	_PlayerController->SetCanMove(!bActivate);
	_PlayerCharacter->GetCharacterMovement()->GravityScale = bActivate ? 1.0f : DefaultGravityScale;
	_PlayerCharacter->GetCharacterMovement()->AirControl = bActivate ? SwingMaxAirControl : DefaultAirControl;
	_PlayerCharacter->GetCharacterMovement()->BrakingDecelerationFalling = 400.0f;
}


void UPS_HookComponent::OnSwing(const float forceWeightAlpha)
{
	//TODO :: WORK BUT JITER
	// FVector velocity = rotMeshCable.Vector() * _PlayerCharacter->GetCharacterMovement()->Mass * ForceWeight * -1;

	//TODO :: TUTO SWING VEL
	FVector dir = (_PlayerCharacter->GetActorLocation() - AttachedMesh->GetComponentLocation());
	dir.Normalize();

	DrawDebugDirectionalArrow(GetWorld(), _PlayerCharacter->GetActorLocation(),_PlayerCharacter->GetActorLocation() + dir * 600, 20.0f, FColor::Blue, false, 0.2, 10, 3);
	
	FVector swingVelocity = -SwingVelocityMultiplicator * dir * (_PlayerCharacter->GetActorLocation() - AttachedMesh->GetComponentLocation()).Dot(_PlayerCharacter->GetVelocity());
	swingVelocity = UPSFl::ClampVelocity(swingVelocity, dir * _PlayerCharacter->GetDefaultMaxWalkSpeed(), _PlayerCharacter->GetDefaultMaxWalkSpeed() * 2);

	//Air Control
	const float timeWeightAlpha = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetTimeSeconds(), SwingStartTimestamp, SwingStartTimestamp + SwingMaxDuration, 0.0f, 1.0f);
	const float targetAirControl = FMath::Lerp(SwingMaxAirControl, DefaultAirControl, timeWeightAlpha);
	
	const float airControlAlpha = UKismetMathLibrary::MapRangeClamped(FMath::Abs(_PlayerCharacter->GetVelocity().Length()), 0.0f, _PlayerCharacter->GetCharacterMovement()->GetMaxSpeed(), 0.0f, 1.0f);
	_PlayerCharacter->GetCharacterMovement()->AirControl = FMath::Lerp(0.0f, targetAirControl, airControlAlpha);

	//Braking Deceleration Falling
	_PlayerCharacter->GetCharacterMovement()->BrakingDecelerationFalling = FMath::Lerp(400.0f, _PlayerCharacter->GetCharacterMovement()->BrakingDecelerationWalking, timeWeightAlpha);

	//Add Force
	_PlayerCharacter->GetCharacterMovement()->AddForce((swingVelocity) * _PlayerCharacter->CustomTimeDilation);

	//Add movement
	const float velocityToAbsFwd = _PlayerCharacter->GetArrowComponent()->GetForwardVector().Dot(_PlayerCharacter->GetVelocity());
	float fakeInputWeight = FMath::Sign(velocityToAbsFwd);
	
	DrawDebugDirectionalArrow(GetWorld(), _PlayerCharacter->GetActorLocation(),_PlayerCharacter->GetActorLocation() + _PlayerCharacter->GetArrowComponent()->GetForwardVector() * _PlayerCharacter->GetVelocity() , 20.0f, FColor::Cyan, false, 0.2, 10, 3);

	//(1 - forceWeightAlpha * fakeInputWeight )
	_PlayerCharacter->AddMovementInput( _PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector() * _PlayerCharacter->CustomTimeDilation, (1 - forceWeightAlpha) * fakeInputWeight);
	_PlayerCharacter->AddMovementInput(_PlayerCharacter->GetFirstPersonCameraComponent()->GetRightVector() * _PlayerCharacter->CustomTimeDilation, _PlayerController->GetMoveInput().X * (1 - forceWeightAlpha));

	UE_LOG(LogTemp, Error, TEXT("%S :: swingVelocity %f, AirControl %f, forceWeightAlpha %f, velocityToAbsFwd %f, fwdInput %f"), __FUNCTION__, swingVelocity.Length(),  _PlayerCharacter->GetCharacterMovement()->AirControl, forceWeightAlpha, velocityToAbsFwd, (1 - forceWeightAlpha) * FMath::Sign(velocityToAbsFwd));

}

//------------------
#pragma endregion Grapple_Logic