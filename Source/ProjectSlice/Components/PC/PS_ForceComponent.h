// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Character/PC/PS_PlayerController.h"
#include "ProjectSlice/Data/PS_Delegates.h"
#include "PS_ForceComponent.generated.h"


class AProjectSliceCharacter;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_ForceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_ForceComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	void UpdatePushTargetLoc();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDebugPush = false;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;
	
	UPROPERTY(Transient)
	AProjectSlicePlayerController* _PlayerController;

#pragma region Push
	//------------------

public:
	UFUNCTION()
	void UnloadPush();
	
	UFUNCTION()
	void ReleasePush();
	
	UFUNCTION()
	void SetupPush();

	UFUNCTION()
	void StopPush();

	FORCEINLINE bool IsPushing() const{return _bIsPushing;}

	FORCEINLINE bool IsPushLoading() const{return _bIsPushLoading;}

	FORCEINLINE bool IsPushReleased() const{return _bIsPushReleased;}
	
	FORCEINLINE float GetMaxPushForceTime() const{return MaxPushForceTime;}
	
	FORCEINLINE float GetInputTimeWeigtAlpha() const { return UKismetMathLibrary::MapRangeClamped(GetWorld()->GetAudioTimeSeconds(), _StartForcePushTimestamp,_StartForcePushTimestamp + MaxPushForceTime,0.0f, 1.0f);};

	UFUNCTION(BlueprintCallable)
	FORCEINLINE FRotator GetPushForceRotation() const{return _PushForceRotation;}

	FORCEINLINE float GetReleasePushTimestamp() const{ return _ReleasePushTimestamp; }

	FORCEINLINE void SetReleasePushTimestamp(const float ReleasePushTimestamp){_ReleasePushTimestamp = ReleasePushTimestamp;}

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Bool OnPushEvent;
	
	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate OnPushReleaseNotifyEvent;

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Bool OnSpawnPushDistorsion;

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Vector_Vector OnSpawnPushBurst;
	
	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Vector_Rotator OnPushTargetUpdate;
	
protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Status|Push")
	bool bIsQuickPush;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push", meta=(UIMin="0", ClampMin="0"))
	float PushForce = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push", meta=(UIMin="0", ClampMin="0", ForceUnits="sec"))
	float QuickPushTimeThreshold = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push", meta=(UIMin="0", ClampMin="0", ForceUnits="sec"))
	float MaxPushForceTime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push|Sweep", meta=(UIMin="0", ClampMin="0", ForceUnits="cm"))
	float SphereRadius = 50.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push|Sweep", meta=(UIMin="0", ClampMin="0", ForceUnits="deg"))
	float ConeAngleDegrees = 35.0f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push|Sweep", meta=(UIMin="0", ClampMin="0", ForceUnits="cm"))
	float ConeLength = 500.0f ;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push|Sweep", meta=(UIMin="0", ClampMin="0", ForceUnits="sec"))
	float StepInterval = 100.0f; 
			
	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Push|Feedback")
	USoundBase* PushSound;

	UFUNCTION()
	void Impulse(UMeshComponent* inComp, const FVector impulse);

private:
	UPROPERTY(Transient)
	bool _bIsPushing;
	
	UPROPERTY(Transient)
	bool _bIsPushLoading;

	UPROPERTY(Transient)
	bool _bIsPushReleased;
	
	UPROPERTY(VisibleInstanceOnly, Category="Status")
	FHitResult _CurrentPushHitResult;

	UPROPERTY(VisibleInstanceOnly, Category="Status")
	float _PushForceWeight;
	
	UPROPERTY(VisibleInstanceOnly, Category="Status")
	FRotator _PushForceRotation;
	
	UPROPERTY(Transient)
	float _StartForcePushTimestamp;
	
	UPROPERTY(Transient)
	float _ReleasePushTimestamp;

#pragma endregion Push

#pragma region Screw
	//------------------
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Screw")
	TSoftObjectPtr<UStaticMesh> ScrewMesh;

	UFUNCTION()
	void OnScrewResetEndEventReceived();
	
	UFUNCTION()
	void AttachScrew();

	//------------------
#pragma endregion Screw
	
#pragma region Cooldown
	//------------------
	
public:
	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate OnCoolDownEnded;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Cooldown", meta=(UIMin="0", ClampMin="0", ForceUnits="sec"))
	float CoolDownDuration = 0.2f;

private:
	UPROPERTY(Transient)
	FTimerHandle _CoolDownTimerHandle;
	
	//------------------
#pragma endregion Cooldown
};
