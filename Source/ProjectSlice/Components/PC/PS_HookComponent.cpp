// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_HookComponent.h"

#include "MovieSceneTracksComponentTypes.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectSlice/PC/PS_Character.h"
#include "../../../../Runtime/CableComponent/Source/CableComponent/Classes/CableComponent.h" 
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Kismet/KismetMathLibrary.h"

class UCableComponent;

// Sets default values for this component's properties
UPS_HookComponent::UPS_HookComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	HookThrower = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookThrower"));
	HookThrower->SetCollisionProfileName(FName("NoCollision"), true);
	
	FirstCable = CreateDefaultSubobject<UCableComponent>(TEXT("FirstCable"));
	
	// HookMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookMesh"));
	// HookMesh->SetSimulatePhysics(false);

}


// Called when the game starts
void UPS_HookComponent::BeginPlay()
{
	Super::BeginPlay();

	_PlayerCharacter = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if(!IsValid(_PlayerCharacter)) return;
	
	_PlayerController = Cast<APlayerController>(_PlayerCharacter->GetController());
	if(!IsValid(_PlayerController)) return;
	
	_PlayerCharacter->GetWeaponComponent()->OnWeaponInit.AddUniqueDynamic(this, &UPS_HookComponent::OnInitWeaponEventReceived);

}

// Called every frame
void UPS_HookComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	WrapCable();
}

#pragma region Weapon_Event_Receiver
//------------------

void UPS_HookComponent::OnAttachWeapon()
{
	//Setup HookThrower
	HookThrower->SetupAttachment(this);
	
	//Setup Cable
	FirstCable->SetupAttachment(this);
	CableListArray.AddUnique(FirstCable);
	
	
	// //Setup HookMesh
	// HookMesh->SetCollisionProfileName(FName("NoCollision"), true);
	// HookMesh->SetupAttachment(CableMesh, FName("RopeEnd"));	
}


void UPS_HookComponent::OnInitWeaponEventReceived()
{
	
}

//------------------
#pragma endregion Event_Receiver

#pragma region Cable_Wrap_Logic
//------------------


void UPS_HookComponent::WrapCable()
{
	if(!IsValid(GetOwner()) || !IsValid(FirstCable) || !IsValid(HookThrower)) return;
	
	//Debug
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Yellow, FString::Printf(TEXT("Cable List: %i"), CableListArray.Num()));
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Yellow, FString::Printf(TEXT("Cable Attached: %i"), CableAttachedArray.Num()));
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Yellow, FString::Printf(TEXT("Cable Points Loc: %i"), CablePointLocations.Num()));

	
	//-----Add Wrap Logic-----
	//Init works Variables
	UCableComponent* latestCable;
	
	//Add By Last
	if(CableListArray.IsValidIndex(CableListArray.Num()-1))
	{
		bIsAddByFirst = false;
		latestCable = CableListArray[CableListArray.Num() - 1];
	}
	//Add By First
	else if(CableListArray.IsValidIndex(0) && CableListArray.Num() > 0)
	{
		bIsAddByFirst =  true;
		latestCable = CableListArray[0];
	}
	else
		return;

	const FSCableWarpParams currentTraceCableWarp = TraceCableWrap(latestCable, bIsAddByFirst);
	const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, true);
	
	//If hit nothing return;
	if (!currentTraceCableWarp.OutHit.bBlockingHit || !IsValid(currentTraceCableWarp.OutHit.GetComponent())) return;

	//If Location Already Exist return
	if (!CheckPointLocation(currentTraceCableWarp.OutHit.Location, CableWrapErrorTolerance)) return;

	//----Last Cable && New Points---
	//Add new Point Loc && Hitted Component to Array
	if(!bIsAddByFirst)
	{
		CableAttachedArray.AddUnique(latestCable);
		CablePointLocations.Add(currentTraceCableWarp.OutHit.Location);
		CablePointComponents.Add(currentTraceCableWarp.OutHit.GetComponent());
		//TODO :: Check utility
		CablePointUnwrapAlphaArray.Add(0.0f);
	}
	else
	{
		CablePointLocations.Insert(currentTraceCableWarp.OutHit.Location, 0);
		CablePointComponents.Insert(currentTraceCableWarp.OutHit.GetComponent(), 0);
		//TODO :: Check utility
		CablePointUnwrapAlphaArray.Insert(0.0f, 0);
	}

	//Attach Last Cable to Hitted Object && Set his position to it
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
	newCable->RegisterComponent();
	//GetOwner()->AddInstanceComponent(localNewCable);

	//Use zero length and single segment to make it tense
	newCable->CableLength = 0;
	latestCable->NumSegments = 0;

	//Attach New Cable to Hitted Object && Set his position to it
	newCable->SetWorldLocation(HookThrower->GetComponentLocation());
	newCable->AttachToComponent(HookThrower, AttachmentRule);

	//Attach End to Last Cable
	newCable->SetAttachEndToComponent(currentTraceCableWarp.OutHit.GetComponent());
	newCable->EndLocation = currentTraceCableWarp.OutHit.GetComponent()->GetComponentTransform().InverseTransformPosition(currentTraceCableWarp.OutHit.Location);
	newCable->bAttachEnd = true;
	!bIsAddByFirst ? CableListArray.AddUnique(newCable) : CableListArray.Insert(newCable,1);

	//----Caps Sphere---
	//Add Sphere on Caps
	if (bCanUseSphereCaps)
		AddSphereCaps(currentTraceCableWarp);

	//----Set New Cable Params identical to First Cable---
	if (bCableUseSharedSettings)
	{
		newCable->CableWidth = FirstCable->CableWidth;

		//TODO :: Review this thing for Pull by Tens func
		newCable->CableLength = FirstCable->CableLength;
		newCable->CableGravityScale = FirstCable->CableGravityScale;
		newCable->SolverIterations = FirstCable->CableLength;

		newCable->TileMaterial = FirstCable->TileMaterial;
		newCable->CollisionFriction = FirstCable->CollisionFriction;
		newCable->bEnableCollision = FirstCable->bEnableCollision;
		newCable->bEnableStiffness = FirstCable->bEnableStiffness;
	}


	//----Debug Cable Color---
	if (bDebugMaterialColors)
	{
		if (!IsValid(CableDebugMaterialInst)) return;

		UMaterialInstanceDynamic* dynMatInstance = newCable->CreateDynamicMaterialInstance(0, CableDebugMaterialInst);

		if (!IsValid(dynMatInstance)) return;
		dynMatInstance->SetVectorParameterValue(FName("Color"),UKismetMathLibrary::HSVToRGB(UKismetMathLibrary::RandomFloatInRange(0, 360), 1, 1, 1));
	}
}

