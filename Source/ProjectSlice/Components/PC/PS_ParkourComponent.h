// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "ProjectSlice/Character/PC/PS_PlayerController.h"
#include "ProjectSlice/Data/PS_Delegates.h"
#include "ProjectSlice/Data/PS_GlobalType.h"
#include "PS_ParkourComponent.generated.h"

class AProjectSliceCharacter;

UENUM()
enum class EMantlePhase : uint8
{
	NONE = 0	UMETA(DisplayName ="None"),
	SNAP = 1 UMETA(DisplayName ="Snap"),
	PULL_UP = 2 UMETA(DisplayName ="Pull up"),
};

UENUM()
enum class EDashType : uint8
{
	STANDARD = 0 UMETA(DisplayName ="Standard"),
	SWING = 1 UMETA(DisplayName ="Swing"),
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|Movement")
	bool bDebugSteering = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|WallRun")
	bool bDebugWallRun = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|WallRun")
	bool bDebugWallRunJump = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|Crouch")
	bool bDebugCrouch = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|Crouch")
	bool bDebugSlide = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|Crouch")
	bool bDebugDash = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|Mantle")
	bool bDebugMantle = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|Ledge")
	bool bDebugLedge = false;


public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;
	
	UPROPERTY(Transient)
	AProjectSlicePlayerController* _PlayerController;
	
	UPROPERTY(Transient)
	float _DefaultBrakingDecelerationWalking = 2048.0f;

	UPROPERTY(Transient)
	float _DefaultBrakingDecelerationFalling = 400.0f;

	UPROPERTY(Transient)
	float _DefaulGroundFriction = 8.0f;

#pragma region General
	//------------------

protected:	
	UFUNCTION()
	void OnParkourDetectorBeginOverlapEventReceived(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex, bool bFromSweep, const FHitResult& sweepResult);

	UFUNCTION()
	void OnParkourDetectorEndOverlapEventReceived(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex);
	
	UFUNCTION()
	void OnMovementModeChangedEventReceived(ACharacter* character, EMovementMode prevMovementMode, uint8 previousCustomMode);

	UFUNCTION()
	void ToggleObstacleLockConstraint(const AActor* const otherActor, UPrimitiveComponent* const otherComp,
		const bool bActivate) const;

private:
	
	UPROPERTY(Transient)
	AActor* _ActorOverlap;

	UPROPERTY(Transient)
	UPrimitiveComponent* _ComponentOverlap;
		
//------------------
#pragma endregion General

#pragma region Mantle
	//------------------

public:
	UFUNCTION(BlueprintCallable)
	FORCEINLINE EMantlePhase GetMantlePhase() const{return _MantlePhase;}

	FORCEINLINE bool IsMantling() const{return bIsMantling;}

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Bool OnTriggerMantleEvent;

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate OnPullUpMantleEvent;
	
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Mantle", meta=(UIMin="0", ClampMin="0", ToolTip="Max Height for try Mantle"))
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
	bool TryStartWallRun(AActor* otherActor, const FHitResult& hitResult);

	UFUNCTION()
	void StopWallRun();

	UFUNCTION()
	void JumpOffWallRun();

	//Getters && Setters
	UFUNCTION(BlueprintCallable)
	FORCEINLINE bool IsWallRunning() const  {return _bIsWallRunning;}

	FORCEINLINE FVector GetWallRunDirection() const{return _WallRunDirection;}

protected:
	
	UFUNCTION()
	bool FindWallOrientationFromPlayer(int32& playerToWallOrientation);

