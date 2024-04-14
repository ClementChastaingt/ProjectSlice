// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PS_HookComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_HookComponent : public UPhysicsConstraintComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_HookComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	virtual void SetConstrainedComponents(UPrimitiveComponent* Component1, FName BoneName1, UPrimitiveComponent* Component2, FName BoneName2);

	virtual void BreakConstraint();

#pragma region General
	//------------------

public:
	bool IsConstrainted() const{return bIsConstrainted;}

protected:
	
	UPROPERTY()
	bool bIsConstrainted = false;

private:
	//------------------

#pragma endregion General
	
};
