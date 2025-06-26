// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Field/FieldSystemActor.h"
#include "PS_FieldSystemActor.generated.h"

UCLASS()
class PROJECTSLICE_API APS_FieldSystemActor : public AFieldSystemActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	APS_FieldSystemActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	/* FieldSystemComponent */
	UFUNCTION(BlueprintNativeEvent)
	UShapeComponent* GetCollider();
	UShapeComponent* GetCollider_Implementation(){return nullptr;};

};
