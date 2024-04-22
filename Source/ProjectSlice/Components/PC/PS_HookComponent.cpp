// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_HookComponent.h"

#include "MovieSceneTracksComponentTypes.h"


// Sets default values for this component's properties
UPS_HookComponent::UPS_HookComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	//Create components
	HookThrower = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookThrower"));
	HookMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookMesh"));
	CableMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CableMesh"));
	CableOriginAttach = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("CableOriginConstraint"));
	CableTargetAttach = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("CableTargetConstraint"));

	//Preset physic
	CableOriginAttach->SetDisableCollision(true);
	CableTargetAttach->SetDisableCollision(true);
	HookMesh->SetSimulatePhysics(false);
	HookThrower->SetSimulatePhysics(false);

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


void UPS_HookComponent::OnAttachWeaponEventRecieved()
{
	
	//Setup attachment
	HookThrower->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	HookMesh->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	CableMesh->SetWorldLocation(HookThrower->GetComponentLocation());
	CableMesh->SetSimulatePhysics(true);
	CableOriginAttach->SetWorldLocation(HookThrower->GetComponentLocation());


	//Setup cable constraint to HookThrower
	CableOriginAttach->SetConstrainedComponents(HookThrower, FName("None"),CableMesh, FName("Bone_001"));
}

void UPS_HookComponent::GrappleObject(UPrimitiveComponent* cableTargetConstrainter, FName cableTargetBoneName)
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("SetConstraint")));
	
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



