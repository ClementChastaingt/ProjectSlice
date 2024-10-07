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
	FORCEINLINE UInputMappingContext* GetDefaultMappingContext() const{return DefaultMappingContext;}

	FORCEINLINE UInputMappingContext* GetFireMappingContext() const{return FireMappingContext;}

protected:
	/** Input Mapping Context to be used for player input */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category ="Input|Action")
	UInputMappingContext* InputMappingContext;

	/** DefaultMappingContext use in begin play */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action", meta=(AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action", meta=(AllowPrivateAccess = "true"))
	UInputMappingContext* FireMappingContext;
	
	// Begin Actor interface
	virtual void BeginPlay() override;


#pragma region Action
//------------------
	
#pragma region Player
	//------------------

public:
	FORCEINLINE UInputAction* GetMoveAction() const{return MoveAction;}

	FORCEINLINE UInputAction* GetLookAction() const{return LookAction;}
	
	FORCEINLINE UInputAction* GetJumpAction() const{return JumpAction;}

	FORCEINLINE UInputAction* GetCrouchAction() const{return CrouchAction;}
	
	FORCEINLINE UInputAction* GetDashAction() const{return DashAction;}
	
	FORCEINLINE UInputAction* GetSlowmoAction() const{return SlowmoAction;}

	FORCEINLINE UInputAction* GetStowAction() const{return StowAction;}
	
protected:
	/** Input Actions **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action|Player", meta=(AllowPrivateAccess = "true"))
	UInputAction* MoveAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category ="Input|Action|Player", meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action|Player", meta=(AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action|Player", meta=(AllowPrivateAccess = "true"))
	UInputAction* DashAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action|Player", meta=(AllowPrivateAccess = "true"))
	UInputAction* CrouchAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category ="Input|Action|Player", meta = (AllowPrivateAccess = "true"))
	UInputAction* SlowmoAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category ="Input|Action|Player", meta = (AllowPrivateAccess = "true"))
	UInputAction* StowAction;
	
private:
	//------------------

	//------------------
#pragma endregion Player

#pragma region Weapon
	//------------------

public:
	FORCEINLINE UInputAction* GetFireAction() const{return FireAction;}

	FORCEINLINE UInputAction* GetTurnRackAction() const{return TurnRackAction;}

	FORCEINLINE UInputAction* GetHookAction() const{return HookAction;}

	FORCEINLINE UInputAction* GetWinderAction() const{return WinderAction;}

protected:
	/** Fire Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action|Weapon|Gun", meta=(AllowPrivateAccess = "true"))
	UInputAction* FireAction;

	/** Sight Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action|Weapon|Gun", meta=(AllowPrivateAccess = "true"))
	UInputAction* TurnRackAction;

	/** Hook Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action|Weapon|Gun", meta=(AllowPrivateAccess = "true"))
	UInputAction* HookAction;

	/** Winder Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action|Weapon|Gun", meta=(AllowPrivateAccess = "true"))
	UInputAction* WinderAction;
private:
	//------------------

	//------------------
#pragma endregion Weapon	

//------------------
#pragma endregion Action

#pragma region Input
	//------------------

public:
	FORCEINLINE double GetInputMaxSmoothingWeight() const{return InputMaxSmoothingWeight;}

	FORCEINLINE double GetInputMinSmoothingWeight() const{return InputMinSmoothingWeight;}

	FORCEINLINE FVector2D GetMoveInput() const{return MoveInput;}
	
	FORCEINLINE void SetMoveInput(const FVector2D& moveInput){this->MoveInput = moveInput;}

	FORCEINLINE FVector2D GetRealMoveInput() const{return MoveInput;}
	
	FORCEINLINE void SetRealMoveInput(const FVector2D& moveInput){this->MoveInput = moveInput;}

	FORCEINLINE bool CanMove() const{return bCanMove;}

	FORCEINLINE bool CanLook() const{return bCanLook;}

	FORCEINLINE void SetCanMove(bool bcanMove){this->bCanMove = bcanMove;}

	FORCEINLINE void SetCanLook(bool bcanLook){this->bCanLook = bcanLook;}
	
	FORCEINLINE void SetIsCrouchInputTrigger(const bool bisCrouchInputTrigger){this->bIsCrouchInputTrigger = bisCrouchInputTrigger;}
	
	FORCEINLINE bool IsCrouchInputTrigger() const{return bIsCrouchInputTrigger;}

protected:
	/** Bool for force block Move */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Character)
	bool bCanMove = true;

	/** Bool for force block Look  */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Character)
	bool bCanLook = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Status|Input")
	FVector2D MoveInput = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Status|Input")
	FVector2D RealMoveInput = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Status|Input")
	bool bIsCrouchInputTrigger = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	double InputMaxSmoothingWeight = 0.5f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	double InputMinSmoothingWeight = 0.06f;
	
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
