// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_EnemyBase.h"


// Sets default values
APS_EnemyBase::APS_EnemyBase()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void APS_EnemyBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APS_EnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void APS_EnemyBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

