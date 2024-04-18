// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PS_HookComponent.generated.h"


UCLASS(Blueprintable, BlueprintType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_HookComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_HookComponent();

	UPROPERTY(VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* HookThrower = nullptr;
	
	UPROPERTY(VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* CableMesh = nullptr;

	UPROPERTY(VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	UPhysicsConstraintComponent* CableOriginAttach = nullptr;

	UPROPERTY(VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	UPhysicsConstraintComponent* CableTargetAttach = nullptr;

	UPROPERTY(VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* HookMesh = nullptr;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bDebug = false;

private:
	

#pragma region General
	//------------------

public:
	bool IsConstrainted() const{return bIsConstrainted;}

	UFUNCTION()
	void GrappleObject(UPrimitiveComponent* cableTargetConstrainter, FName cableTargetBoneName);

	UFUNCTION()
	void DettachGrapple();
	
protected:
	
	UPROPERTY(VisibleInstanceOnly, Category="Status")
	FHitResult CurrentHookHitResult;

protected:
	
	UPROPERTY()
	bool bIsConstrainted = false;

private:
	//------------------

#pragma endregion General
	
};