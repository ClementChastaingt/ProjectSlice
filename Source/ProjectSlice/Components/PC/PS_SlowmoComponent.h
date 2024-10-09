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
	void SlowmoTransition();

	FORCEINLINE bool IsIsSlowmoTransiting() const{return bIsSlowmoTransiting;}

	FORCEINLINE bool IsSlowmoActive() const{return bSlowmoActive;}

	FORCEINLINE float GetSlowmoAlpha() const{return SlowmoAlpha;}

	FORCEINLINE float GetGlobalTimeDilationTarget() const{return GlobalTimeDilationTarget;}

	FORCEINLINE float GetPlayerTimeDilationTarget() const{return PlayerTimeDilationTarget;}

protected:

	UFUNCTION()
	void OnStopSlowmo();

	UFUNCTION()
	FORCEINLINE float SetIsSlowmoTransiting (const bool bisSlowmoTransiting) {return bIsSlowmoTransiting = bisSlowmoTransiting;}
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slowmo")
	bool bSlowmoActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slowmo")
	bool bIsSlowmoTransiting = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slowmo")
	float StartSlowmoTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slowmo")
	float SlowmoAlpha = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slowmo")
	float SlowmoTime = 0.0f;
			
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Slowmo")
	FTimerHandle SlowmoTimerHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slowmo")
	float DefaultGlobalTimeDilation = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slowmo")
	float DefaultPlayerTimeDilation = 0.0f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slowmo")
	float StartGlobalTimeDilation = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slowmo")
	float StartPlayerTimeDilation = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo")
	float GlobalTimeDilationTarget = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo")
	float PlayerTimeDilationTarget = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo", meta=(ForceUnits="sec", ToolTip="Slowmo Max duration"))
	float SlowmoDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo|Transition", meta=(ForceUnits="sec", ToolTip="Slowmo transition duration"))
	float SlowmoTransitionDuration = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo|Transition")
	UCurveFloat* SlowmoCurve;
		
private:
	//------------------

#pragma endregion Routine

#pragma region Feedback
	//------------------

public:

	FORCEINLINE float GetSlowmoPostProcessAlpha() const{return SlowmoPostProcessAlpha;}
	
protected:
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slowmo|Feedback")
	float SlowmoPostProcessAlpha = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo|Feedback|FOV", meta=(ToolTip="Slowmo target Field Of View"))
	float TargetFOV = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo|Feedback|FOV", meta=(ForceUnits="sec", ToolTip="Slowmo FOV transition duration"))
	float SlowmoFOVResetDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo|Feedback|FOV", meta=(ForceUnits="sec", ToolTip="Slowmo FOV curves IN // OUT"))
	TArray<UCurveFloat*> SlowmoFOVCurves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slowmo|Feedback|PostProcess")
	TArray<UCurveFloat*> SlowmoPostProcessCurves;
private:
	//------------------

#pragma endregion Feedback
};
