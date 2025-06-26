// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_FieldSystemActor.h"


// Sets default values
APS_FieldSystemActor::APS_FieldSystemActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void APS_FieldSystemActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APS_FieldSystemActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}


