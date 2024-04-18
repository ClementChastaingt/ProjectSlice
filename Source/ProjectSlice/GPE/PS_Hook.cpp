// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_Hook.h"

#include "ProjectSlice/Components/PC/PS_HookComponent.h"


// Sets default values
APS_Hook::APS_Hook()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//Create Component and Attach
	HookComponent = CreateDefaultSubobject<UPS_HookComponent>(TEXT("HookComponent"));
	HookComponent->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void APS_Hook::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APS_Hook::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

