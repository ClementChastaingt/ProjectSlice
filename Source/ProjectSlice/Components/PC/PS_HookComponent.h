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
	bool bBool = true;
	
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
	UPROPERTY(VisibleAnywhere, Category="Cable|Rope", meta=(ToolTip="Cable list array, each added cable including the first one will be stored here"))
	TArray<UCableComponent*> CableListArray;

	UPROPERTY(VisibleAnywhere, Category="Cable|Rope", meta=(ToolTip="Attached cables array, each attached cable point will be stored here."))
	TArray<UCableComponent*> CableAttachedArray;

	UPROPERTY(VisibleAnywhere, Category="Cable|Point", meta=(ToolTip="Cable caps array, each added cap will be stored here"))
	TArray<UStaticMeshComponent*> CableCapArray;

	UPROPERTY(VisibleAnywhere, Category="Cable|Point", meta=(ToolTip="Cable points array, each attached point will be stored here."))
	TArray<FVector> CablePointLocations;

	UPROPERTY(VisibleAnywhere, Category="Cable", meta=(ToolTip="Cable points alphas, alpha value to detach for each point."))
	TArray<float> CablePointUnwrapAlphaArray;

	UFUNCTION()
	void WrapCableAddbyLast();

	UFUNCTION()
	void UnwrapCableAddbyLast();

	UFUNCTION()
	void TraceCableWrap(USceneComponent* cable, const bool bReverseLoc);


	
private:
	//------------------

#pragma endregion Rope
};
