// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PS_HookComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_HookComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_HookComponent();

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

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	

#pragma region General
	//------------------

public:
	bool IsConstrainted() const{return bIsConstrainted;}

	UFUNCTION()
	void GrappleObject(UPrimitiveComponent* cableTargetConstrainter, FName cableTargetBoneName);

	UFUNCTION()
	void DettachGrapple();

	UFUNCTION()
	void OnAttachWeaponEventReceived();

protected:
	
	UPROPERTY()
	bool bIsConstrainted = false;

private:
	//------------------

#pragma endregion General
	
};
