// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PS_HookComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PS_WeaponComponent.generated.h"

class UProceduralMeshComponent;
class AProjectSliceCharacter;

UCLASS(Blueprintable, BlueprintType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_WeaponComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* SightComponent = nullptr;

public:
	/** Sets default values for this component's properties */
	UPS_WeaponComponent();
	
	/** Attaches the actor to a FirstPersonCharacter */
	UFUNCTION(BlueprintCallable, Category="Weapon")
	void AttachWeapon(AProjectSliceCharacter* TargetCharacter);


protected:
	virtual void BeginPlay() override;
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	/** Ends gameplay for this component. */
	UFUNCTION()
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bDebug = false;

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
	
private:
	//------------------
	

#pragma endregion Input

#pragma region Fire
	//------------------

public:
	/** Make the weapon Fire a Slice */
	UFUNCTION()
	void Fire();
	
protected:
	
	UPROPERTY(VisibleInstanceOnly, Category="Status")
	FHitResult CurrentFireHitResult;
	
	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Weapon")
	USoundBase* FireSound;
	
	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Weapon")
	UAnimMontage* FireAnimation;
	
private:
	//------------------

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
	void Grapple();

	
private:
	UPROPERTY(Transient)
	UPS_HookComponent* _HookComponent = nullptr;

#pragma endregion Hook

#pragma region SightRack
	//------------------

public:
	UStaticMeshComponent* GetSightComponent() const{return SightComponent;}

protected:
	/** Make the weapon Turn his Rack */
	UFUNCTION()
	void TurnRack();
	
	/** Rack is placed in horizontal */
	UPROPERTY(VisibleInstanceOnly, Category="Status")
	bool bRackInHorizontal = true;
	
	UPROPERTY(VisibleInstanceOnly, Category="Status")
	bool bInterpRackRotation = false;

	UPROPERTY(VisibleInstanceOnly, Category="Status")
	float InterpRackRotStartTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleInstanceOnly, Category="Status")
	FRotator StartRackRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleInstanceOnly, Category="Status")
	FRotator TargetRackRotation = FRotator::ZeroRotator;
	
	/** Sight Rack Mesh default Transform*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight")
	FTransform SightDefaultTransform = FTransform(FRotator::ZeroRotator ,FVector(5.00,0.400,-2.0), FVector(0.050000,0.400000,0.050000));
	
	/** Gun muzzle's offset from the characters location ::UNUSED:: */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight")
	FVector MuzzleOffset;
	
	/** Rack Rotation interpolation params */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight|Move")
	float RackRotDuration = 0.5f;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight|Move")
	UCurveFloat* RackRotCurve;
	
	
private:
	//------------------

#pragma endregion SightRack
};
