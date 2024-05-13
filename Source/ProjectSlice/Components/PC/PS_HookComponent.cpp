// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_HookComponent.h"

#include "../../../../../../../../../../../Program Files/Epic Games/UE_5.3/Engine/Plugins/Runtime/CableComponent/Source/CableComponent/Classes/CableComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectSlice/PC/PS_Character.h"


// Sets default values for this component's properties
UPS_HookComponent::UPS_HookComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	HookThrower = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookThrower"));
	HookThrower->SetupAttachment(this);
	HookThrower->SetSimulatePhysics(false);
	
	CableMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CableMesh"));
	CableMesh->SetupAttachment(this);
	
	CableOriginAttach = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("CableOriginConstraint"));
	CableOriginAttach->SetupAttachment(this);
	CableOriginAttach->SetDisableCollision(true);

	CableTargetAttach = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("CableTargetConstraint"));
	CableTargetAttach->SetupAttachment(this);
	CableTargetAttach->SetDisableCollision(true);
	
	HookMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookMesh"));
	HookMesh->SetupAttachment(this);
	HookMesh->SetSimulatePhysics(false);

}


// Called when the game starts
void UPS_HookComponent::BeginPlay()
{
	Super::BeginPlay();

	_PlayerCharacter = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	_PlayerController = Cast<APlayerController>(_PlayerCharacter->GetController());
	
}

// Called every frame
void UPS_HookComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

void UPS_HookComponent::OnAttachWeaponEventReceived()
{
	
	//Setup HookThrower
	HookThrower->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	//HookThrower->SetCollisionProfileName(FName("NoCollision"), true);

	//Setup Cable
	CableOriginAttach->AttachToComponent(HookThrower, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	DrawDebugPoint(GetWorld(),_PlayerCharacter->GetWeaponComponent()->GetSightComponent()->GetComponentLocation(), 10.0f, FColor::Cyan, true);
	CableMesh->SetWorldLocation(_PlayerCharacter->GetWeaponComponent()->GetSightComponent()->GetComponentLocation());
	//CableMesh->AttachToComponent(HookThrower, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	
	//Setup HookMesh
	HookMesh->SetCollisionProfileName(FName("NoCollision"), true);
	HookMesh->AttachToComponent(CableMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("Bone"));
	// HookMesh->AttachToComponent(CableMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("CableEnd"));
	
	//Setup cable constraint to HookThrower
	//TODO :: Problem Socket but not BONE
	CableMesh->SetSimulatePhysics(true);
	CableOriginAttach->SetConstrainedComponents(HookThrower, FName("None"),CableMesh, FName("Bone_001"));
	//CableOriginAttach->SetConstrainedComponents(HookThrower, FName("None"),CableMesh, FName("CableStart"));
}


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
	
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("SetConstraint")));
	CableMesh->SetWorldLocation(sourceLocation);
	//CableMesh->SetAttachEndToComponent(CurrentHookHitResult.GetComponent());
	CableTargetAttach->AttachToComponent(CurrentHookHitResult.GetComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	//TODO :: Problem Socket but not BONE
	CableTargetAttach->SetConstrainedComponents(CableMesh, FName("Bone"),CurrentHookHitResult.GetComponent(), FName("None"));
	//CableTargetAttach->SetConstrainedComponents(CableMesh, FName("CableEnd"),CurrentHookHitResult.GetComponent(), FName("None"));

	bIsConstrainted = true;
}

void UPS_HookComponent::DettachGrapple()
{	
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("BreakConstraint")));
	
	CableTargetAttach->BreakConstraint();
	
	//Reset Hook Mesh
	//HookMesh->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	
	bIsConstrainted = false;
}



