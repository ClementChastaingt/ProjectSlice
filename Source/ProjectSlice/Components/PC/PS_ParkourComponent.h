// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "ProjectSlice/Data/PS_Delegates.h"
#include "ProjectSlice/PC/PS_PlayerController.h"
#include "PS_ParkourComponent.generated.h"

class AProjectSliceCharacter;

UENUM()
enum class EMantlePhase : uint8
{
	NONE = 0	UMETA(DisplayName ="None"),
	SNAP = 1 UMETA(DisplayName ="Snap"),
	PULL_UP = 2 UMETA(DisplayName ="Pull up"),
};

UCLASS(Blueprintable, ClassGroup=(Component), meta=(BlueprintSpawnableComponent))
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|Crouch")
	bool bDebugCrouch = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|Crouch")
	bool bDebugSlide = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|Mantle")
	bool bDebugMantle = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|Ledge")
	bool bDebugLedge = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Component Tick", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="WallRun custom tick rate"))
	float CustomTickRate = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Component Tick", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="WallRun custom tick rate"))
	float CanStandTickRate = 0.1f;


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
	
	UFUNCTION()
	void OnMovementModeChangedEventReceived(ACharacter* character, EMovementMode prevMovementMode, uint8 previousCustomMode);
		
//------------------
#pragma endregion General

#pragma region Mantle
	//------------------

public:
	UFUNCTION(BlueprintCallable)
	FORCEINLINE EMantlePhase GetMantlePhase() const{return _MantlePhase;}

	FORCEINLINE bool IsMantling() const{return bIsMantling;}

	//------------------
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Mantle")
	bool bIsMantling = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Mantle|Snap")
	float StartMantleSnapTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Mantle|PullUp")
	float StartMantlePullUpTimestamp = TNumericLimits<float>().Lowest();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Mantle", meta=(UIMin="0", ClampMin="0", ForceUnits="deg", ToolTip="Max angle for try Mantle and not WallRun"))
	float MaxMantleAngle = 40.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Mantle", meta=(UIMin="0", ClampMin="0", ForceUnits="cm", ToolTip="Max Height for try Mantle"))
	float MaxMantleHeight = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Mantle", meta=(UIMin="0", ClampMin="0", ForceUnits="cm", ToolTip="Max Height for try Mantle"))
	float OnAiMaxCapsHeightMultiplicator = 1.5f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Mantle", meta=(ToolTip="Offset between capsule test and height test (for surface with asperities)"))
	float MantleCapsuletHeightTestOffset = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Mantle", meta=(UIMin="1", ClampMin="1", ToolTip="Rotation speed for facing climbed object interp movement"))
	float MantleFacingRotationSpeed = 10.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Mantle|PullUp", meta=(ToolTip="Mantle PullUp curve XY"))
	UCurveFloat* MantlePullUpCurveXY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Mantle|PullUp", meta=(ToolTip="Mantle PullUp curve Z"))
	UCurveFloat* MantlePullUpCurveZ;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Mantle|PullUp", meta=(ForceUnits="sec", ToolTip="Smooth PullUp Mantle duration"))
	float MantlePullUpDuration = 0.2f;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Mantle|Snap", meta=( ForceUnits="sec", ToolTip="Smooth Snap Mantle duration"))
	float MantlMaxSnapDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Mantle|Snap", meta=( ForceUnits="sec", ToolTip="Smooth Snap Mantle duration"))
	float MantleMinSnapDuration = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Mantle|Snap", meta=(ToolTip="Offset between obstacle border and player position for simulate 'arms lenght' "))
	float MantleSnapOffset = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Mantle|Snap", meta=(ToolTip="Mantle Snap curve"))
	UCurveFloat* MantleSnapCurve;
	
	UFUNCTION()
	bool CanMantle(const FHitResult& inFwdHit);

	UFUNCTION()
	void OnStartMantle();

	UFUNCTION()
	void OnStoptMantle();
	
	UFUNCTION()
	void MantleTick();

	UFUNCTION()
	void OnStartLedge(const FVector& targetLoc);

	UFUNCTION()
	void OnStopLedge();

	UFUNCTION()
	void LedgeTick() ;
	
