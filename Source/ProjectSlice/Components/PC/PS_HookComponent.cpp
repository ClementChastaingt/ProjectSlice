// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_HookComponent.h"


// Sets default values for this component's properties
UPS_HookComponent::UPS_HookComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UPS_HookComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UPS_HookComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UPrimitiveComponent* outComponent1;
	UPrimitiveComponent* outComponent2;
	FName outBoneName1, outBoneName2;

	GetConstrainedComponents(outComponent1,outBoneName1, outComponent2, outBoneName2);

	if(!IsValid(outComponent1) || !IsValid(outComponent2) || !IsValid(GetWorld()))
		return;

	DrawDebugLine(GetWorld(),outComponent1->GetComponentLocation(), outComponent2->GetComponentLocation(), FColor::Orange);
	
}

void UPS_HookComponent::SetConstrainedComponents(UPrimitiveComponent* Component1, FName BoneName1, UPrimitiveComponent* Component2, FName BoneName2)
{
	Super::SetConstrainedComponents(Component1,BoneName1,Component2,BoneName2);

	bIsConstrainted = true;
}

void UPS_HookComponent::BreakConstraint()
{
	Super::BreakConstraint();

	bIsConstrainted = false;
}



