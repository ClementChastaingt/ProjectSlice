// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PS_HookComponent.generated.h"


class UCableComponent;
class AProjectSliceCharacter;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_HookComponent : public USceneComponent
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* HookThrower = nullptr;
	
	UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* CableMesh = nullptr;

	UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	UPhysicsConstraintComponent* CableOriginAttach = nullptr;

	UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	UPhysicsConstraintComponent* CableTargetAttach = nullptr;

	UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* HookMesh = nullptr;

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
	bool IsConstrainted() const{return bIsConstrainted;}

	UFUNCTION()
	void Grapple(const FVector& sourceLocation);

	UFUNCTION()
	void DettachGrapple();

	UFUNCTION()
	void OnAttachWeaponEventReceived();

protected:
	
	UPROPERTY()
	bool bIsConstrainted = false;

		
	UPROPERTY(VisibleInstanceOnly, Category="Status")
	FHitResult CurrentHookHitResult;

private:
	//------------------

#pragma endregion General
	
};
