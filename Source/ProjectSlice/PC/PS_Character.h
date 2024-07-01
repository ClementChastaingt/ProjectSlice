// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PS_PlayerController.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "..\Components\PC\PS_WeaponComponent.h"
#include "ProjectSlice/Components/Common/PS_ComponentsManager.h"
#include "ProjectSlice/Components/PC/PS_ParkourComponent.h"
#include "PS_Character.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class PROJECTSLICE_API AProjectSliceCharacter : public ACharacter
{
	GENERATED_BODY()
	
	/** ComponentsManager */
	UPROPERTY(VisibleDefaultsOnly, Category=Manager)
	UPS_ComponentsManager* ComponentsManager;

	/** ParkourComponent */
	UPROPERTY(VisibleDefaultsOnly, Category=Mesh)
	UPS_ParkourComponent* ParkourComponent;

	/** WeaponComponent */
	UPROPERTY(VisibleDefaultsOnly, Category=Mesh)
	UPS_WeaponComponent* WeaponComponent;
	
	/** HookComponent */
	UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	UPS_HookComponent* HookComponent = nullptr;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;
	
public:
	AProjectSliceCharacter();

	virtual void TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

	UFUNCTION(BlueprintCallable)
	AProjectSlicePlayerController* GetPlayerController() const{return _PlayerController;}
	
	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	UPS_ComponentsManager* GetComponentsManager() const {return ComponentsManager; }

	/** Returns ParkourComponent **/
	UFUNCTION(BlueprintCallable)
	UPS_ParkourComponent* GetParkourComponent() const{return ParkourComponent;}

	/** Returns WeaponComponent **/
	UFUNCTION(BlueprintCallable)
	UPS_WeaponComponent* GetWeaponComponent() const{return WeaponComponent;}

	/** Returns HookComponent **/
	UFUNCTION(BlueprintCallable)
	UPS_HookComponent* GetHookComponent() const{return HookComponent;}
	
protected:
	virtual void BeginPlay();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug, meta=(ToolTip="Display debug location gizmo"))
	bool bDebugMovementTrail = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug, meta=(ToolTip="Display debug Player direction Arrow"))
	bool bDebugArrow = false;

private:
	UPROPERTY(Transient)
	AProjectSlicePlayerController* _PlayerController;

#pragma region Move
	//------------------
	

public:

protected:
	/** Movement **/
	virtual void OnMovementModeChanged(EMovementMode previousMovementMode, uint8 previousCustomMode) override;
	
	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

	/** Called for Jump input */
	virtual void Jump() override;

	/** Called for stop Jump input */
	virtual void StopJumping() override;

	/** Called for Crouch input */
	void Crouching();
	
	void OnCrouch();

	void Stooping();
	
	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for stop movement input */
	void StopMoving();

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon)
	bool bIsCrouch = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon)
	bool bIsStooping = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon)
	float StartStoopTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon)
	float StartCrouchHeight = TNumericLimits<float>().Lowest();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CharacterMovement|Crounch", meta=(ToolTip="Smooth crouch curve"))
	UCurveFloat* CrouchCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CharacterMovement|Crounch", meta=(ToolTip="Smooth crouch duration"))
	float SmoothCrouchDuration = 0.1f;

//------------------
#pragma endregion Move


#pragma region Weapon
	//------------------

public:
	/** Bool for AnimBP to switch to another animation set */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon)
	bool bHasRifle;

	/** Setter to set the bool */
	UFUNCTION(BlueprintCallable, Category = Weapon)
	void SetHasRifle(bool bNewHasRifle);

	/** Getter for the bool */
	UFUNCTION(BlueprintCallable, Category = Weapon)
	bool GetHasRifle();

protected:
	//------------------
private:
	//------------------

#pragma endregion Weapon
	
	
};

