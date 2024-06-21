// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PS_PlayerController.generated.h"

class UInputMappingContext;

/**
 *
 */
UCLASS(config=Game, BlueprintType, Blueprintable)
class PROJECTSLICE_API AProjectSlicePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	FVector2D GetMoveInput() const{return MoveInput;}

	void SetMoveInput(const FVector2D& moveInput){this->MoveInput = moveInput;}

protected:
	/** Input Mapping Context to be used for player input */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputMappingContext* InputMappingContext;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Input)
	FVector2D MoveInput = FVector2D::ZeroVector;

	// Begin Actor interface
	virtual void BeginPlay() override;
};
