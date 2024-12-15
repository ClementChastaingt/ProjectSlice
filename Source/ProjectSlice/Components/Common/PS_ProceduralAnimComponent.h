// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/TimelineComponent.h"
#include "ProjectSlice/Data/PS_Delegates.h"
#include "PS_ProceduralAnimComponent.generated.h"


class AProjectSlicePlayerController;
class AProjectSliceCharacter;

UCLASS(ClassGroup=(Animation), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_ProceduralAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_ProceduralAnimComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebug = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebugDip = false;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;

	UPROPERTY(Transient)
	AProjectSlicePlayerController* _PlayerController;

#pragma region Dip
	//------------------

public:
	UFUNCTION()
	void StartDip(const float speed = 1.0f, const float strenght = 1.0f);

	UFUNCTION()
	void LandingDip();

	UFUNCTION()
	void DashDip();

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
	UFUNCTION(BlueprintCallable)
	void Walking(const float& leftRightAlpha, const float& upDownAlpha, const float& rollAlpha);
	
	UFUNCTION()
	void StartWalkingAnim();

	UFUNCTION(BlueprintCallable)
	void StartWalkingAnimWithDelay(const float delay);

	UFUNCTION()
	void StopWalkingAnim();
	
	//Delegates
	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate OnStartWalkFeedbackEvent;

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate OnStopWalkFeedbackEvent;

protected:

	UFUNCTION(BlueprintCallable)
	void SetLagPositionAndAirTilt();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Walking", meta=(ToolTip="Walking anim position"))
	FVector WalkAnimPos = FVector::Zero();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Walking", meta=(ToolTip="Walking anim rotation"))
	FRotator WalkAnimRot = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Walking", meta=(ToolTip="Walking anim alpha"))
	float WalkAnimAlpha = 0.0f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Walking", meta=(ToolTip="Location lag position"))
	FVector LocationLagPosition = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Walking", meta=(UIMax= "0", ClampMax="0", ToolTip="Current Walking Timeline play rate"))
	float WalkingSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Walking", meta=(UIMin = "0", ClampMin="0", ToolTip="Walking Left/Right Offest"))
	float WalkingLeftRightOffest = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Walking", meta=(UIMin = "0", ClampMin="0", ToolTip="Walking Up Offest"))
	float WalkingUpOffest = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Walking", meta=(UIMax= "0", ClampMax="0", ToolTip="Walking Down Offest"))
	float WalkingDownOffest = -0.35f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Walking", meta=(UIMax= "0", ClampMax="0", ToolTip="Walking Down Offest"))
	float WalkingMaxSpeed = 1.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Walking|Lag", meta=(UIMin = "0", ClampMin="0", ToolTip="Frame rate smoothing speed for Velocity Lag postion interpolation"))
	float VelocityLagSmoothingSpeed = 6.0f;

	
	
private:
	//------------------

#pragma endregion Walking

#pragma region Aerial
	//------------------

public:
	//------------------
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Aerial", meta=(UIMax= "0", ClampMax="0", ToolTip="Aerial Tilt lag rotator"))
	FRotator InAirTilt = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Aerial", meta=(UIMax= "0", ClampMax="0", ToolTip="Aerial Tilt lag offset"))
	FVector InAirOffset = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Aerial|Lag", meta=(UIMin = "0", ClampMin="0", ToolTip="Frame rate smoothing speed for Air Tilt Lag postion interpolation"))
	float AirTiltLagSmoothingSpeed = 12.0f;
private:
	//------------------

#pragma endregion Aerial

#pragma region Sway
	//------------------

public:

	UFUNCTION(BlueprintCallable)
	FORCEINLINE FRotator GetCurrentCamRot() const{return CurrentCamRot;}

protected:
	UFUNCTION(BlueprintCallable)
	void ApplyLookSwayAndOffset(const FRotator& camRotPrev);


protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Sway", meta=(ToolTip="Pitch Offset position"))
	FVector PitchOffsetPos = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Sway", meta=(ToolTip="Camera rotation Offset position"))
	FVector CamRotOffsetGun = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Sway", meta=(ToolTip="Camera rotation Offset position"))
	FVector CamRotOffsetHook= FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Sway", meta=(ToolTip="Current Camera rotation"))
	FRotator CurrentCamRot = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Sway", meta=(ToolTip="Camera Gun rotation rate"))
	FRotator CamRotRateGun = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Sway", meta=(ToolTip="Camera Hook rotation rate"))
	FRotator CamRotRateHook = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Look", meta=(UIMin="0", ClampMin="0", ToolTip="Max pitch Offset modifier for look input "))
	FVector MaxPitchOffset = FVector(35.0f ,3.0f, 2.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Look", meta=(UIMin = "0", ClampMin="0", ToolTip="Frame rate smoothing speed for Gun Sway rotation lag interpolation"))
	float SwayLagSmoothingSpeedGun = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Look", meta=(UIMin = "0", ClampMin="0", ToolTip="Frame rate smoothing speed for Hook Sway rotation lag interpolation"))
	float SwayLagSmoothingSpeedHook = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Look", meta=(UIMin="0", ClampMin="0", ToolTip="Max sway Offset modifier for look input "))
	FVector MaxCamRotOffset = FVector(6.0f ,0.0f, 10.0f);
private:
	//------------------

#pragma endregion Sway

#pragma region Hook
	//------------------

public:
	UFUNCTION()
	void ApplyWindingVibration(const float alpha);
	
protected:
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Hook", meta=(ToolTip="Winde Hook location"))
	FVector HookLocOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Hook", meta=(ToolTip="Winde Hook max offset"))
	float HookLocMaxOffset = 3.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Hook", meta=(ToolTip="Winde Hook offset progression curve"))
	UCurveFloat* HookLocOffsetCurve;
	
private:
	//------------------

#pragma endregion Hook
};
