// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/TimelineComponent.h"
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
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Dip", meta=(ToolTip="Start time in second"))
	float DipStartTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Dip", meta=(ToolTip="Current Dip alpha"))
	float DipAlpha = 0.0f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Dip", meta=(ToolTip="Current Dip strenght"))
	float DipStrenght = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Dip", meta=(ToolTip="Current Dip speed"))
	float DipSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Dip", meta=(ToolTip="Dip duration"))
	float DipDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Dip", meta=(ToolTip="Dip proc animation Curve"))
	UCurveFloat* DipCurve;
private:
	//------------------

#pragma endregion Dip

#pragma region Walking
	//------------------

public:
	
	UFUNCTION()
	void Walking(const float leftRightAlpha, const float upDownAlpha, const float rollAlpha);
	
protected:

	UFUNCTION()
	void SetVelocityLagPosition();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Walking", meta=(ToolTip="Walking anim position"))
	FVector WalkAnimPos = FVector::Zero();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Walking", meta=(ToolTip="Walking anim rotation"))
	FRotator WalkAnimRot = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Walking", meta=(ToolTip="Walking anim alpha"))
	float WalkAnimAlpha = 0.0f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Walking", meta=(ToolTip="Location lag position"))
	FVector LocationLagPosition = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Walking", meta=(UIMax= "0", ClampMax="0", ToolTip="Current Walking speed"))
	float WalkingSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Walking", meta=(UIMin = "0", ClampMin="0", ToolTip="Walking Left/Right Offest"))
	float WalkingLeftRightOffest = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Walking", meta=(UIMin = "0", ClampMin="0", ToolTip="Walking Up Offest"))
	float WalkingUpOffest = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Walking", meta=(UIMax= "0", ClampMax="0", ToolTip="Walking Down Offest"))
	float WalkingDownOffest = -0.35f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Walking", meta=(UIMax= "0", ClampMax="0", ToolTip="Walking Down Offest"))
	float WalkingMaxSpeed = 1.65f;
private:
	//------------------

#pragma endregion Walking
};
