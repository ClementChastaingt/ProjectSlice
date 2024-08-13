// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PS_SlowmoComponent.generated.h"


class AProjectSlicePlayerController;
class AProjectSliceCharacter;

UCLASS(Blueprintable, ClassGroup=(Component), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_SlowmoComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_SlowmoComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebug = false;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
private:
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;
	
	UPROPERTY(Transient)
	AProjectSlicePlayerController* _PlayerController;

#pragma region Routine
	//------------------

public:
	UFUNCTION()
	void OnTriggerSlowmo();
		
	UFUNCTION()
	void SlowmoTransition(const float& DeltaTime);
	
	FORCEINLINE bool IsSlowmoActive() const{return bSlowmoActive;}

protected:
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slowmo")
	bool bSlowmoActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slowmo")
	bool bIsSlowmoTransiting = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slowmo")
	float StartSlowmoTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slowmo")
	float SlowmoTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slowmo")
	float SlowmoAlpha = 0.0f;
		
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Slowmo")
	FTimerHandle SlowmoTimerHandle;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slowmo")
	float StartTimeDilation = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo")
	float GlobalTimeDilationTarget = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo")
	float PlayerTimeDilationTarget = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo", meta=(ForceUnits="sec", ToolTip="Slowmo transition duration"))
	float SlowmoTransitionDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo")
	UCurveFloat* SlowmoCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo", meta=(ForceUnits="sec", ToolTip="Slowmo Max duration"))
	float SlowmoDuration = 1.0f;
		
private:
	//------------------

#pragma endregion Routine
};