void UPS_HookComponent::UnwrapCable()
{
}

FSCableWarpParams UPS_HookComponent::TraceCableWrap(USceneComponent* cable, const bool bReverseLoc) const
{
	if(IsValid(cable) && IsValid(GetWorld()))
	{
		FSCableWarpParams out;
		
		FVector start = cable->GetSocketLocation(FName("CableStart"));
		FVector end = cable->GetSocketLocation(FName("CableEnd"));

		out.CableStart = bReverseLoc ? end : start;
		out.CableEnd = bReverseLoc ? start : end;
		
		const TArray<AActor*> actorsToIgnore;
		UKismetSystemLibrary::LineTraceSingle(GetWorld(), out.CableStart, out.CableEnd,  UEngineTypes::ConvertToTraceType(ECC_Visibility),
			true, actorsToIgnore, bDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, out.OutHit, true);

		return out;
	}
	else
		return FSCableWarpParams();
}

void UPS_HookComponent::AddSphereCaps(const FSCableWarpParams& currentTraceParams)
{
	const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, true);
	
	FVector rotatedCapTowardTarget = UKismetMathLibrary::GetUpVector(UKismetMathLibrary::FindLookAtRotation(currentTraceParams.CableStart,currentTraceParams.OutHit.Location));
	const FTransform& capsRelativeTransform = FTransform(rotatedCapTowardTarget.Rotation(),currentTraceParams.OutHit.Location,UKismetMathLibrary::Conv_DoubleToVector(FirstCable->CableWidth * 0.0105));

	//Add new Cap Sphere (sphere size should be like 0.0105 of cable to fit)
	UStaticMeshComponent* newCapMesh = Cast<UStaticMeshComponent>(GetOwner()->AddComponentByClass(UStaticMeshComponent::StaticClass(), true, capsRelativeTransform,false));
	newCapMesh->RegisterComponent();
	if (!IsValid(newCapMesh))
	{
		UE_LOG(LogTemp, Error, TEXT("PS_HookComponent :: newCapMesh Invalid"));
		return;
	}
	//GetOwner()->AddInstanceComponent(newCapMesh);

	//Set Mesh && Attach Cap to Hitted Object
	if(IsValid(CapsMesh)) newCapMesh->SetStaticMesh(CapsMesh);
	newCapMesh->AttachToComponent(currentTraceParams.OutHit.GetComponent(), AttachmentRule);

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
	return bPointLocEqual;
}

//------------------
#pragma endregion Cable_Wrap_Logic


#pragma region Grapple_Logic
//------------------

void UPS_HookComponent::Grapple(const FVector& sourceLocation)
{
	//Break Hook constraint if already exist
	if(IsConstrainted())
	{
		DettachGrapple();
		return;
	}
		
	//Trace config
	const FRotator SpawnRotation = _PlayerController->PlayerCameraManager->GetCameraRotation();
	const TArray<AActor*> ActorsToIgnore{_PlayerCharacter, GetOwner()};
	
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), sourceLocation,
										  sourceLocation + SpawnRotation.Vector() * 1000,
										  UEngineTypes::ConvertToTraceType(ECC_PhysicsBody), false, ActorsToIgnore,
										  bDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, CurrentHookHitResult, true);

	
	if (!CurrentHookHitResult.bBlockingHit || !IsValid(CurrentHookHitResult.GetComponent())) return;

	//If Hook too Far Away
	if(CurrentHookHitResult.Distance > MaxHookDistance)
	{
		UE_LOG(LogTemp, Log, TEXT("UPS_HookComponent:: Can't Attach, Cable too short"));
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, FString::Printf(TEXT("Cable too short")));
		return;
	}

	// //If Attach
	// if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, FString::Printf(TEXT("SetConstraint")));
	//
	// CurrentHookHitResult.GetComponent()->SetWorldLocation(CableTargetAttach->GetComponentLocation());
	// //CableMesh->SetCollisionProfileName(FName("PhysicsActor"), true);
	// CableMesh->SetSimulatePhysics(true);
	// CableTargetAttach->SetConstrainedComponents(CableMesh, FName("Bone_001"),CurrentHookHitResult.GetComponent(), FName("None"));

	bIsConstrainted = true;
}

void UPS_HookComponent::DettachGrapple()
{	
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, FString::Printf(TEXT("BreakConstraint")));
	
	// CableTargetAttach->BreakConstraint();
	// //CableMesh->SetCollisionProfileName(FName("NoCollision"), true);
	// CableMesh->SetSimulatePhysics(false);
	// CableMesh->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	// CableMesh->SetRelativeRotation(FRotator(90,0,0), false, nullptr, ETeleportType::TeleportPhysics);
		
	bIsConstrainted = false;
}

//------------------
#pragma endregion Grapple_Logic




