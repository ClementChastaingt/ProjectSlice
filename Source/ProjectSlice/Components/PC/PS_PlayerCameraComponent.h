// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "PS_PlayerCameraComponent.generated.h"

class AProjectSlicePlayerController;
class AProjectSliceCharacter;
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

protected:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PostProcess|Debug")
	bool bDebug = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PostProcess|Debug")
	bool bDebugPostProcess = false;

private:
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;
	
	UPROPERTY(Transient)
	AProjectSlicePlayerController* _PlayerController;

#pragma region General
	//------------------

public:
	UFUNCTION()
	void SetupFOVInterp(const float targetFOV, const float duration, UCurveFloat* interCurve = nullptr);

	FORCEINLINE float GetDefaultFOV() const{return DefaultFOV;}

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|General|FOV")
	bool bFieldOfViewInterpChange = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|General|FOV")
	float DefaultFOV;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|General|FOV")
	float StartFOV = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|General|FOV",  meta = (UIMin = "5.0", UIMax = "170", ClampMin = "0.001", ClampMax = "360.0", Units = deg))
	float TargetFOV = 60.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|General|FOV")
	float StartFOVInterpTimestamp = TNumericLimits<float>::Min();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|General|FOV")
	float FOVIntertpDuration = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|General|FOV")
	UCurveFloat* FOVIntertpCurve = nullptr;
	
	
	
	UFUNCTION()
	void FieldOfViewTick();
private:
	//------------------

#pragma endregion General


#pragma region Post-Process
	//------------------

public:
	//------------------
protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PostProcess|Status")
	TArray<FWeightedBlendable> WeightedBlendableArray;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PostProcess|Status")
	UMaterialInstanceDynamic* SlowmoMatInst = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PostProcess|Parameters")
	UMaterialInterface* SlowmoMaterial = nullptr;
	
	UFUNCTION()
	void InitPostProcess();

	UFUNCTION()
	void CreatePostProcessMaterial(const UMaterialInterface* material, UMaterialInstanceDynamic*& outMatInst);

	UFUNCTION()
	void SlowmoTick();
	
private:
	//------------------

#pragma endregion Post-Process
};
