// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PSFL_CameraShake.generated.h"

/**
 * 
 */

enum class EScreenShakeType : uint8;

USTRUCT(BlueprintType, Category = "Struct")
struct FSShakeParams
{
	GENERATED_BODY()

	FSShakeParams(){};
	
public:
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|CameraShake")
	TSubclassOf<UCameraShakeBase> CameraShake;
	
	//A World Shake is a shake at loc, so it use epicenter logic
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|CameraShake", meta=(DisplayName="Is a WorldShake"))
	bool bIsAWorldShake = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|CameraShake", meta=(EditCondition = bIsAWorldShake, EditConditionHides))
	float InnerRadius = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|CameraShake", meta=(EditCondition = bIsAWorldShake, EditConditionHides))
	float OuterRadius = 300.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|CameraShake", meta=(EditCondition = bIsAWorldShake, EditConditionHides))
	float Falloff = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|CameraShake", meta=(EditCondition = bIsAWorldShake, EditConditionHides))
	bool bOrientShakeTowardsEpicenter = false;
};

UCLASS()
class PROJECTSLICE_API UPSFL_CameraShake : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

#pragma region CameraShake
	//------------------
	
public:
	UFUNCTION(BlueprintCallable)
	static void ShakeCamera(const UObject* context, const EScreenShakeType& shakeType, const float& scale = 1.0f, const FVector& epicenter = FVector::ZeroVector);
	
	UFUNCTION(BlueprintCallable)
	static void StopCameraShake(const UObject* context, const EScreenShakeType& shakeType, const bool& bImmediately = true);

	UFUNCTION(BlueprintCallable)
	static void UpdateCameraShake(const UObject* context, const EScreenShakeType& shakeType, const float& scale = 1.0f);
	
	//------------------
#pragma endregion CameraShake
};
