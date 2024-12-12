// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PS_PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Logging/LogMacros.h"
#include "ProjectSlice/Character/PS_CharacterBase.h"
#include "ProjectSlice/Components/PC/PS_WeaponComponent.h"
#include "ProjectSlice/Components/Common/PS_ProceduralAnimComponent.h"
#include "ProjectSlice/Components/PC/PS_ParkourComponent.h"
#include "ProjectSlice/Components/PC/PS_SlowmoComponent.h"
#include "PS_Character.generated.h"

class UPS_PlayerCameraComponent;
class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class PROJECTSLICE_API AProjectSliceCharacter : public APS_CharacterBase
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleDefaultsOnly, Category=Root)
	USceneComponent* FirstPersonRoot;

	UPROPERTY(VisibleDefaultsOnly, Category=Root)
	USpringArmComponent* MeshRoot;

	UPROPERTY(VisibleDefaultsOnly, Category=Root)
	USpringArmComponent* CamRoot;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* CameraSkeletalMeshComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UPS_PlayerCameraComponent* FirstPersonCameraComponent;

	/** ProceduralAnimComponent */
	UPROPERTY(VisibleDefaultsOnly, Category=Manager)
	UPS_ProceduralAnimComponent* ProceduralAnimComponent;

	/** ParkourComponent */
	UPROPERTY(VisibleDefaultsOnly, Category=Mesh)
	UPS_SlowmoComponent* SlowmoComponent;

	/** ParkourComponent */
	UPROPERTY(VisibleDefaultsOnly, Category=Mesh)
	UPS_ParkourComponent* ParkourComponent;

	/** WeaponComponent */
	UPROPERTY(VisibleDefaultsOnly, Category=Mesh)
	UPS_WeaponComponent* WeaponComponent;
	
	/** HookComponent */
	UPROPERTY(VisibleInstanceOnly, Category=Mesh, meta = (AllowPrivateAccess = "true"))
	UPS_HookComponent* HookComponent = nullptr;
	
public:
	AProjectSliceCharacter();

	virtual void TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

	UFUNCTION(BlueprintCallable)
	AProjectSlicePlayerController* GetPlayerController() const{return _PlayerController;}
	
	/** Returns FirstPersonRoot subobject **/
	USceneComponent* GetFirstPersonRoot() const { return FirstPersonRoot; }
	
	/** Returns MeshRoot subobject **/
	USpringArmComponent* GetMeshRoot() const { return MeshRoot; }

	/** Returns CamRoot subobject **/
	USpringArmComponent* GetCamRoot() const { return CamRoot; }

	/** Returns CameraSkeletalMeshComponent subobject **/
	USkeletalMeshComponent* GetCameraSkeletalMeshComponent() const { return CameraSkeletalMeshComponent; }
	
	/** Returns FirstPersonCameraComponent subobject **/
	UPS_PlayerCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
	
	/** Returns ComponentsManager **/
	UFUNCTION(BlueprintCallable)
	UPS_ProceduralAnimComponent* GetProceduralAnimComponent() const {return ProceduralAnimComponent; }

	/** Returns ComponentsManager **/
	UFUNCTION(BlueprintCallable)
	UPS_SlowmoComponent* GetSlowmoComponent() const {return SlowmoComponent; }

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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug, meta=(ToolTip="Display debug location gizmo"))
	float MovementTrailDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug, meta=(ToolTip="Display Player current velocity"))
	bool bDebugVelocity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug, meta=(ToolTip="Display debug Player direction Arrow"))
	bool bDebugArrow = false;

private:
	UPROPERTY(Transient)
	AProjectSlicePlayerController* _PlayerController;

#pragma region Input
	//------------------

public:
	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface
	
protected:
	//------------------

private:
	//------------------

#pragma endregion Input

#pragma region Move
	//------------------

public:
	
	//Delegates
	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate OnJumpEvent;

	//Getters && Setters
	FORCEINLINE float GetDefaultMaxWalkSpeed() const{return _DefaultMaxWalkSpeed;}
	
	FORCEINLINE float GetDefaultMinAnalogSpeed() const{return _DefaultMinAnalogSpeed;}

	FORCEINLINE float GetDefaultGravityScale() const{return _DefaultGravityScale;}

	FORCEINLINE float GetDefaultAirControl() const{return _DefaultAirControl;}

	FORCEINLINE FVector GetOnJumpLocation() const{return OnJumpLocation;}

	FORCEINLINE FTimerHandle GetCoyoteTimerHandle() const{ return CoyoteTimerHandle;}

	/** Called for Crouch input */
	void Crouching();


protected:
	/** Movement **/
	virtual void OnMovementModeChanged(EMovementMode previousMovementMode, uint8 previousCustomMode) override;
	
	virtual void Landed(const FHitResult& Hit) override;

	/** Called for Jump input */
	virtual bool CanJumpInternal_Implementation() const override;

	/** Called for Jump input */
	virtual void Jump() override;

	virtual void OnJumped_Implementation() override;

	/** Called for stop Jump input */
	virtual void StopJumping() override;

	void CoyoteTimeStart();

	void CoyoteTimeStop();
	
	void Dash();
	
	//Crouch functions override
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	
	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for stop movement input */
	void StopMoving() const;

	/** Called for looking input */
	void Look(const FInputActionValue& Value);
		
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Status")
	FVector OnJumpLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Movement|Status|Coyote", meta=(ToolTip="Coyote timer handler"))
	FTimerHandle CoyoteTimerHandle;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Jump|Coyote")
	float CoyoteTime = 0.35f;

private:
	UPROPERTY(Transient)
	float _DefaultMaxWalkSpeed;

	UPROPERTY(Transient)
	float _DefaultMinAnalogSpeed;
	
	UPROPERTY(Transient)
	float _DefaultGravityScale;

	UPROPERTY(Transient)
	float _DefaultAirControl;
	
#pragma endregion Move

#pragma region Slowmo
	//------------------

public:

	UFUNCTION()
	void SetPlayerTimeDilation(const float newDilation);
	
protected:
	/** Called for Slowmo input */
	UFUNCTION()
	void Slowmo();
	
private:
	//------------------
#pragma endregion Slowmo

#pragma region Glasses
	//------------------
	
public:
	UFUNCTION()
	void Glasses();
protected:


private:
	UPROPERTY(Transient)
	bool _bGlassesActive = false;
	
#pragma endregion Glasses


#pragma region Weapon
	//------------------

public:

	UFUNCTION(BlueprintCallable, Category=Weapon)
	FORCEINLINE bool GetHasRifle() const {return bHasRifle;}

	UFUNCTION(BlueprintCallable, Category=Weapon)
	FORCEINLINE void SetHasRifle(const bool bhasRifle){this->bHasRifle = bhasRifle;}

	UFUNCTION(BlueprintCallable, Category=Weapon)
	FORCEINLINE bool IsIsWeaponStow() const{return bIsWeaponStow;}

protected:
	
	/** Called for Slowmo input */
	UFUNCTION()
	void Stow();

	/** Bool for AnimBP to switch to another animation set */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon)
	bool bHasRifle = false;

	/** Bool for AnimBP to switch to another animation set */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon)
	bool bIsWeaponStow = false;
private:
	//------------------

#pragma endregion Weapon
	
	
};