private:
	UPROPERTY(Transient)
	EMantlePhase _MantlePhase = EMantlePhase::NONE;
	
	UPROPERTY(Transient)
	FVector _StartMantleLoc;

	UPROPERTY(Transient)
	FRotator _StartMantleRot;

	UPROPERTY(Transient)
	FVector _TargetMantlePullUpLoc;

	UPROPERTY(Transient)
	FVector _TargetMantleSnapLoc;
	
	UPROPERTY(Transient)
	FRotator _TargetMantleRot;
	
	//------------------
#pragma endregion Mantle

#pragma region Ledge
	//------------------

public:
	FORCEINLINE bool IsLedging() const{return bIsLedging;}

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Ledge")
	bool bIsLedging = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Ledge")
	float StartLedgeTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Ledge", meta=(ForceUnits="sec", ToolTip="Smooth Ledge duration"))
	float LedgeDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Ledge", meta=(ToolTip="Ledge curve"))
	UCurveFloat* LedgePullUpCurve;
	
	
private:
	UPROPERTY(Transient)
	FVector _StartLedgeLoc;
	
	UPROPERTY(Transient)
	FVector _TargetLedgeLoc;
	

#pragma endregion Ledge

#pragma region WallRun
	//------------------

public:
	UFUNCTION()
	void OnWallRunStart(AActor* otherActor);

	UFUNCTION()
	void OnWallRunStop();

	UFUNCTION()
	void JumpOffWallRun();

	//Getters && Setters
	FORCEINLINE bool IsWallRunning() const  {return bIsWallRunning;}

	void SetForceWallRun(bool bforceWallRun){this->bForceWallRun = bforceWallRun;}

protected:
	UFUNCTION()
	void WallRunTick();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="Is currently WallRunning"))
	bool bIsWallRunning = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="Is currently force WallRunning"))
	bool bForceWallRun = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Camera", meta=(UIMin=-1, ClampMin=-1, UIMax=1, ClampMax=1, ToolTip="Camera orientation to Wall, basiclly use for determine if Camera is rotate to left or right to Wall"))
	int32 CameraTiltOrientation = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="WallRun timer handler"))
	FTimerHandle WallRunTimerHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="Player to Wall direction"))
	AActor* Wall = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="WallRun start time in second"))
	float StartWallRunTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="WallRun tick current time in second"))
	float WallRunSeconds = TNumericLimits<float>().Lowest();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="WallRun move direction"))
	FVector WallRunDirection = FVector::ZeroVector;
	
	//-1:Left, 0:Forward, 1:Right
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(UIMin=-1, ClampMin=-1, UIMax=1, ClampMax=1, ToolTip="Player orientation from Wall, basiclly use for determine if Player is to left or right from Wall"))
	int32 WallToPlayerOrientation = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="Player to Wall Run velocity "))
	float VelocityWeight = 1.0f;	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Force", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="WallRun force multiplicator"))
	float WallRunSpeedBoost = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category="Parameters|WallRun|Force", meta=(ToolTip="WallRun force interpolation curve"))
	UCurveFloat* WallRunForceCurve = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Force", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="Max Speed multiplicator by DefaultMaxWalkSpeed"))
	float MaxWallRunSpeedMultiplicator = 1.5f;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Force", meta=(UIMin = 0.f, ClampMin = 0.f, ForceUnits="s", ToolTip="Fake input push force when input was not pressed"))
	float WallRunNoInputVelocity = 0.8f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Force", meta=(UIMin = 0.f, ClampMin = 0.f, ForceUnits="s", ToolTip="Time to WallRun for start falling, falling occur after gravity fall"))
	float WallRunTimeToFall = 2.0f;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category="Parameters|WallRun|Force", meta=(ToolTip="WallRun gravity interpolation curve"))
	UCurveFloat* WallRunGravityCurve = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Jump", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="WallRun jump off force multiplicator "))
	float JumpOffForceMultiplicator = 1500.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun", meta=(UIMin = 90.0f, ClampMin = 90.f, ForceUnits="deg", ToolTip="Jump Off Player forward Direction to Wall right dir threshold angle"))
	float JumpOffPlayerFwdDirThresholdAngle = 100.0f;
	
