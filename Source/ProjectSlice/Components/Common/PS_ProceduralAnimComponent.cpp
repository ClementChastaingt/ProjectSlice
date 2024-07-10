// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ProceduralAnimComponent.h"

#include "Kismet/GameplayStatics.h"
#include "ProjectSlice/PC/PS_Character.h"
#include "ProjectSlice/PC/PS_PlayerController.h"


// Sets default values for this component's properties
UPS_ProceduralAnimComponent::UPS_ProceduralAnimComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UPS_ProceduralAnimComponent::BeginPlay()
{
	Super::BeginPlay();

	//Init default Variable
	if(!IsValid(GetWorld())) return;
	
	_PlayerCharacter = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if(!IsValid(_PlayerCharacter)) return;
	
	_PlayerController = Cast<AProjectSlicePlayerController>(_PlayerCharacter->GetController());
	if(!IsValid(_PlayerController))return;
	
}


// Called every frame
void UPS_ProceduralAnimComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

