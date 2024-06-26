// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "ProjectSlice/PC/PS_PlayerController.h"
#include "PS_ParkourComponent.generated.h"

class AProjectSliceCharacter;

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_ParkourComponent : public UCapsuleComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_ParkourComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebug = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|WallRun")
	bool bDebugWallRun = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|WallRun")
	bool bDebugWallRunJump = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|CameraTilt")
	bool bDebugCameraTilt = false;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;
	
	UPROPERTY(Transient)
	AProjectSlicePlayerController* _PlayerController;

#pragma region General
	//------------------

protected:

	UFUNCTION()
	void OnParkourDetectorBeginOverlapEventReceived(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex, bool bFromSweep, const FHitResult& sweepResult);

	UFUNCTION()
	void OnParkourDetectorEndOverlapEventReceived(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex);
	

//------------------
#pragma endregion General

#pragma region WallRun
	//------------------

public:
	UFUNCTION()
	void WallRunTick();

	UFUNCTION()
	void OnWallRunStart(AActor* otherActor);

	UFUNCTION()
	void OnWallRunStop();

	UFUNCTION()
	void CameraTilt(const int32 wallOrientationToPlayer, const float currentSeconds, const float startWallRunTimestamp);

	UFUNCTION()
	void JumpOffWallRun();

	//Getters && Setters
	 bool GetIsWallRunning() const
	{return bIsWallRunning;}

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="Is currently WallRunning"))
	bool bIsWallRunning = false;
		
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="Default player gravity scale"))
	float EnterVelocity  = 0.0f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="WallRun timer handler"))
	FTimerHandle WallRunTimerHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="Player to Wall direction"))
	AActor* Wall = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="WallRun start time in second"))
	float StartWallRunTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="WallRun tick current time in second"))
	float WallRunSeconds = TNumericLimits<float>().Lowest();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="Player to Wall direction"))
	FVector WallRunDirection = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Camera", meta=(ToolTip="Is currently resetting CameraTilt smoothly"))
	bool bIsResetingCameraTilt = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Camera", meta=(ToolTip="Camera Tilt rest start time in second"))
	float StartCameraTiltResetTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Camera", meta=(ToolTip="Camera Tilt roll on Start"))
	int32 StartCameraTiltRoll = 360;

	//-1:Left, 0:Forward, 1:Right
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(UIMin=-1, ClampMin=-1, UIMax=1, ClampMax=1, ToolTip="Wall to player direction, basiclly use for determine if Wall is to left or right from player"))
	int32 WallToPlayerDirection = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="Player to Wall Run velocity "))
	float VelocityWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="WallRun custom tick rate"))
	float WallRunTickRate = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun", meta=(UIMin = 0.f, ClampMin = 0.f, ForceUnits="deg", ToolTip="Wall to Player Cam Min Angle for accept launch a WallRun "))
	float MinEnterAngle = 8.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Force", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="WallRun force multiplicator"))
	float WallRunForceBoost = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category="Parameters|WallRun|Force", meta=(ToolTip="WallRun force interpolation curve"))
	UCurveFloat* WallRunForceCurve = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Force", meta=(UIMin = 0.f, ClampMin = 0.f, ForceUnits="s", ToolTip="Time to WallRun for start falling, falling occur after gravity fall"))
	float WallRunTimeToFall = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Jump", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="WallRun jump off force multiplicator "))
	float JumpOffForceMultiplicator = 1500.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category="Parameters|WallRun|Gravity", meta=(ToolTip="WallRun gravity interpolation curve"))
	UCurveFloat* WallRunGravityCurve = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Camera", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="Angle of the camera in relation to the wall when the player is stuck to it"))
	float WallRunCameraAngle = 20.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Camera", meta=(UIMin = 0.f, ClampMin = 0.f, ForceUnits="s", ToolTip="Camera rotation tilt duration"))
	float WallRunCameraTiltDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category="Parameters|WallRun|Camera", meta=(ToolTip="Camera rotation tilt interpolation curve"))
	UCurveFloat* WallRunCameraTiltCurve = nullptr;
	
private:
	//------------------

#pragma endregion WallRun
};