private:
	//------------------

	UPROPERTY(Transient, meta=(ToolTip="Start WallRun player velocity scale"))
	FVector _WallRunEnterVelocity;

#pragma endregion WallRun

#pragma region Crouch
	//------------------

public:
	
	//Crouch functions
	void OnCrouch();

	FORCEINLINE float GetSmoothCrouchDuration() const{ return SmoothCrouchDuration;}

protected:

	//Check if can Stand
	bool CanStand() const;

	UFUNCTION()
	void CanStandTick();

	//Crouch Tick func
	void Stooping();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Status|Crounch")
	bool bIsCrouched = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Crounch")
	bool bIsStooping = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Slide", meta=(ToolTip="Slide timer handler"))
	FTimerHandle CanStandTimerHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Crounch")
	float StartStoopTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Crounch")
	float StartCrouchHeight = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Crounch")
	float CrouchAlpha = TNumericLimits<float>().Lowest();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Crounch", meta=(ToolTip="Smooth crouch curve"))
	UCurveFloat* CrouchCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Crounch", meta=(ToolTip="Smooth crouch duration"))
	float SmoothCrouchDuration = 0.1f;

private:
	//------------------

#pragma endregion Crouch

#pragma region Slide
	//------------------

public:
	UFUNCTION()
	void OnStartSlide();

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Bool OnSlideEvent;

	FORCEINLINE bool IsSliding() const{return bIsSliding;}

protected:
	UFUNCTION()
	void OnStopSlide();

	UFUNCTION()
	void SlideTick();
	
	UFUNCTION()
	FVector CalculateFloorInflucence(const FVector& floorNormal) const;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slide")
	bool bIsSliding = false;

	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slide")
	// bool bIsSlidingOnSLope = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Slide", meta=(ToolTip="Slide timer handler"))
	FTimerHandle SlideTimerHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Slide")
	float StartSlideTimestamp = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Slide", meta=(ToolTip="Slide tick current time in second"))
	float SlideSeconds = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Slide", meta=(ToolTip="Current slide alpha"))
	float SlideAlpha = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Slide", meta=(ToolTip="Character Movement default Ground Friction"))
	FVector SlideDirection = FVector::ZeroVector;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Slide", meta=(ToolTip="Character Movement default Walking Braking Deceleration"))
	float DefaultBrakingDecelerationWalking = 2048.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Slide", meta=(ToolTip="Character Movement default Falling Braking Deceleration"))
	float DefaultBrakingDecelerationFalling = 400.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Slide", meta=(ToolTip="Character Movement default Ground Friction"))
	float DefaulGroundFriction = 8.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Slide", meta=(ToolTip="Floor Slope degree angle Pitch"))
	float OutSlopePitchDegreeAngle = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Slide", meta=(ToolTip="Floor Slope degree angle Roll"))
	float OutSlopeRollDegreeAngle = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Slide", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="Slide Boost Speed, use when slide on flat surface"))
	float SlideSpeedBoost = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Slide", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="Max Speed multiplicator by DefaultMaxWalkSpeed, use when slide on flat surface"))
	float MaxSlideSpeedMultiplicator = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Slide", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="Multiplicator used give a weight to slope on slide"))
	float SlopeForceDecelerationWeight = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Slide|Deceleration", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="Slide Max braking deceleration "))
	float MaxBrakingDecelerationSlide = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slide|Deceleration", meta=(UIMin = 0.f, ClampMin = 0.f,ToolTip="Time to Max Braking Deceleration"))
	float TimeToMaxBrakingDeceleration = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slide|Deceleration", meta=(ToolTip="Braking Deceleration Curve"))
	UCurveFloat* SlideAccelerationCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slide|Deceleration", meta=(ToolTip="Braking Deceleration Curve"))
	UCurveFloat* SlideBrakingDecelerationCurve;

private:
	//------------------

#pragma endregion Slide

#pragma region Dash
	//------------------

public:
	UFUNCTION()
	void OnDash();

	FORCEINLINE float GetDashSpeed() const{ return DashSpeed;}

protected:

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate OnDashEvent;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Dash", meta=(UIMin = 0.f, ClampMin = 0.f,ToolTip="Dash Speed"))
	float DashSpeed = 1500.0f;

private:
	//------------------

#pragma endregion Dash
};


