// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_SlowmoComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ProjectSlice/PC/PS_Character.h"


// Sets default values for this component's properties
UPS_SlowmoComponent::UPS_SlowmoComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UPS_SlowmoComponent::BeginPlay()
{
	Super::BeginPlay();

	//Init default Variable
	_PlayerCharacter = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if(!IsValid(_PlayerCharacter)) return;
	
	_PlayerController = Cast<AProjectSlicePlayerController>(_PlayerCharacter->GetController());
	if(!IsValid(_PlayerController))return;

}


// Called every frame
void UPS_SlowmoComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UPS_SlowmoComponent::OnStartSlowmo()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(GetWorld())) return;

	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S"), __FUNCTION__);

	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.5f);
	_PlayerCharacter->CustomTimeDilation = 0.8f;
	
}



