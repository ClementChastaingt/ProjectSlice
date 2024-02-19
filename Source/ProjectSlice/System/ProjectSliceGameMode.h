// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ProjectSliceGameMode.generated.h"

class UPS_SliceComponent;

UCLASS(minimalapi)
class AProjectSliceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AProjectSliceGameMode();

	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

#pragma region General
	//------------------

public:
	//------------------
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	TArray<AActor*> SliceableActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	TSubclassOf<UPS_SliceComponent> SliceComponent;
private:
	//------------------

#pragma endregion General

	
};



