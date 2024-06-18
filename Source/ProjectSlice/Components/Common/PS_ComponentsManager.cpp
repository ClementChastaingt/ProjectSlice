// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ComponentsManager.h"

#include "Kismet/GameplayStatics.h"
#include "ProjectSlice/Components/PC/PS_ParkourComponent.h"
#include "ProjectSlice/PC/PS_Character.h"


// Sets default values for this component's properties
UPS_ComponentsManager::UPS_ComponentsManager()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	//Init default Variable
	if(!IsValid(GetOwner())) return;
	
	_PlayerCharacter = Cast<AProjectSliceCharacter>(GetOwner());
	if(!IsValid(_PlayerCharacter)) return;
	
	//TODO :: Override component on player by BP instanced in ComponentArray 
	// if(IsValid(_PlayerCharacter->GetParkourComponent()))
	// {
	// 	return;
	// }

}




// Called when the game starts
void UPS_ComponentsManager::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UPS_ComponentsManager::TickComponent(float DeltaTime, ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

