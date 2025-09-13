// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProjectSlice/Data/PS_Delegates.h"
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
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;
	
	UPROPERTY(Transient)
	AProjectSlicePlayerController* _PlayerController;

#pragma region Routine
	//------------------

public:
	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Bool OnSlowmoEvent;
	
	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate OnStopSlowmoEvent;
	
	UFUNCTION()
	void OnTriggerSlowmo();

	UFUNCTION()
	void UpdateObjectDilation(AActor* actorToUpdate);

	UFUNCTION(BlueprintCallable)
	FORCEINLINE bool IsSlowmoActive() const{return _bIsSlowmoTransiting || _bSlowming;}

	FORCEINLINE bool IsSlowmoTransiting() const{return _bIsSlowmoTransiting;}
	
	FORCEINLINE bool IsSlowming() const{return _bSlowming;}

	FORCEINLINE float GetPlayerSlowmoAlpha() const{return _PlayerSlowmoAlpha;}

	FORCEINLINE float GetGlobalTimeDilationTarget() const{return GlobalTimeDilationTarget;}

	FORCEINLINE float GetInteractedObjectTimeDilationTarget() const{return InteractedObjectTimeDilationTarget;}

	FORCEINLINE float GetPlayerTimeDilationTarget() const{return PlayerTimeDilationTarget;}

protected:

	UFUNCTION()
	void OnStopSlowmo();

	UFUNCTION()
	FORCEINLINE float SetIsSlowmoTransiting (const bool bisSlowmoTransiting) {return _bIsSlowmoTransiting = bisSlowmoTransiting;}

	UFUNCTION()
	void SlowmoTransition();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo")
	float GlobalTimeDilationTarget = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo")
	float InteractedObjectTimeDilationTarget = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo")
	float PlayerTimeDilationTarget = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo", meta=(UIMin="0.01", ClampMin="0.01", ForceUnits="s", ToolTip="Slowmo Max duration"))
	float SlowmoDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo|Transition", meta=(UIMin="0.01", ClampMin="0.01", ForceUnits="s", ToolTip="Slowmo transition duration"))
	float SlowmoTransitionDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo|Transition")
	UCurveFloat* SlowmoGlobalDilationCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo|Transition")
	UCurveFloat* SlowmoPlayerDilationCurve;
		
private:
	UPROPERTY(Transient)
	bool _bSlowming;

	UPROPERTY(Transient)
	bool _bIsSlowmoTransiting;
	
	UPROPERTY(Transient)
	float _StartSlowmoTimestamp;

	UPROPERTY(Transient)
	float _PlayerSlowmoAlpha;

	UPROPERTY(Transient)
	float _SlowmoTime;
			
	UPROPERTY(Transient)
	FTimerHandle _SlowmoTimerHandle;

	UPROPERTY(Transient)
	float _DefaultGlobalTimeDilation;

	UPROPERTY(Transient)
	float _DefaultPlayerTimeDilation;
	
	UPROPERTY(Transient)
	float _StartGlobalTimeDilation;

	UPROPERTY(Transient)
	float _StartPlayerTimeDilation;

	UPROPERTY(Transient)
	TArray<AActor*> _ActorsDilated;

#pragma endregion Routine

#pragma region Feedback
	//------------------

public:
	
	UFUNCTION(BlueprintCallable)
	FORCEINLINE float GetSlowmoPostProcessAlpha() const{return _SlowmoPostProcessAlpha;}
	
protected:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo|Feedback|FOV", meta=(ToolTip="Slowmo target Field Of View"))
	float TargetFOV = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo|Feedback|FOV", meta=(ForceUnits="s", UIMin="0.01", ClampMin="0.01", ToolTip="Slowmo FOV transition duration"))
	float SlowmoFOVResetDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo|Feedback|FOV", meta=(ToolTip="Slowmo FOV curves IN // OUT"))
	TArray<UCurveFloat*> SlowmoFOVCurves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo|Feedback|PostProcess", meta=(ToolTip="Slowmo PostProcess curves IN // OUT"))
	TArray<UCurveFloat*> SlowmoPostProcessCurves;

private:
	UPROPERTY(Transient)
	float _SlowmoPostProcessAlpha;

#pragma endregion Feedback
};
