// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_HookComponent.h"



// Sets default values for this component's properties
UPS_HookComponent::UPS_HookComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
	HookThrower = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookThrower"));
	CableOriginAttach = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("CableOriginConstraint"));
	CableOriginAttach->SetDisableCollision(true);
	CableMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CableMesh"));
	CableTargetAttach = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("CableTargetConstraint"));
	CableTargetAttach->SetDisableCollision(true);
	HookMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookMesh"));

}


// Called when the game starts
void UPS_HookComponent::BeginPlay()
{
	Super::BeginPlay();

	CableMesh->SetVisibility(false);
	CableMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	//Setup attachment
	// const FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, true);
	// HookMesh->AttachToComponent(this, AttachmentRules);
	// HookThrower->AttachToComponent(this, AttachmentRules);
	// CableMesh->AttachToComponent(this, AttachmentRules);
	// CableOriginAttach->AttachToComponent(CableMesh, AttachmentRules, FName("Bone_001_Socket"));
	// CableTargetAttach->AttachToComponent(CableMesh, AttachmentRules, FName("Bone_Socket"));

	//Setup cable constraint to HookThrower
	// HookThrower->SetSimulatePhysics(true);
	// HookThrower->SetEnableGravity(false);
	CableOriginAttach->SetConstrainedComponents(HookThrower, FName("None"),CableMesh, FName("Bone_001"));
}

// Called every frame
void UPS_HookComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if(bIsConstrainted)
	{
		//Set Position to HookAttach position
		//HookThrower->SetWorldLocation(GetComponentLocation());
		
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
		
	}
	
}


void UPS_HookComponent::GrappleObject(UPrimitiveComponent* cableTargetConstrainter, FName cableTargetBoneName)
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("SetConstraint")));

	//Setup Cable physic 
	CableMesh->SetVisibility(true);
	CableMesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	CableMesh->SetSimulatePhysics(true);

	//Setup constraint
	CableTargetAttach->SetConstrainedComponents(CableMesh, FName("Bone"),cableTargetConstrainter, cableTargetBoneName);

	bIsConstrainted = true;
}

void UPS_HookComponent::DettachGrapple()
{	
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("BreakConstraint")));

	CableMesh->SetVisibility(false);
	
	CableTargetAttach->BreakConstraint();
	
	bIsConstrainted = false;
}








