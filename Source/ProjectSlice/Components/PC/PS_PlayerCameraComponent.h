// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "ProjectSlice/Data/PS_Delegates.h"
#include "ProjectSlice/Data/PS_GlobalType.h"
#include "ProjectSlice/FunctionLibrary/PSFL_CameraShake.h"
#include "PS_PlayerCameraComponent.generated.h"

class AProjectSlicePlayerController;
class AProjectSliceCharacter;
/**
 * 
 */

UENUM()
enum class ETiltType : uint8
{
	WALLRUN = 0 UMETA(DisplayName ="WallRun"),
	SLIDE = 1 UMETA(DisplayName ="Slide"),
};

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


USTRUCT(BlueprintType, Category = "Struct")
struct FSCameraTiltParams
{
	GENERATED_BODY()

	FSCameraTiltParams(){};
	
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|CameraRollTilt", meta=(UIMin = 0.f, ClampMin = 0.f, ForceUnits="s", ToolTip="Camera rotation tilt duration"))
	bool bUseDynTiltUpdating = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|CameraRollTilt", meta=(UIMin = 0.f, ClampMin = 0.f, ForceUnits="s", ToolTip="Camera rotation tilt duration"))
	float CameraTiltDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|CameraRollTilt", meta=(UIMin = 0.f, ClampMin = 0.f, ForceUnits="deg", ToolTip="Camera rotation Roll tilt target"))
	float CameraTilRollTarget = 20;
};


