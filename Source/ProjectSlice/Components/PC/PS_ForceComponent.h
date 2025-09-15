// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Data/PS_Delegates.h"
#include "ProjectSlice/Interface/PS_CanGenerateImpactField.h"
#include "PS_ForceComponent.generated.h"


class AProjectSlicePlayerController;
class APS_FieldSystemActor;
class AProjectSliceCharacter;

UENUM(BlueprintType)
enum class EPushSFXType : uint8
{
	RELEASED = 0 UMETA(DisplayName = "Released"),
	WHISPER  = 1 UMETA(DisplayName = "Whisper"),
	IMPACT = 2 UMETA(DisplayName = "Impact"),
};

UCLASS(Blueprintable, BlueprintType, ClassGroup=(Component), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_ForceComponent : public UActorComponent, public IPS_CanGenerateImpactField
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_ForceComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
	bool bDebugPush = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebugChaos = false;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:


private:
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;
	
	UPROPERTY(Transient)
	AProjectSlicePlayerController* _PlayerController;

#pragma region General
	//------------------

protected:
	UFUNCTION()	
	void UpdatePushTargetLoc();
	
	UFUNCTION()
	void OnFireEventReceived();

	UFUNCTION()
	void OnFireTriggeredEventReceived();

	UFUNCTION()
	void OnToggleTurnRackTargetEventReceived(const bool bActive);
	
	UFUNCTION()
	void OnTurnRackEventReceived();

	//------------------
#pragma endregion General

#pragma region Push
	//------------------

public:
	UFUNCTION()
	void UnloadPush();
	
	UFUNCTION()
	void ReleasePush();

	UFUNCTION()
	void OnPushReleasedEventReceived();

	UFUNCTION()
	void DeterminePushType();
		
	UFUNCTION()
	static void SortPushTargets(const TArray<FHitResult>& hitsToSort, TArray<FHitResult>& outFilteredHitResult);
	
	UFUNCTION()
	void SetupPush();

	UFUNCTION()
	void StopPush();

	UFUNCTION()
	void PlaySound(EPushSFXType soundType);

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Audio OnPushPlaySoundEvent;

	UFUNCTION(BlueprintCallable)
	FORCEINLINE bool IsPushing() const{return _bIsPushing;}

	UFUNCTION(BlueprintCallable)
	FORCEINLINE bool IsPushLoading() const{return _bIsPushLoading;}

	UFUNCTION(BlueprintCallable)
	FORCEINLINE bool IsPushReleased() const{return _bIsPushReleased;}
	
	FORCEINLINE float GetMaxPushForceTime() const{return InputMaxPushForceDuration;}
	
	FORCEINLINE float GetInputTimeWeightAlpha() const { return UKismetMathLibrary::MapRangeClamped(GetWorld()->GetAudioTimeSeconds(), _StartForcePushTimestamp,_StartForcePushTimestamp + InputMaxPushForceDuration,0.0f, 1.0f);};

	UFUNCTION(BlueprintCallable)
	FORCEINLINE float GetStartForceHandOffset() const{return _StartForceHandOffset;}

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push", meta=(UIMin="0", ClampMin="0", ForceUnits="cm"))
	float QuickPushStartLocOffset = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push", meta=(UIMin="0", ClampMin="0", ForceUnits="s"))
	float QuickPushTimeThreshold = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push", meta=(UIMin="0", ClampMin="0", ForceUnits="s"))
	float InputMaxPushForceDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push", meta=(UIMin="0", ClampMin="0", ForceUnits="s"))
	float MaxPushForceTime = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push|Sweep", meta=(UIMin="0", ClampMin="0", ForceUnits="cm"))
	float SphereRadius = 50.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push|Sweep", meta=(UIMin="0", ClampMin="0", ForceUnits="deg"))
	float ConeAngleDegrees = 35.0f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push|Sweep", meta=(UIMin="0", ClampMin="0", ForceUnits="cm"))
	float ConeLength = 500.0f ;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push|Sweep", meta=(UIMin="0", ClampMin="0", ForceUnits="sec"))
	float StepInterval = 20.0f; 
			
	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Push|Feedback")
	TMap<EPushSFXType,USoundBase*> PushSounds;

	UFUNCTION()
	void Impulse(UMeshComponent* inComp, FVector impulse, const FHitResult impactPoint);

private:
	UPROPERTY(Transient)
	bool _bIsPushing;
	
	UPROPERTY(Transient)
	bool _bIsPushLoading;

	UPROPERTY(Transient)
	bool _bIsPushReleased;
	
	UPROPERTY(VisibleInstanceOnly, Category="Status")
	FHitResult _CurrentPushHitResult;

	UPROPERTY(Transient)
	float _AlphaInput;

	UPROPERTY(VisibleInstanceOnly, Category="Status")
	float _PushForceWeight;
	
	UPROPERTY(VisibleInstanceOnly, Category="Status")
	FRotator _PushForceRotation;
	
	UPROPERTY(Transient)
	float _StartForcePushTimestamp;
	
	UPROPERTY(Transient)
	float _ReleasePushTimestamp;

	UPROPERTY(Transient)
	FVector _StartForcePushLoc;

	UPROPERTY(Transient)
	FVector _ImpactForcePushLoc;
	
	UPROPERTY(Transient)
	float _StartForceHandOffset;
	
	UPROPERTY(Transient)
	FVector _DirForcePush;
	
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Cooldown", meta=(UIMin="0", ClampMin="0", ForceUnits="s"))
	float CoolDownDuration = 0.2f;

private:
	UPROPERTY(Transient)
	FTimerHandle _CoolDownTimerHandle;
	
	//------------------
#pragma endregion Cooldown

#pragma region Destruction
	//------------------

public:
	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Field OnForceChaosFieldGeneratedEvent;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Destruction")
	TSubclassOf<APS_FieldSystemActor> FieldSystemActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Destruction", meta=(ForceUnits="cm", UIMin="0", ClampMin="0"))
	float FieldSystemMoveTargetLocFwdOffset = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Destruction", meta=(ForceUnits="s", UIMin="0", ClampMin="0"))
	float FieldSystemDestroyingDelay = 0.2f;

private:
	UPROPERTY(Transient)
	APS_FieldSystemActor* _ImpactField;

	UPROPERTY(Transient)
	FTransform _FieldTransformOnGeneration;

	UPROPERTY(Transient)
	float _GenerateFieldTimestamp;

	UPROPERTY(Transient)
	float _MoveFieldDuration;

	UPROPERTY(Transient)
	bool _bCanMoveField;

#pragma region IPS_CanGenerateImpactField
	//------------------

protected:
	UFUNCTION()
	virtual void GenerateImpactField(const FHitResult& targetHit, const FVector extent = FVector::Zero())override;

	virtual void UpdateImpactField() override;

	virtual APS_FieldSystemActor* GetImpactField_Implementation() const override { return _ImpactField;};

	virtual TSubclassOf<APS_FieldSystemActor> GetFieldSystemClass() const override { return FieldSystemActor;};

#pragma endregion IPS_CanGenerateImpactField

	//------------------
#pragma endregion Destruction
	
};
