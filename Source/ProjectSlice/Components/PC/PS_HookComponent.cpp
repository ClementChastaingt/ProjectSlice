// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_HookComponent.h"


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
	
}

// Called every frame
void UPS_HookComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if(bIsConstrainted)
	{
		// //Set Position to HookAttach position
		// HookMesh->SetWorldLocation(GetComponentLocation());
		//
		// //Cable
		// UPrimitiveComponent* outComponent1;
		// UPrimitiveComponent* outComponent2;
		// FName outBoneName1, outBoneName2;
		//
		// GetConstrainedComponents(outComponent1,outBoneName1, outComponent2, outBoneName2);
		//
		// if(!IsValid(outComponent1) || !IsValid(outComponent2) || !IsValid(GetWorld()))
		// 	return;
		//
		// DrawDebugLine(GetWorld(),outComponent1->GetComponentLocation(), outComponent2->GetComponentLocation(), FColor::Orange);
		//
	}
	
}

void UPS_HookComponent::OnAttachWeaponEventReceived()
{
	
	//Setup attachment
	HookThrower->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	//HookThrower->SetCollisionProfileName(FName("NoCollision"), true);
	
	CableOriginAttach->AttachToComponent(HookThrower, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	CableMesh->SetWorldLocation(HookThrower->GetComponentLocation() + CableMesh->GetPlacementExtent().BoxExtent);
	//CableMesh->SetWorldLocation(HookThrower->GetComponentLocation() + FVector(0,0,90));
	CableTargetAttach->AttachToComponent(HookThrower, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	HookMesh->SetCollisionProfileName(FName("NoCollision"), true);
	HookMesh->AttachToComponent(CableMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("Bone"));


	//Setup cable constraint to HookThrower
	CableMesh->SetSimulatePhysics(true);
	CableOriginAttach->SetConstrainedComponents(HookThrower, FName("None"),CableMesh, FName("Bone_001"));
}


void UPS_HookComponent::GrappleObject(UPrimitiveComponent* cableTargetConstrainter, FName cableTargetBoneName)
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("SetConstraint")));

	CableTargetAttach->AttachToComponent(cableTargetConstrainter, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	CableTargetAttach->SetConstrainedComponents(CableMesh, FName("Bone"),cableTargetConstrainter, cableTargetBoneName);

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