	UFUNCTION()
	void WallRunTick(const float& deltaTime);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Activation", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="WallRun force multiplicator"))
	float WallRunSpeedBoost = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Activation", meta=(ForceUnits="deg", UIMin = 0.f, ClampMin = 0.f, ToolTip="WallRun min angle, under player face the wall and stop chaining wallrun"))
	float WallRunForwardViewAngleBlocked = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Activation", meta=(ForceUnits="deg", UIMin = 0.f, ClampMin = 0.f, ToolTip="WallRun fwd angle dead zone, forward actor to capsule vel angle"))
	float WallRunEnterMaxAngle = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Activation", meta=(ForceUnits="s", UIMin = 0.f, ClampMin = 0.f, ToolTip="WallRun cooldown duration"))
	float WallRunCooldownDuration = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category="Parameters|WallRun|Force", meta=(ToolTip="WallRun force interpolation curve"))
	UCurveFloat* WallRunForceCurve = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Force", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="Max Speed multiplicator by DefaultMaxWalkSpeed"))
	float MaxWallRunSpeedMultiplicator = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Force", meta=(UIMin="0", ClampMin="0", ForceUnits="deg", ToolTip="Max angle threshold for decrease velocity hard when looking backward from wallrun direction"))
	float WallRunMaxCamOrientationAngle = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Force", meta=(ToolTip="Min velocity threshold to maintain WallRunning"))
	float MinWallRunVelocityThreshold = 200.0f;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Input", meta=(UIMin = 0.f, ClampMin = 0.f, ForceUnits="cm/s", ToolTip="Fake input weight use when input was not pressed"))
	float WallRunDefaultInputWeight = 0.8f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Fall", meta=(UIMin = 0.f, ClampMin = 0.f, ForceUnits="s", ToolTip="Time to WallRun for start falling, falling occur after gravity fall"))
	float WallRunTimeToFall = 2.0f;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category="Parameters|WallRun|Fall", meta=(ToolTip="WallRun gravity interpolation curve"))
	UCurveFloat* WallRunGravityCurve = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|WallRun|Jump", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="WallRun jump off force multiplicator "))
	float JumpOffForceSpeed = 750.0f;
	
	
private:
	
	UPROPERTY(Transient)
	bool _bIsWallRunning;

	UPROPERTY(Transient)
	int32 _JumpCountOnWallRunning;

	//Player orientation from Wall, basiclly use for determine if Player is on the left or the right from Wall
	//-1:Left, 0:Forward, 1:Right
	UPROPERTY(DuplicateTransient)
	int32 _PlayerToWallOrientation = 0;

	//Camera orientation to Wall, basiclly use for determine if Camera is rotate to left or right to Wall
	UPROPERTY(DuplicateTransient)
	int32 _WallRunCameraTiltOrientation = 0;

	UPROPERTY(Transient)
	FHitResult _WallRunSideHitResult;
	
	UPROPERTY(Transient)
	float _StartWallRunTimestamp;

	UPROPERTY(Transient)
	float _WallRunSeconds;

	UPROPERTY(Transient)
	AActor* _Wall;
		
	UPROPERTY(DuplicateTransient)
	FVector _WallRunDirection = FVector::ZeroVector;

	UPROPERTY(DuplicateTransient)
	float _VelocityWeight = 1.0f;
	
	UPROPERTY(Transient)
	FVector _WallRunEnterVelocity;

	UPROPERTY(Transient)
	FTimerHandle _WallRunResetTimerHandle;

#pragma endregion WallRun

#pragma region Crouch
	//------------------

public:
	
	//Crouch functions
	void OnCrouch();

	FORCEINLINE float GetSmoothCrouchDuration() const{ return SmoothCrouchDuration;}

protected:

	//Check if can Stand
	bool CanStand();

	UFUNCTION()
	void CanStandTick();

	//Crouch Tick func
	void Stooping(float deltaTime);
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Status|Crounch")
	bool bIsCrouched = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Crounch")
	bool bIsStooping = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Crounch")
	float StartCrouchHeight = TNumericLimits<float>().Lowest();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Crounch")
	float CrouchAlpha = TNumericLimits<float>().Lowest();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Crounch", meta=(ToolTip="Smooth crouch curve"))
	UCurveFloat* CrouchCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Crounch", meta=(ToolTip="Smooth crouch duration"))
	float SmoothCrouchDuration = 0.1f;

private:
	UPROPERTY(Transient)
	float _StoopTime;

	UPROPERTY(Transient)
	bool _bMustCheckStandInTick;

#pragma endregion Crouch

#pragma region Slide
	//------------------

public:
	UFUNCTION()
	void OnStartSlide();
	
	UFUNCTION(BlueprintCallable)
	FORCEINLINE bool IsSliding() const{ return _bIsSliding;}

	UFUNCTION(BlueprintCallable)
	FORCEINLINE float GetSlideAlphaFeedback() const{ return _SlideAlphaFeedback;}

	UFUNCTION(BlueprintCallable)
	FORCEINLINE FVector GetDefaultSlideDirection() const{return _DefaultSlideDirection;}

	UFUNCTION(BlueprintCallable)
	FORCEINLINE FVector GetSteeredSlideDirection() const{return _SteeredSlideDirection;}

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Bool OnSlideEvent;

