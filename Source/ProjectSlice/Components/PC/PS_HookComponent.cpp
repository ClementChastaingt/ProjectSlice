// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_HookComponent.h"

#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectSlice/PC/PS_Character.h"


// Sets default values for this component's properties
UPS_HookComponent::UPS_HookComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	HookThrower = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookThrower"));
	HookThrower->SetCollisionProfileName(FName("NoCollision"), true);
	HookThrower->SetupAttachment(this);
	HookThrower->SetSimulatePhysics(false);
	
	CableMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CableMesh"));
	CableMesh->SetupAttachment(this);
	
	CableOriginAttach = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("CableOriginConstraint"));
	CableOriginAttach->SetDisableCollision(true);
	CableOriginAttach->SetupAttachment(this);

	CableTargetAttach = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("CableTargetConstraint"));
	CableTargetAttach->SetDisableCollision(true);
	CableTargetAttach->SetupAttachment(this);
	
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
	DrawDebugPoint(GetWorld(),_PlayerCharacter->GetFirstPersonCameraComponent()->GetComponentLocation(), 10.0f, FColor::Cyan, true);

	//Setup HookThrower
	HookThrower->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	
	//Setup Cable
	CableMesh->SetCollisionProfileName(FName("PhysicsActor"), true);
	CableMesh->SetSimulatePhysics(true);
	//CableMesh->SetWorldLocation(_PlayerCharacter->GetFirstPersonCameraComponent()->GetComponentLocation());
	CableMesh->SetWorldLocation(HookThrower->GetComponentLocation() + CableMesh->GetPlacementExtent().BoxExtent);
	//CableMesh->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	
	
	//Setup HookMesh
	HookMesh->SetCollisionProfileName(FName("NoCollision"), true);
	HookMesh->AttachToComponent(CableMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("RopeEnd"));
	// HookMesh->AttachToComponent(CableMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("CableEnd"));
	
	//Setup cable constraint
	//CableOriginAttach->AttachToComponent(HookThrower, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	//CableOriginAttach->SetWorldLocation(HookThrower->GetComponentLocation());
	CableOriginAttach->SetWorldLocation(_PlayerCharacter->GetFirstPersonCameraComponent()->GetComponentLocation());
	
	//CableTargetAttach->SetWorldLocation(CableMesh->GetSocketLocation(FName("Bone")));

	
	CableOriginAttach->SetDisableCollision(true);
	CableOriginAttach->SetConstrainedComponents(HookThrower, FName("None"),CableMesh, FName("Bone"));
	//CableOriginAttach->SetConstrainedComponents(HookThrower, FName("None"),CableMesh, FName("CableStart"));

	CableTargetAttach->SetDisableCollision(true);
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
	CableTargetAttach->SetConstrainedComponents(CableMesh, FName("Bone_001"),CurrentHookHitResult.GetComponent(), FName("None"));
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



