// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "GameFramework/PlayerController.h"
#include "PS_PlayerController.generated.h"

class AProjectSliceCharacter;
class UInputMappingContext;

/**
 *
 */
UCLASS(config=Game, BlueprintType, Blueprintable)
class PROJECTSLICE_API AProjectSlicePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
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

	// Begin Actor interface
	virtual void BeginPlay() override;

#pragma region Input
	//------------------

public:
	FVector2D GetMoveInput() const{return MoveInput;}
	
	void SetMoveInput(const FVector2D& moveInput){this->MoveInput = moveInput;}

	bool CanMove() const{return bCanMove;}

	bool CanLook() const{return bCanLook;}

	void SetCanMove(bool bcanMove){this->bCanMove = bcanMove;}

	void SetCanLook(bool bcanLook){this->bCanLook = bcanLook;}

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Input)
	FVector2D MoveInput = FVector2D::ZeroVector;

	/** Bool for force block Move */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Character)
	bool bCanMove = true;

	/** Bool for force block Look  */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Character)
	bool bCanLook = true;
private:
	//------------------

#pragma endregion Input


#pragma region Misc
	//------------------

public:
	//------------------
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Status")
	AProjectSliceCharacter* CurrentPossessingPawn = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugMode = false;
private:
	//------------------

#pragma endregion Misc
};