protected:
	UFUNCTION()
	void OnStopSlide();

	UFUNCTION()
	void SlideTick(float deltaTime);
	
	UFUNCTION()
	FVector CalculateFloorInflucence(const FVector& floorNormal) const;

	UFUNCTION()
	void ApplySlideSteering(FVector& movementDirection, FVector2D inputValue, float alpha);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Slide|Speed", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="Slide Boost Speed, use when slide on flat surface"))
	float SlideSpeedBoost = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Slide|Speed", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="DefaultMaxWalkSpeed multiplicator used for define maxSpeed authorized"))
	float MaxSlideSpeedMultiplicator = 5.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Slide|Slope", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="DefaultMaxWalkSpeed multiplicator used for define maxSpeed authorized"))
	float FloorInfluenceVelocityWeight = 1500000.0f;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Slide|Slope", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="Multiplicator used give a weight to slope on slide"))
	float SlopeForceDecelerationWeight = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Slide|Deceleration", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="Slide Max braking deceleration "))
	float MaxBrakingDecelerationSlide = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slide|Deceleration", meta=(UIMin = 0.f, ClampMin = 0.f,ToolTip="Time to Max Braking Deceleration"))
	float TimeToMaxBrakingDeceleration = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slide|Deceleration", meta=(ToolTip="Braking Deceleration Curve"))
	UCurveFloat* SlideAccelerationCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slide|Deceleration", meta=(ToolTip="Braking Deceleration Curve"))
	UCurveFloat* SlideBrakingDecelerationCurve;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slide|Steering")
	bool bUseSteering = true;
	
	// The closest the character is to its Max ReferenceMovementSpeed, the more the SteeringSpeed gets closer to Min SteeringSpeed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slide|Steering", meta=(ClampMin=1, UIMin=1, EditCondition = "bUseSteering", EditConditionHides))
	FFloatInterval SteeringSpeed = FFloatInterval(400.f, 600.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slide|Steering",
	meta = (EditCondition = "bUseSteering", EditConditionHides))
	UCurveFloat* SteeringSpeedCurve = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slide|Steering", meta = (ClampMin=0.1, UIMin=0.1, EditCondition = "bUseSteering", EditConditionHides))
	float SteeringSmoothingInterpSpeed = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Slide|Steering",
	meta = (EditCondition = "bUseSteering", EditConditionHides))
	UCurveFloat* SteeringSmoothCurve = nullptr;

private:
	UPROPERTY(Transient)
	bool _bIsSliding;

	UPROPERTY(Transient)
	float _SlideAlphaFeedback;

	UPROPERTY(Transient)
	float _SlideSeconds;
	
	UPROPERTY()
	FVector _DefaultSlideDirection;

	UPROPERTY()
	FVector _SteeredSlideDirection;

	UPROPERTY(Transient)
	FRotator _SlideStartRot;
	
	UPROPERTY(Transient)
	float _OutSlopePitchDegreeAngle;

	UPROPERTY(Transient)
	float _OutSlopeRollDegreeAngle;

	UPROPERTY(Transient)
	int32 _SteeringSign;
	
	UPROPERTY(Transient)
	FVector2D _LastSteeringInputDirection;

	UPROPERTY(Transient)
	bool _bIsSteeringCurrentlyTilting;
	
#pragma endregion Slide

#pragma region Dash
	//------------------

public:
	UFUNCTION()
	void OnDash();

	UFUNCTION()
	void ResetDash();

	FORCEINLINE float GetDashSpeed() const{ return DashSpeed;}

	UFUNCTION(BlueprintCallable)
	FORCEINLINE FVector2D GetDashFeedbackDirection() const{ return _DashFeedbackDir;}

	UFUNCTION(BlueprintCallable)
	FORCEINLINE bool IsDashing() const{return _bIsDashing;}

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate OnDashEvent;

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate OnResetDashEvent;

protected:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Dash", meta=(UIMin = 0.f, ClampMin = 0.f,ToolTip="Dash Speed"))
	float DashSpeed = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Dash", meta=(UIMin = 0.f, ClampMin = 0.f ,ForceUnits="s",ToolTip="Dash duration"))
	float DashDuration = 0.3f;

	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Dash", meta=(UIMin = 0.f, ClampMin = 0.f ,ForceUnits="s",ToolTip="Dash duration"))
	// float DashCooldown= 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Dash", meta=(UIMin = 0.f, ClampMin = 0.f,ToolTip="Dash ground friction"))
	float DashGroundFriction = 10.0f;

private:
	UPROPERTY(Transient)
	FTimerHandle _DashResetTimerHandle;

	UPROPERTY(Transient)
	bool _bIsDashing;

	UPROPERTY(Transient)
	FVector2D _DashFeedbackDir;

	// UPROPERTY(Transient)
	// FTimerHandle _DashCooldownTimerHandle;

#pragma endregion Dash
};


