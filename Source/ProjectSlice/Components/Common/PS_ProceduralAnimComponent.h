// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PS_ProceduralAnimComponent.generated.h"


class AProjectSliceCharacter;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_ProceduralAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_ProceduralAnimComponent();

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
	
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;

	UPROPERTY(Transient)
	APlayerController* _PlayerController;


#pragma region Dip
	//------------------

public:
	UFUNCTION()
	void StartDip(const float speed = 1.0f, const float strenght = 1.0f);

	UFUNCTION()
	void LandingDip();
	
protected:
	
	UFUNCTION()
	void Dip();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Dip", meta=(ToolTip="Dip is in action"))
	bool bIsDipping = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Dip", meta=(ToolTip="Status start time in second"))
	float DipStartTimestamp = TNumericLimits<float>().Lowest();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Dip", meta=(ToolTip="Dip strenght"))
	float DipStrenght = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Dip", meta=(ToolTip="Dip speed"))
	float DipSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Dip", meta=(ToolTip="Dip duration"))
	float DipDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Dip", meta=(ToolTip="Dip Curve"))
	UCurveFloat* DipCurve;
private:
	//------------------

#pragma endregion Dip
};
