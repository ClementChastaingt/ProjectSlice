// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PS_CharacterBase.generated.h"

UCLASS()
class PROJECTSLICE_API APS_CharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	APS_CharacterBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;


#pragma region Utilities
	//------------------

public:
	
	/*
	 * @brief Get the input character foot placement
	 * @param character: tested character reference
	 * @return Foot placement location
	 */
	FVector GetFootPlacementLoc() const;

	/*
	 * @brief Get the input character foot placement
	 * @param character: tested character reference
	 * @return Foot placement location
	*/
	bool IsInAir() const;
	
protected:
	//------------------
private:
	//------------------

#pragma endregion Utilities
};
