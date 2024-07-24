// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PS_CharacterMovementComponent.generated.h"

UENUM(BlueprintType)
enum ECustomMovementMode
{
	CMOVE_None		UMETA(Hidden),
	CMOVE_Slide		UMETA(DisplayName ="Slide"),
	CMOVE_WallRun	UMETA(DisplayName ="WallRun"),
	CMOVE_Climbing	UMETA(DisplayName ="Climbing"),
	CMOVE_MAX		UMETA(Hidden),
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_CharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_CharacterMovementComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void PhysCustom(float deltaTime, int32 Iterations) override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
};
