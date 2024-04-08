// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PS_SlicedComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PS_WeaponComponent.generated.h"

class UProceduralMeshComponent;
class AProjectSliceCharacter;

UCLASS(Blueprintable, BlueprintType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECTSLICE_API UPS_WeaponComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* SightComponent = nullptr;

public:
	/** Sets default values for this component's properties */
	UPS_WeaponComponent();

	AProjectSliceCharacter* GetPlayerCharacter() const{return _PlayerCharacter;}

	APlayerController* GetPlayerController() const{return _PlayerController;}

protected:
	UFUNCTION()
	virtual void BeginPlay() override;
	
	/** Ends gameplay for this component. */
	UFUNCTION()
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** The Character holding this weapon*/
	AProjectSliceCharacter* _PlayerCharacter;

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
	
private:
	//------------------
	

#pragma endregion Input

#pragma region Fire
	//------------------

public:
	/** Attaches the actor to a FirstPersonCharacter */
	UFUNCTION(BlueprintCallable, Category="Weapon")
	void AttachWeapon(AProjectSliceCharacter* TargetCharacter);

	/** Make the weapon Fire a Projectile */
	UFUNCTION(BlueprintCallable, Category="Weapon")
	void Fire();
	
protected:
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
	UPROPERTY(VisibleInstanceOnly, Category="Status")
	FHitResult CurrentFireHitResult;

	UPROPERTY(VisibleInstanceOnly, Category="Status")
	UPS_SlicedComponent* CurrentSlicedComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Slice")
	UMaterialInterface* HalfSectionMaterial = nullptr;

private:
	//__________________________________________________

#pragma endregion Slice
	
#pragma region Sight
	//------------------

public:
	//------------------
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight")
	FVector SightMeshLocation = FVector(5.00,0.400,-2.0);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight")
	FVector SightMeshScale = FVector(0.050000,0.400000,0.050000);
	
	/** Gun muzzle's offset from the characters location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight")
	FVector MuzzleOffset;
	
private:
	//------------------

#pragma endregion Sight
};
