// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_HookComponent.h"


// Sets default values for this component's properties
UPS_HookComponent::UPS_HookComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	HookMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookMesh"));
	HookMesh->SetupAttachment(this);
	HookMesh->SetEnableGravity(false);
}


// Called when the game starts
void UPS_HookComponent::BeginPlay()
{
	Super::BeginPlay();
	
	HookMesh->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	
}


// Called every frame
void UPS_HookComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if(bIsConstrainted)
	{
		//Set Position to HookAttach position
		HookMesh->SetWorldLocation(GetComponentLocation());
		
		//Cable
		UPrimitiveComponent* outComponent1;
		UPrimitiveComponent* outComponent2;
		FName outBoneName1, outBoneName2;

		GetConstrainedComponents(outComponent1,outBoneName1, outComponent2, outBoneName2);

		if(!IsValid(outComponent1) || !IsValid(outComponent2) || !IsValid(GetWorld()))
			return;

		DrawDebugLine(GetWorld(),outComponent1->GetComponentLocation(), outComponent2->GetComponentLocation(), FColor::Orange);
	
	}
	
}

void UPS_HookComponent::SetConstrainedComponents(UPrimitiveComponent* ComponentToAttach, FName  ComponentBoneName)
{
	HookMesh->SetSimulatePhysics(true);
	
	Super::SetConstrainedComponents(HookMesh,FName("None"),ComponentToAttach,ComponentBoneName);

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("SetConstraint")));

	bIsConstrainted = true;
}

void UPS_HookComponent::BreakConstraint()
{	
	Super::BreakConstraint();
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("BreakConstraint")));

	//Reset Hook Mesh
	HookMesh->SetSimulatePhysics(false);
	HookMesh->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	
	bIsConstrainted = false;
}



