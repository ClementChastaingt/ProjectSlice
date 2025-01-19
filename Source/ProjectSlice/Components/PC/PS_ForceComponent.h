// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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
	void UpdatePushForce();
	
	UFUNCTION()
	void StartPush(const FInputActionInstance& inputActionInstance);
	
	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Bool OnPushEvent;
	
protected:
		/** AnimMontage to play each time we fire */	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push", meta=(UIMin="0", ClampMin="0"))
	float PushForce = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push", meta=(UIMin="0", ClampMin="0", ForceUnits="cm"))
	float SphereRadius = 50.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push", meta=(UIMin="0", ClampMin="0", ForceUnits="deg"))
	float ConeAngleDegrees = 35.0f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push", meta=(UIMin="0", ClampMin="0", ForceUnits="cm"))
	float ConeLength = 500.0f ;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push", meta=(UIMin="0", ClampMin="0", ForceUnits="sec"))
	float StepInterval = 100.0f; 
			
	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Push")
	USoundBase* PushSound;
	
	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Push")
	UAnimMontage* PushAnimation;

private:
	UPROPERTY(VisibleInstanceOnly, Category="Status")
	FHitResult _CurrentPushHitResult;

	UPROPERTY(VisibleInstanceOnly, Category="Status")
	float _PushForceWeight;


#pragma endregion Push
};
