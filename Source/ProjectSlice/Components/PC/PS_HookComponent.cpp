// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_HookComponent.h"

#include "MovieSceneTracksComponentTypes.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectSlice/PC/PS_Character.h"
#include "../../../../Runtime/CableComponent/Source/CableComponent/Classes/CableComponent.h" 
#include "Elements/Framework/TypedElementQueryBuilder.h"

class UCableComponent;

// Sets default values for this component's properties
UPS_HookComponent::UPS_HookComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	HookThrower = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookThrower"));
	HookThrower->SetCollisionProfileName(FName("BlockAllDynamic"), true);
	
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

	WrapCableAddbyLast();
}

#pragma region Weapon_Event_Receiver
//------------------

void UPS_HookComponent::OnAttachWeapon()
{
	//Setup HookThrower
	HookThrower->SetupAttachment(this);
	
	//Setup Cable
	//FirstCable->SetupAttachment(this);
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


void UPS_HookComponent::WrapCableAddbyLast()
{
	if(!IsValid(GetOwner())) return;
	
	//Debug
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Yellow, FString::Printf(TEXT("Cable List: %i"), CableListArray.Num()));
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Yellow, FString::Printf(TEXT("Cable Attached: %i"), CableAttachedArray.Num()));
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Yellow, FString::Printf(TEXT("Cable Points Loc: %i"), CablePointLocations.Num()));
	
	if(CableListArray.IsValidIndex(CableListArray.Num()-1))
	{
		UCableComponent* localLatestCable = CableListArray[CableListArray.Num()-1];
		UCableComponent* localNewCable;
		FSCableWarpParams currentTraceCableWarp = TraceCableWrap(localLatestCable, false);

		//If hit nothing return;
		if(!currentTraceCableWarp.OutHit.bBlockingHit || !IsValid(currentTraceCableWarp.OutHit.GetComponent())) return;

		if(CheckPointLocation(currentTraceCableWarp.OutHit.Location, CableWrapErrorTolerance))
		{
			CableAttachedArray.AddUnique(localLatestCable);
			CablePointLocations.Add(currentTraceCableWarp.OutHit.Location);
			CablePointComponents.Add(currentTraceCableWarp.OutHit.GetComponent());

			const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::KeepWorld, true);
			localLatestCable->AttachToComponent(currentTraceCableWarp.OutHit.GetComponent(), AttachmentRule);
			localLatestCable->SetWorldLocation(currentTraceCableWarp.OutHit.Location, false, nullptr, ETeleportType::TeleportPhysics);
			
			//Add new cable component 
			localNewCable = Cast<UCableComponent>(GetOwner()->AddComponentByClass(UCableComponent::StaticClass(), false, FTransform(), false));
			localNewCable->RegisterComponent();
			GetOwner()->AddInstanceComponent(localNewCable);

			//Use zero length and single segment to make it tense
			localNewCable->CableLength = 0;
			localNewCable->NumSegments = 0;

			if(bCanUseSphereCaps)
			{
				GetOwner()->AddComponentByClass(UStaticMeshComponent::StaticClass(), true, );
			}
			
		}
		
		
	}
	else
	{
		//TODO:: For Add by First here
	}

		
}

void UPS_HookComponent::UnwrapCableAddbyLast()
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




