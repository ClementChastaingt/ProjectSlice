// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "GameFramework/PlayerController.h"
#include "PS_PlayerController.generated.h"

class UPS_PlayerCameraComponent;
class UPS_SlowmoComponent;
class UPS_ForceComponent;
class UPS_HookComponent;
class UPS_WeaponComponent;
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category ="Input|Mapping", meta=(AllowPrivateAccess = "true"))
	UInputMappingContext* InputMappingContext;

	/** DefaultMappingContext use in begin play */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Mapping", meta=(AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Mapping", meta=(AllowPrivateAccess = "true"))
	UInputMappingContext* FireMappingContext;
	
	// Begin Actor interface
	virtual void BeginPlay() override;

#pragma region Action
//------------------
	
#pragma region BaseMovement
	//------------------

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input")
	bool bIsUsingGamepad = true;
	
	UFUNCTION()
	void SetupMovementInputComponent();
	
protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Action")
	const UInputAction* IA_ActionKeyboard;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Action")
	const UInputAction* IA_AxisKeyboard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Action")
	const UInputAction* IA_ActionGamepad;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Action")
	const UInputAction* IA_AxisGamepad;
	
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
	
	//Keyboard && Gamepad mapping
	void OnIAActionKeyboardTriggered(const FInputActionInstance& inputActionInstance);
	void OnIAAxisKeyboardTriggered(const FInputActionInstance& inputActionInstance);
	void OnIAActionGamepadTriggered(const FInputActionInstance& inputActionInstance);
	void OnIAAxisGamepadTriggered(const FInputActionInstance& inputActionInstance);

	//Input action received func
	void OnMoveInputTriggered(const FInputActionValue& Value);
	void OnLookInputTriggered(const FInputActionValue& Value);
	
	void OnTurnRackInputTriggered(const FInputActionInstance& actionInstance);
	void OnTurnRackTargetedInputTriggered(const FInputActionInstance& actionInstance);
	void OnTurnRackTargetedInputCompleted();
	
	void OnForcePushInputStarted();
	void OnForcePushInputTriggered(const FInputActionInstance& actionInstancee);



private:
	UPROPERTY(Transient)
	UPS_PlayerCameraComponent* _CameraComp;
	
	

	//------------------
#pragma endregion BaseMovement

#pragma region MiscAction
	//------------------

public:
	UFUNCTION()
	void SetupMiscComponent();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category ="Input|Action|Player", meta = (AllowPrivateAccess = "true"))
	UInputAction* SlowmoAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category ="Input|Action|Player", meta = (AllowPrivateAccess = "true"))
	UInputAction* GlassesAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category ="Input|Action|Player", meta = (AllowPrivateAccess = "true"))
	UInputAction* StowAction;

private:
	UPROPERTY(Transient)
	UPS_SlowmoComponent* _SlowmoComp;
	
	//------------------
#pragma endregion MiscAction

#pragma region Weapon
	//------------------

public:
	UFUNCTION()
	void SetupWeaponInputComponent();

protected:
		
	/** Fire Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action|Weapon|Gun", meta=(AllowPrivateAccess = "true"))
	UInputAction* IA_Fire;

	/** Sight Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action|Weapon|Gun", meta=(AllowPrivateAccess = "true"))
	UInputAction* IA_TurnRack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action|Weapon|Gun", meta=(AllowPrivateAccess = "true"))
	UInputAction* IA_TurnRack_Targeted;

	/** Hook Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action|Weapon|Gun", meta=(AllowPrivateAccess = "true"))
	UInputAction* IA_Hook;

	/** Winder Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action|Weapon|Gun", meta=(AllowPrivateAccess = "true"))
	UInputAction* IA_WinderPull;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action|Weapon|Gun", meta=(AllowPrivateAccess = "true"))
	UInputAction* IA_WinderPush;

	/** ForcePush Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Action|Weapon|Gun", meta=(AllowPrivateAccess = "true"))
	UInputAction* IA_ForcePush;

private:
	
	UPROPERTY(Transient)
	UPS_WeaponComponent* _WeaponComp;

	UPROPERTY(Transient)
	UPS_HookComponent* _HookComp;

	UPROPERTY(Transient)
	UPS_ForceComponent* _ForceComp;
	
	//------------------
#pragma endregion Weapon	

//------------------
#pragma endregion Action

#pragma region Input
	//------------------

public:
	FORCEINLINE FVector2D GetLookInput() const{return LookInput;}
	
	FORCEINLINE FVector2D GetMoveInput() const{return MoveInput;}
	
	FORCEINLINE void SetMoveInput(const FVector2D& moveInput){this->MoveInput = moveInput;}

	FORCEINLINE double GetInputMaxSmoothingWeight() const{return InputMaxSmoothingWeight;}

	FORCEINLINE double GetInputMinSmoothingWeight() const{return InputMinSmoothingWeight;}
	
	FORCEINLINE bool CanMove() const{return bCanMove;}

	FORCEINLINE bool CanLook() const{return bCanLook;}

	FORCEINLINE bool CanCrouch() const{return bCanCrouch;}

	FORCEINLINE bool CanFire() const{return bCanFire;}

	FORCEINLINE void SetCanMove(bool bcanMove){this->bCanMove = bcanMove;}

	FORCEINLINE void SetCanLook(bool bcanLook){this->bCanLook = bcanLook;}

	FORCEINLINE void SetCanCrouch(bool bcanCrouch){this->bCanCrouch = bcanCrouch;}

	FORCEINLINE void SetCanFire(bool bcanFire){this->bCanFire = bcanFire;}
	
	FORCEINLINE void SetIsCrouchInputTrigger(const bool bisCrouchInputTrigger){this->bIsCrouchInputTrigger = bisCrouchInputTrigger;}
	
	FORCEINLINE bool IsCrouchInputTrigger() const{return bIsCrouchInputTrigger;}

	FORCEINLINE void SetIsTurnRackInputTrigger(const bool bisCrouchInputTrigger){this->bIsCrouchInputTrigger = bisCrouchInputTrigger;}
	
	FORCEINLINE bool IsCrouchInputPressed() const{return bIsCrouchInputTrigger;}

protected:
	/** Bool for force block Move */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Character)
	bool bCanMove = true;

	/** Bool for force block Look  */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Character)
	bool bCanLook = true;

	/** Bool for force block Crouch  */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Character)
	bool bCanCrouch = true;
	
	/** Bool for force block Fire  */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Character)
	bool bCanFire = true;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Status|Input|Look")
	FVector2D LookInput = FVector2D::ZeroVector;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Status|Input|Move")
	FVector2D MoveInput = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input|Move")
	double InputMaxSmoothingWeight = 0.5f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input|Move")
	double InputMinSmoothingWeight = 0.06f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Status|Input|Move")
	bool bIsCrouchInputTrigger = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Status|Input|Weapon|Rack")
	FInputActionInstance TurnRackInputActionInstance = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input|TurnRack", meta=(UIMin="0", ClampMin="0", ForceUnits="sec"))
	float InputTurnRackHoldThresholdTargetting = 1.0f;
	
private:
	/** Bool for force block TurnRack targetting  */
	UPROPERTY(Transient)
	bool _bTurnRackTargeted = false;

#pragma endregion Input

#pragma region Misc
	//------------------

protected:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugMode = false;
private:
	UPROPERTY(Transient)
	AProjectSliceCharacter* _CurrentPossessingPawn = nullptr;

#pragma endregion Misc
};
