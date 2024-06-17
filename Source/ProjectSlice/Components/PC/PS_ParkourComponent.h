// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "PS_ParkourComponent.generated.h"


class AProjectSliceCharacter;

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_ParkourComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_ParkourComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bDebug = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bDebugTick = false;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;
	
	UPROPERTY(Transient)
	APlayerController* _PlayerController;

#pragma region General
	//------------------

protected:

	UFUNCTION()
	void OnParkourDetectorBeginOverlapEventReceived(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex, bool bFromSweep, const FHitResult& sweepResult);

	UFUNCTION()
	void OnParkourDetectorEndOverlapEventReceived(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex);
	
	UPROPERTY(VisibleAnywhere, Category="Component", meta = (AllowPrivateAccess = "true"))
	UCapsuleComponent* ParkourDetector = nullptr;

	
	

//------------------
#pragma endregion General

#pragma region WallRun
	//------------------

public:
	UFUNCTION()
	void WallRunTick();
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="Is currently WallRunning"))
	bool bIsWallRunning = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="Default player gravity scale"))
	float DefaultGravity  = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status|WallRun", meta=(ToolTip="WallRun timer handler"))
	FTimerHandle WallRunTimerHandle;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status|WallRun", meta=(ToolTip="WallRun start time in second"))
	float StartWallRunTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status|WallRun", meta=(ToolTip="WallRun tick current time in second"))
	float WallRunTimestamp = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status|WallRun", meta=(ToolTip="Player to Wall direction"))
	FVector WallRunDirection = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="WallRun custom tick rate"))
	float WallRunTickRate = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Force", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="WallRun force multiplicator"))
	float WallRunForceMultiplicator = 1500.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Gravity", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="WallRun target gravity scale"))
	float WallRunTargetGravity = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Gravity", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="Time to WallRun for reach target gravity scale"))
	float WallRunTimeToMaxGravity = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category="Parameters|WallRun|Gravity", meta=(ToolTip="WallRun gravity interpolation curve"))
	UCurveFloat* WallRunGravityCurve = nullptr;
	
private:
	//------------------

#pragma endregion WallRun
};


