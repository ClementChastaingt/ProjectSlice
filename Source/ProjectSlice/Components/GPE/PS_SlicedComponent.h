// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "PS_SlicedComponent.generated.h"



UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_SlicedComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_SlicedComponent(const FObjectInitializer& objectInitializer);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

#pragma region General
//------------------

public:
	UFUNCTION()
	void InitSliceObject();

	//Getters && Setters
	UStaticMeshComponent* GetParentMesh() const{return _RootMesh;}

private:
	
	UPROPERTY(Transient)
	UStaticMeshComponent* _RootMesh = nullptr;

//------------------	
#pragma endregion General

	
	
};
