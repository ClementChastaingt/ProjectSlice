// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "ProjectSlice/Data/PS_Delegates.h"
#include "PS_PlayerCameraComponent.generated.h"

class AProjectSlicePlayerController;
class AProjectSliceCharacter;
/**
 * 
 */

USTRUCT(BlueprintType, Category = "Struct")
struct FSVignetteAnimation
{
	GENERATED_BODY()

	FSVignetteAnimation(){};
	
public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters", meta=(Tooltip="X is PITCH && Y is YAW"))
	FVector2D RateMinMax = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	float AnimSpeed = 10.0f;
	
	UPROPERTY(meta=(Tooltip="X is PITCH && Y is YAW"))
	FVector2D Rate = FVector2D::ZeroVector;
	
	UPROPERTY(meta=(Tooltip="X is PITCH && Y is YAW"))
	FVector2D VignetteOffset = FVector2D::ZeroVector;
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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Debug")
	bool bDebug = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|CameraRollTilt|Debug")
	bool bDebugCameraTilt = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|CameraRollTilt|Debug")
	bool bDebugCameraTiltTick = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Debug")
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
	FLinearColor DefaultDirtMaskTint;

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
	void TriggerDash(const bool bActivate);

	UFUNCTION()
	void TriggerGlasses(const bool bActivate, const bool bRenderCustomDepth);

	//Getters && Setters
	UFUNCTION()
	FORCEINLINE FPostProcessSettings GetDefaultPostProcessSettings() const{return _DefaultPostProcessSettings;}

	//Delegates
	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Bool OnTriggerGlasses;
	
protected:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|PostProcess|Material")
	UMaterialInterface* SlowmoMaterial = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|PostProcess|Material")
	UMaterialInterface* DashMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|PostProcess|Material")
	UMaterialInterface* GlassesMaterial = nullptr;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|PostProcess|Material")
	UMaterialInterface* FishEyeMaterial = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|PostProcess|Material")
	UMaterialInterface* VignetteMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|PostProcess|Dash",meta=(UIMin="0", ClampMin="0", ForceUnits="s"))
	float DashDuration = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameters|PostProcess|Glasses")
	FPostProcessSettings GlassesPostProcessSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameters|PostProcess|Glasses|Vignette")
	FSVignetteAnimation VignetteAnimParams;
		
	UFUNCTION()
	void InitPostProcess();

	UFUNCTION()
	void CreatePostProcessMaterial(UMaterialInterface* const material, UMaterialInstanceDynamic*& outMatInst);

	
	UFUNCTION()
	void UpdateWeightedBlendPostProcess();
	
	UFUNCTION()
	void SlowmoTick();

	UFUNCTION()
	void OnStopSlowmoEventReceiver();

	UFUNCTION()
	void DashTick() const;

	UFUNCTION()
	void GlassesTick(const float deltaTime);

private:
	UPROPERTY(Transient)
	FPostProcessSettings _DefaultPostProcessSettings;

	UPROPERTY(Transient)
	TArray<FWeightedBlendable> _WeightedBlendableArray;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* _SlowmoMatInst;
	
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* _DashMatInst;
	
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* _GlassesMatInst;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* _FishEyeMatInst;
	
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* _VignetteMatInst;
	
	UPROPERTY(Transient)
	float _DashStartTimestamp;

	UPROPERTY(Transient)
	FTimerHandle _DashTimerHandle;
	
	UPROPERTY(Transient)
	bool _bIsUsingGlasses;
	
	UPROPERTY(Transient)
	bool _bGlassesRenderCustomDepth;

	UPROPERTY(Transient)
	FRotator _CamRot;

#pragma endregion Post-Process

#pragma region Tilt
	//------------------

public:
	UFUNCTION()
	void SetupCameraTilt(const bool& bIsReset, const int32& targetOrientation = 0.0f);

	UFUNCTION()
	void ForceUpdateTargetTilt();

	UFUNCTION()
	void CameraRollTilt();
	
	FORCEINLINE float GetCurrentCameraTiltOrientation() const{return CurrentCameraTiltOrientation;}

protected:
		
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Camera", meta=(ToolTip="Camera Tilt rest start time in second"))
	float StartCameraTiltTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Camera", meta=(ToolTip="Camera rot before Tilting"))
	FRotator DefaultCameraRot = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Camera", meta=(ToolTip="Camera rot on Start tiliting"))
	FRotator StartCameraRot = FRotator::ZeroRotator;
			
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Camera", meta=(UIMin=-1, ClampMin=-1, UIMax=1, ClampMax=1, ToolTip="Camera orientation to Wall, basiclly use for determine if Camera is rotate to left or right to Wall"))
	float CurrentCameraTiltOrientation = 0.0f;

	//Init an impossible value for a dot product
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Camera")
	float LastAngleCamToWall = 0.0f;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Camera", meta=(UIMin = 0.f, ClampMin = 0.f, ForceUnits="s", ToolTip="Camera rotation tilt duration"))
	float CameraTiltDuration = 0.2f;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category="Parameters|Camera", meta=(ToolTip="Camera rotation tilt interpolation curve"))
	UCurveFloat* CameraTiltCurve = nullptr;

private:
	UPROPERTY(Transient)
	bool _bIsCameraTiltingByInterp = false;
	
	UPROPERTY(Transient)
	bool _bIsCameraTilted = false;

	UPROPERTY(Transient)
	bool _bIsResetingCameraTilt = false;
	
#pragma endregion CameraTilt
	
};
