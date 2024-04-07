// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ProjectSliceGameMode.generated.h"

class UPS_SlicedComponent;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sliceable")
	bool bEnableInitSliceableContent = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugMode = false;
	
private:
	//------------------

#pragma endregion General


#pragma region Slice
	//------------------

public:
	UFUNCTION()
	void InitSliceableContent();
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	TArray<AActor*> SliceableActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	TSubclassOf<UPS_SlicedComponent> SliceComponent;
	
private:
	//------------------

#pragma endregion Slice
};



