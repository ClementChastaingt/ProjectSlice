// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PS_HookComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ProjectSlice/Data/PS_Delegates.h"
#include "PS_WeaponComponent.generated.h"

class UProceduralMeshComponent;
class AProjectSliceCharacter;

UCLASS(Blueprintable, BlueprintType, ClassGroup=(Component), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_WeaponComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* SightMesh = nullptr;

public:
	/** Sets default values for this component's properties */
	UPS_WeaponComponent();
	
	/** Attaches the actor to a FirstPersonCharacter */
	UFUNCTION(BlueprintCallable, Category="Weapon")
	void AttachWeapon(AProjectSliceCharacter* TargetCharacter);

	UFUNCTION(BlueprintCallable, Category="Weapon")
	void InitWeapon(AProjectSliceCharacter* Target_PlayerCharacter);
	
	FOnPSDelegate OnWeaponInit;

protected:
	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	/** Ends gameplay for this component. */
	UFUNCTION()
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebug = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebugSightShader = false;
	
private:
	/** The Character holding this weapon*/
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;

	UPROPERTY(Transient)
	APlayerController* _PlayerController;

#pragma region Input
	//------------------

public:
	//------------------
	
protected:	
	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameters|Input", meta=(AllowPrivateAccess = "true"))
	class UInputMappingContext* FireMappingContext;

	/** Fire Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameters|Input", meta=(AllowPrivateAccess = "true"))
	class UInputAction* FireAction;

	/** Sight Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameters|Input", meta=(AllowPrivateAccess = "true"))
	class UInputAction* TurnRackAction;

	/** Hook Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameters|Input", meta=(AllowPrivateAccess = "true"))
	class UInputAction* HookAction;

	/** Winder Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameters|Input", meta=(AllowPrivateAccess = "true"))
	class UInputAction* WinderAction;
	
	
private:
	//------------------
	

#pragma endregion Input

#pragma region Fire
	//------------------

public:

	UFUNCTION()
	void FireTriggered();
	
	/** Make the weapon Fire a Slice */
	UFUNCTION()
	void Fire();

protected:
	
	UPROPERTY(VisibleInstanceOnly, Category="Status")
	bool bIsHoldingFire = false;
	
	UPROPERTY(VisibleInstanceOnly, Category="Status")
	FHitResult CurrentFireHitResult;

	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Weapon", meta=(UIMin="0", ClampMin="0", ForceUnits="cm"))
	float MaxFireDistance = 5000.0f;
			
	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Weapon")
	USoundBase* FireSound;
	
	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Weapon")
	UAnimMontage* FireAnimation;

#pragma endregion Fire

#pragma region Slice
	//__________________________________________________

public:
	//__________________________________________________

protected:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Slice")
	UMaterialInterface* HalfSectionMaterial = nullptr;

private:
	//__________________________________________________

#pragma endregion Slice

#pragma region Hook
	//------------------

public:
	
	/** Make the weapon Fire a Hook */
	UFUNCTION()
	void HookObject();

	UFUNCTION()
	void WindeHook();

	UFUNCTION()
	void StopWindeHook();

private:
	UPROPERTY(Transient)
	UPS_HookComponent* _HookComponent = nullptr;

#pragma endregion Hook

#pragma region SightRack
	//------------------

public:
	UFUNCTION(BlueprintCallable)
	UStaticMeshComponent* GetSightMeshComponent() const{return SightMesh;}

protected:
	/** Make the weapon Turn his Rack */
	UFUNCTION()
	void TurnRack();

	UFUNCTION()
	void SightMeshRotation();
	
	/**Sight slice shader */
	UFUNCTION()
	void SightShaderTick();
	
	UFUNCTION()
	void ResetSightRackProperties();

	UFUNCTION()
	void ForceInitSliceBump();

	UFUNCTION()
	void SetupSliceBump();
	
	UFUNCTION()
	void SliceBump();
	
	/** Rack is placed in horizontal */
	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Mesh")
	bool bRackInHorizontal = true;
	
	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Mesh")
	bool bInterpRackRotation = false;

	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Mesh")
	float InterpRackRotStartTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Mesh")
	FRotator RackDefaultRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Mesh")
	FRotator StartRackRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Mesh")
	FRotator TargetRackRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Mesh")
	bool bSliceBumping = false;
	
	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Shader")
	float StartSliceBumpTimestamp = TNumericLimits<float>().Min();

	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Shader")
	float CurrentCurveAlpha = 0;
	
	/** Gun muzzle's offset from the characters location ::UNUSED:: */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight")
	FVector MuzzleOffset;
	
	/** Rack Rotation interpolation params */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight|Move")
	float RackRotDuration = 0.5f;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight|Move")
	UCurveFloat* RackRotCurve;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight|Shader")
	float SliceBumpDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight|Shader")
	UCurveFloat* SliceBumpCurve;

private:
	UPROPERTY(Transient)
	UPrimitiveComponent* _CurrentSightedComponent;
	
	UPROPERTY(Transient)
	UMaterialInterface* _CurrentSightedBaseMat;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* _CurrentSightedMatInst;


#pragma endregion SightRack
};
