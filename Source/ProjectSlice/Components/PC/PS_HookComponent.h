// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PS_HookComponent.generated.h"


class UCableComponent;
class AProjectSliceCharacter;

USTRUCT(BlueprintType, Category = "Struct")
struct FSCableWarpParams
{
	GENERATED_BODY()

	FSCableWarpParams(){};
	
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	FHitResult OutHit = FHitResult();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	FVector CableStart = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	FVector CableEnd = FVector::ZeroVector;
	
};

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_HookComponent : public USceneComponent
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* HookThrower = nullptr;
	
	UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	UCableComponent* FirstCable= nullptr;
	
	// UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	// UStaticMeshComponent* HookMesh = nullptr;

public:
	/** Sets default values for this component's properties */
	UPS_HookComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bDebug = false;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;
	
	UPROPERTY(Transient)
	APlayerController* _PlayerController;
	

#pragma region General
	//------------------

public:
	UFUNCTION()
	void OnAttachWeapon();

	UFUNCTION()
	void OnInitWeaponEventReceived();

	bool IsConstrainted() const{return bIsConstrainted;}

	UFUNCTION()
	void Grapple(const FVector& sourceLocation);

	UFUNCTION()
	void DettachGrapple();


protected:
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status")
	bool bIsConstrainted = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters", meta=(UIMin="0", ClampMin="0", ForceUnits="cm"))
	float MaxHookDistance = 500.0f;
	
	UPROPERTY(VisibleInstanceOnly, Category="Status")
	FHitResult CurrentHookHitResult;

private:
	//------------------

#pragma endregion General

#pragma region Rope
	//------------------

public:
	//------------------
protected:
	//Status
	UPROPERTY(VisibleAnywhere, Category="Status|Cable|Rope", meta=(ToolTip="Cable list array, each added cable including the first one will be stored here"))
	TArray<UCableComponent*> CableListArray;

	UPROPERTY(VisibleAnywhere, Category="Status|Cable|Rope", meta=(ToolTip="Attached cables array, each attached cable point will be stored here."))
	TArray<UCableComponent*> CableAttachedArray;

	UPROPERTY(VisibleAnywhere, Category="Status|Cable|Cable", meta=(ToolTip="Cable caps array, each added cap will be stored here"))
	TArray<UStaticMeshComponent*> CableCapArray;

	UPROPERTY(VisibleAnywhere, Category="Status|Cable|Point", meta=(ToolTip="Cable points array, each hit component with the location will be stored here."))
	TArray<USceneComponent*> CablePointComponents;

	UPROPERTY(VisibleAnywhere, Category="Status|Cable|Point", meta=(ToolTip="Cable points array, each attached point will be stored here."))
	TArray<FVector> CablePointLocations;

	// UPROPERTY(VisibleAnywhere, Category="Status|Cable|Point", meta=(ToolTip="Cable points alphas, alpha value to detach for each point."))
	// TArray<float> CablePointUnwrapAlphaArray;

	//Parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Point", meta=(ToolTip="Use cable sphere caps, basically spawns dynamic mesh points on corners with new cable points to make smoother looks"))
	bool bCanUseSphereCaps = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Point", meta=(UIMin="0", ClampMin="0", ToolTip="Cable error tolerance for wrapping, so there will be no duplicate points around already added ones, keep this low for smooth wrapping."))
	float CableWrapErrorTolerance = 0.002;

	UFUNCTION()
	void WrapCableAddbyLast();

	UFUNCTION()
	void UnwrapCableAddbyLast();

	UFUNCTION()
	FSCableWarpParams TraceCableWrap(USceneComponent* cable, const bool bReverseLoc) const;

	//Check if this location is not existing already in "cable points locations", error tolerance to determine how close another wrap point can be added
	UFUNCTION()
	bool CheckPointLocation(const FVector& targetLoc, const float& errorTolerance);


	
private:
	//------------------

#pragma endregion Rope
};