UCLASS(Blueprintable, ClassGroup=(Player), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_PlayerCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_PlayerCameraComponent();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	void StartCustomTick();
	
	void CustomTick();

protected:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Debug")
	bool bDebug = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|CameraRollTilt|Debug")
	bool bDebugCameraTilt = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|CameraRollTilt|Debug")
	bool bDebugCameraTiltTick = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Debug")
	bool bDebugPostProcess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|CameraShake|Debug")
	bool bDebugCameraShake = false;

private:
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;
	
	UPROPERTY(Transient)
	AProjectSlicePlayerController* _PlayerController;
	
	UPROPERTY(Transient)
	float _LastTickTime;

	UPROPERTY(Transient)
	FTimerHandle _CustomTickTimerHandler;

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

#pragma region CameraShake
	//------------------

public:

	// Func for do a basic shake, epicenter is used if worldshake is ON. If you need to do a WorldShake with custom FSWorldShakeParams params use WorldShakeCamera().
	UFUNCTION(BlueprintCallable)
	void ShakeCamera(const EScreenShakeType& shakeType,const float& scale = 1.0f, const FVector& epicenter = FVector::ZeroVector);

	//Use for stop looping shakes
	UFUNCTION(BlueprintCallable)
	void StopCameraShake(const EScreenShakeType& shakeType, const bool& bImmediately = false);

	//Use for update currently playing camera shake scale
	UFUNCTION(BlueprintCallable)
	void UpdateCameraShakeScale(const EScreenShakeType& shakeType, const float& scale = 1.0f);

	
	// Func for do a Dyn World Shake, use input FSWorldShakeParams instead of ShakesParams input
	UFUNCTION(BlueprintCallable)
	void WorldShakeCamera(const EScreenShakeType& shakeType, const FVector& epicenter, const FSWorldShakeParams& worldShakeParams);

	UFUNCTION(BlueprintCallable)
	void SetWorldShakeOverrided(const EScreenShakeType& shakeType);
	
	UFUNCTION(BlueprintCallable)
	bool IsCameraShakeExist(const EScreenShakeType& shakeType){ return _CameraShakesInst.Contains(shakeType) && IsValid(*_CameraShakesInst.Find(shakeType));};
	
protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|CameraShake")
	TMap<EScreenShakeType, FSShakeParams> ShakesParams;
	
private:
	UPROPERTY(Transient)
	TMap<EScreenShakeType, UCameraShakeBase*> _CameraShakesInst;
	
	//------------------
#pragma endregion CameraShake

#pragma region Post-Process
	//------------------

public:
	UFUNCTION()
	void TriggerDash(const bool bActivate);
	
	//Getters && Setters
	UFUNCTION()
	FORCEINLINE FPostProcessSettings GetDefaultPostProcessSettings() const{return _DefaultPostProcessSettings;}

protected:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|PostProcess|Material")
	UMaterialInterface* GlassesMaterial = nullptr;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|PostProcess|Material")
	UMaterialInterface* FishEyeMaterial = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|PostProcess|Material")
	UMaterialInterface* VignetteMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|PostProcess|Material")
	UMaterialInterface* OutlineMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|PostProcess|Material")
	UMaterialInterface* SlowmoMaterial = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|PostProcess|Material")
	UMaterialInterface* DashMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|PostProcess|Material")
	UMaterialInterface* DepthMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|PostProcess|Dash",meta=(UIMin="0", ClampMin="0", ForceUnits="s"))
	float DashDuration = 0.1f;
	
	UFUNCTION()
	void InitPostProcess();

	UFUNCTION()
	void CreatePostProcessMaterial(UMaterialInterface* const material, UMaterialInstanceDynamic*& outMatInst);
	
	UFUNCTION()
	void UpdateWeightedBlendPostProcess();
	
	UFUNCTION()
	void SlowmoTick();

	UFUNCTION()
	void OnStartSlowmoEventReceiver();

	UFUNCTION()
	void OnStopSlowmoEventReceiver();

	UFUNCTION()
	void DashTick() const;


private:
	UPROPERTY(Transient)
	FPostProcessSettings _DefaultPostProcessSettings;

	UPROPERTY(Transient)
	TArray<FWeightedBlendable> _WeightedBlendableArray;
		
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* _GlassesMatInst;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* _FishEyeMatInst;
	
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* _VignetteMatInst;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* _OutlineMatInst;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* _SlowmoMatInst;
	
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* _DashMatInst;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* _DepthMatInst;
	
	UPROPERTY(Transient)
	float _DashStartTimestamp;

#pragma region Glasses
	//------------------

public:
	UFUNCTION()
	void TriggerGlasses(const bool bActivate, const bool bBlendPostProcess);

	UFUNCTION()
	void DisplayOutlineOnSightedComp(const bool bRenderCustomDepth);
	
	//Delegates
	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Bool OnTriggerGlasses;

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Float OnTransitToGlasses;

	
protected:
	UFUNCTION()
	void GlassesTick(const float deltaTime);

	UFUNCTION()
	void OnStopGlasses();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameters|PostProcess|Glasses", meta=(ClampMin=0.0f, UIMin=0.0f, ForceUnits="sec"))
	float GlassesTransitionSpeed = 0.25;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameters|PostProcess|Glasses")
	FPostProcessSettings GlassesPostProcessSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameters|PostProcess|Glasses")
	UMaterialParameterCollection* GlassesMatCollec;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameters|PostProcess|Glasses|Vignette")
	FSVignetteAnimation VignetteAnimParams;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameters|PostProcess|Glasses|FilmGrain")
	UMaterialParameterCollection* FilmGrainMatCollec;
private:
	UPROPERTY(Transient)
	bool _bIsUsingGlasses = false;

	UPROPERTY(Transient)
	bool _bGlassesIsActive = false;
	
	UPROPERTY(Transient)
	bool _bGlassesRenderCustomDepth;

	UPROPERTY(Transient)
	FRotator _CamRot;

	UPROPERTY(Transient)
	float _BlendWeight;


#pragma endregion Glasses


#pragma endregion Post-Process

#pragma region Tilt
	//------------------

public:
	UFUNCTION()
	bool StartCameraTilt(const ETiltType& tiltType, const int32& targetOrientation = 0);

	UFUNCTION()
	void StopCameraTilt(const ETiltType& tiltType);

	UFUNCTION()
	void ForceUpdateTargetTilt();

	UFUNCTION()
	void CameraRollTilt(const float& deltaTime);
	
	UFUNCTION()
	void OnCameraTiltMovementEnded();

	UFUNCTION()
	void UpdateRollTiltTarget(float alpha, const int32& orientation, float startRoll = 0.0f);

	UFUNCTION()
	float GetAngleCamToTarget();
	
	FORCEINLINE float GetCurrentCameraTiltOrientation() const{return _TargetTiltOrientation;}

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|CameraRollTilt", meta=(UIMin = 0.f, ClampMin = 0.f, ForceUnits="deg", ToolTip="Camera rotation Roll tilt target"))
	TMap<ETiltType, FSCameraTiltParams> CameraTiltRollParams;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category="Parameters|CameraRollTilt", meta=(ToolTip="Camera rotation tilt interpolation curve"))
	UCurveFloat* CameraTiltCurve = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|CameraRollTilt", meta=(UIMin = 1.f, ClampMin = 1.f, ToolTip="Camera rotation Roll tilt interp smoothing speed"))
	float CameraTiltSmoothingSpeed = 10.0f;

private:
	UPROPERTY(Transient)
	ETiltType _CurrentTiltType;

	UPROPERTY(Transient)
	FSCameraTiltParams _CurrentCameraTiltRollParams;

	UPROPERTY()
	bool _bIsTilting = false;

	UPROPERTY()
	bool _bIsResetingCameraTilt = false;

	UPROPERTY(Transient)
	float _AlphaTiltWeight;
	
	UPROPERTY(Transient)
	float _StartCameraTiltTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(Transient)
	FRotator _DefaultCameraRot = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	FRotator _StartCameraRot = FRotator::ZeroRotator;
	
	UPROPERTY(Transient)
	FRotator _CurrentCameraRot = FRotator::ZeroRotator;
			
	UPROPERTY()
	float _TargetTiltOrientation = 0.0f;

	//Init an impossible value for a dot product
	UPROPERTY()
	float _LastAngleCamToTarget = 0.0f;

#pragma endregion CameraTilt
	
};
