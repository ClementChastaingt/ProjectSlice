// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
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

	UInputMappingContext* GetDefaultMappingContext() const{return DefaultMappingContext;}

	UInputAction* GetJumpAction() const{return JumpAction;}

	UInputAction* GetCrouchAction() const{return CrouchAction;}

	UInputAction* GetMoveAction() const{return MoveAction;}

	UInputAction* GetLookAction() const{return LookAction;}

protected:
	/** Input Mapping Context to be used for player input */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputMappingContext* InputMappingContext;

	/** DefaultMappingContext use in begin play */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Input Actions **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* CrouchAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* MoveAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Input)
	FVector2D MoveInput = FVector2D::ZeroVector;

	// Begin Actor interface
	virtual void BeginPlay() override;
};
