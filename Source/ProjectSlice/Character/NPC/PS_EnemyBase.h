// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProjectSlice/Character/PS_CharacterBase.h"
#include "PS_EnemyBase.generated.h"

UCLASS()
class PROJECTSLICE_API APS_EnemyBase : public APS_CharacterBase
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	APS_EnemyBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
