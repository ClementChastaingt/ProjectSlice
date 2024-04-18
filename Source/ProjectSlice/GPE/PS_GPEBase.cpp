// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_GPEBase.h"


// Sets default values
APS_GPEBase::APS_GPEBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void APS_GPEBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APS_GPEBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

