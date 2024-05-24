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
	HookThrower->SetCollisionProfileName(FName("BlockAllDynamic"), true);
	
	CableMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CableMesh"));
	CableOriginAttach = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("CableOriginConstraint"));
	CableTargetAttach = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("CableTargetConstraint"));
	
	HookMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookMesh"));
	HookMesh->SetSimulatePhysics(false);

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
	
}

void UPS_HookComponent::OnAttachWeapon()
{
	//Setup HookThrower
	HookThrower->SetupAttachment(this);
	
	//Setup Cable
	CableMesh->SetupAttachment(this);
	//CableMesh->SetCollisionProfileName(FName("NoCollision"), true);
	CableMesh->SetSimulatePhysics(false);

	//Setup cable constraint
	CableOriginAttach->SetDisableCollision(true);
	CableOriginAttach->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
	CableOriginAttach->SetAngularVelocityDriveTwistAndSwing(true, true);

	CableTargetAttach->SetDisableCollision(true);
	CableTargetAttach->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
	CableTargetAttach->SetAngularVelocityDriveTwistAndSwing(true, true);

	CableOriginAttach->SetupAttachment(this);
	CableTargetAttach->SetupAttachment(this);
	
	//Setup HookMesh
	HookMesh->SetCollisionProfileName(FName("NoCollision"), true);
	HookMesh->SetupAttachment(CableMesh, FName("RopeEnd"));	
}


void UPS_HookComponent::OnInitWeaponEventReceived()
{
	
	DrawDebugPoint(GetWorld(),HookThrower->GetComponentLocation(), 10.0f, FColor::Cyan, true);

	//Setup cable constraint attach
	const FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, true);
	CableOriginAttach->AttachToComponent(CableMesh, AttachmentRules, FName("RopeStart"));
	CableTargetAttach->AttachToComponent(HookMesh, AttachmentRules);

	//Setup Origin Constraint
	CableOriginAttach->SetConstrainedComponents(CableMesh, FName("Bone"), HookThrower, FName("None"));
	
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

	//If Hook too Far Away
	if(CurrentHookHitResult.Distance > MaxHookDistance)
	{
		UE_LOG(LogTemp, Log, TEXT("UPS_HookComponent:: Can't Attach, Cable too short"));
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, FString::Printf(TEXT("Cable too short")));
		return;
	}

	//If Attach
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, FString::Printf(TEXT("SetConstraint")));

	CurrentHookHitResult.GetComponent()->SetWorldLocation(CableTargetAttach->GetComponentLocation());
	//CableMesh->SetCollisionProfileName(FName("PhysicsActor"), true);
	CableMesh->SetSimulatePhysics(true);
	CableTargetAttach->SetConstrainedComponents(CableMesh, FName("Bone_001"),CurrentHookHitResult.GetComponent(), FName("None"));

	bIsConstrainted = true;
}

void UPS_HookComponent::DettachGrapple()
{	
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, FString::Printf(TEXT("BreakConstraint")));
	
	CableTargetAttach->BreakConstraint();
	//CableMesh->SetCollisionProfileName(FName("NoCollision"), true);
	CableMesh->SetSimulatePhysics(false);
	CableMesh->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	CableMesh->SetRelativeRotation(FRotator(90,0,0), false, nullptr, ETeleportType::TeleportPhysics);
		
	bIsConstrainted = false;
}



