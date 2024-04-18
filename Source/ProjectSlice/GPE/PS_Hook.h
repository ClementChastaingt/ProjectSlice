// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PS_GPEBase.h"
#include "ProjectSlice/Components/PC/PS_HookComponent.h"
#include "PS_Hook.generated.h"

UCLASS()
class PROJECTSLICE_API APS_Hook : public APS_GPEBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	APS_Hook();

	UPROPERTY(meta = (AllowPrivateAccess = "true"))
	UPS_HookComponent* HookComponent = nullptr;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
