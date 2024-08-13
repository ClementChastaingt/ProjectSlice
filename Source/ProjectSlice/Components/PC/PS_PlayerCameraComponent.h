// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "PS_PlayerCameraComponent.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, ClassGroup=(Player), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_PlayerCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_PlayerCameraComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void CreatePostProcessMaterial(const UMaterialInterface* Material, UMaterialInstanceDynamic& outMatInst,
		FWeightedBlendable& outWeightedBlendable) const;

#pragma region Post-Process
	//------------------

public:
	//------------------
protected:
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PostProcess|Status")
	UMaterialInstanceDynamic* SlowmoMatInst = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="PostProcess|Parameters")
	UMaterialInterface* SlowmoMaterial = nullptr;
	
	UFUNCTION()
	void InitPostProcess();
	
private:
	//------------------

#pragma endregion Post-Process
};
