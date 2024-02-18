// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "PS_SliceComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_SliceComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_SliceComponent();

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
	//------------------
protected:
	// UPROPERTY(VisibleAnywhere)
	// UProceduralMesh;
	
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* RootMesh;
	
private:
	//------------------
	
#pragma endregion General

	
	
};
