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

UENUM()
enum class ETiltUsage : uint8
{
	NONE = 0	UMETA(DisplayName ="None"),
	WALL_RUN = 1 UMETA(DisplayName ="Wall_Run"),
};

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="CameraRollTilt|Debug")
	bool bDebugCameraTilt = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PostProcess|Debug")
	bool bDebugPostProcess = false;

private:
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;
	
	UPROPERTY(Transient)
	AProjectSlicePlayerController* _PlayerController;

#pragma region FOV
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

#pragma endregion FOV

#pragma region Post-Process
	//------------------

public:
	UFUNCTION()
	void OnTriggerDash(const bool bActivate);
	
protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PostProcess|Status")
	TArray<FWeightedBlendable> WeightedBlendableArray;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PostProcess|Status")
	UMaterialInstanceDynamic* SlowmoMatInst = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PostProcess|Parameters")
	UMaterialInterface* SlowmoMaterial = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PostProcess|Status")
	UMaterialInstanceDynamic* DashMatInst = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PostProcess|Status")
	FTimerHandle DashTimerHandle;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PostProcess|Parameters")
	UMaterialInterface* DashMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PostProcess|Parameters",meta=(UIMin="0", ClampMin="0", ForceUnits="s"))
	float DashDuration = 0.1f;
	
	UFUNCTION()
	void InitPostProcess();

	UFUNCTION()
	void CreatePostProcessMaterial(UMaterialInterface* const material, UMaterialInstanceDynamic*& outMatInst);
	
	UFUNCTION()
	void SlowmoTick();

	UFUNCTION()
	void OnStopSlowmoEventReceiver();

	UFUNCTION()
	void DashTick() const;

	UFUNCTION()
	void UpdateWeightedBlendPostProcess();

private:
	UPROPERTY(Transient)
	float _DashStartTimestamp;


#pragma endregion Post-Process

#pragma region Tilt
	//------------------

public:
	UFUNCTION()
	void SetupCameraTilt(const bool& bIsReset, const ETiltUsage usage, const int32& targetOrientation = 0.0f);

	UFUNCTION()
	void CameraRollTilt(float currentSeconds, const float startTime);
	
	FORCEINLINE int32 GetCurrentCameraTiltOrientation() const{return CurrentCameraTiltOrientation;}

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Camera", meta=(ToolTip="CameraTilt usage"))
	ETiltUsage CurrentUsageType;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Camera", meta=(ToolTip="Camera Tilt rest start time in second"))
	float StartCameraTiltTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Camera", meta=(ToolTip="Camera rot before Tilting"))
	FRotator DefaultCameraRot = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Camera", meta=(ToolTip="Camera rot on Start tiliting"))
	FRotator StartCameraRot = FRotator::ZeroRotator;
			
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Camera", meta=(UIMin=-1, ClampMin=-1, UIMax=1, ClampMax=1, ToolTip="Camera orientation to Wall, basiclly use for determine if Camera is rotate to left or right to Wall"))
	int32 CurrentCameraTiltOrientation = 0;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Camera", meta=(UIMin = 0.f, ClampMin = 0.f, ForceUnits="s", ToolTip="Camera rotation tilt duration"))
	float CameraTiltDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Camera", meta=(UIMin = 0.f, ClampMin = 0.f, ForceUnits="s", ToolTip="Camera rotation tilt duration"))
	float CameraOrientationTiltSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Camera", meta=(UIMin = 0.f, ClampMin = 0.f, ForceUnits="deg", ToolTip="Camera rotation tilt amplitude in degree"))
	TMap<ETiltUsage, FRotator> CameraTiltRotAmplitude = {{ETiltUsage::NONE,FRotator::ZeroRotator}};

	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category="Parameters|Camera", meta=(ToolTip="Camera rotation tilt interpolation curve"))
	UCurveFloat* CameraTiltCurve = nullptr;

private:
	UPROPERTY(Transient)
	bool _bIsCameraTilting = false;
	
	UPROPERTY(Transient)
	bool _bIsCameraTilted = false;

	UPROPERTY(Transient)
	bool _bIsResetingCameraTilt = false;

	UPROPERTY(Transient)
	float _lastAngleCamToWall;
	
#pragma endregion CameraTilt
	
};
