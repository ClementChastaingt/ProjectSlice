// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "Components/BoxComponent.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "ProjectSlice/Data/PS_Delegates.h"
#include "PS_HookComponent.generated.h"


class AProjectSlicePlayerController;
class UCableComponent;
class AProjectSliceCharacter;

USTRUCT(BlueprintType, Category = "Struct")
struct FSCableWarpParams
{
	GENERATED_BODY()

	FSCableWarpParams()
	{
	};

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	FHitResult OutHit = FHitResult();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	FVector CableStart = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	FVector CableEnd = FVector::ZeroVector;
};

UCLASS(Blueprintable, ClassGroup=(Component), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_HookComponent : public USceneComponent
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Component", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* HookThrower = nullptr;
	
	UPROPERTY(VisibleAnywhere, Category="Component", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* HookCollider = nullptr;

	UPROPERTY(VisibleAnywhere, Category="Component", meta = (AllowPrivateAccess = "true"))
	UCableComponent* FirstCable = nullptr;

	UPROPERTY(VisibleInstanceOnly, Category="Component", meta = (AllowPrivateAccess = "true"))
	UPhysicsConstraintComponent* HookPhysicConstraint = nullptr;

	/** FirstPerson ConstraintAttach */
	UPROPERTY(VisibleDefaultsOnly, Category=Root)
	UStaticMeshComponent* ConstraintAttachSlave;

	UPROPERTY(VisibleDefaultsOnly, Category=Root)
	UStaticMeshComponent* ConstraintAttachMaster;

	// UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	// UStaticMeshComponent* HookMesh = nullptr;

public:
	/** Sets default values for this component's properties */
	UPS_HookComponent();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Returns HookThrowerComp subobject **/
	UFUNCTION(BlueprintCallable)
	FORCEINLINE USkeletalMeshComponent* GetHookThrower() const { return HookThrower; }

	UFUNCTION(BlueprintCallable)
	FORCEINLINE UBoxComponent* GetHookCollider() const { return HookCollider; }

	UFUNCTION(BlueprintCallable)
	FORCEINLINE UCableComponent* GetFirstCable() const { return FirstCable; }

	/** Returns Swing system ConstraintAttachSlave subobject **/
	FORCEINLINE UStaticMeshComponent* GetConstraintAttachSlave() const { return ConstraintAttachSlave; }

	/** Returnss Swing system ConstraintAttachMaster subobject  **/
	FORCEINLINE UStaticMeshComponent* GetConstraintAttachMaster() const { return ConstraintAttachMaster; }

protected:	
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnMovementModeChangedEventReceived(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebug = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebugArm = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebugCable = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebugSwing = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebugPull = false;

private:
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;

	UPROPERTY(Transient)
	AProjectSlicePlayerController* _PlayerController;

#pragma region Event_Receiver
	//------------------

public:
	UFUNCTION()
	void InitHookComponent();

protected:

	UFUNCTION()
	void OnSlowmoTriggerEventReceived(const bool bIsSlowed);

	UFUNCTION()
	void OnParkourDetectorBeginOverlapEventReceived(UPrimitiveComponent* overlappedComponent, AActor* otherActor,
		UPrimitiveComponent* otherComp, int32 otherBodyIndex, bool
		bFromSweep, const FHitResult& sweepResult);

	//------------------
#pragma endregion Event_Receiver

#pragma region Arm
	//------------------

protected:
	UFUNCTION()
	void OnHookBoxBeginOverlapEvent(UPrimitiveComponent* overlappedComponent, AActor* otherActor,
		UPrimitiveComponent* otherComp, int32 otherBodyIndex, bool bFromSweep, const FHitResult& sweepResult);

	UFUNCTION()
	void OnHookBoxEndOverlapEvent(UPrimitiveComponent* overlappedComponent, AActor* otherActor,
		UPrimitiveComponent* otherComp, int32 otherBodyIndex);

	UFUNCTION()
	void InitArmPhysicalAnimation();

	UFUNCTION()
	void ToggleArmPhysicalAnimation(const bool bActivate);

	UFUNCTION()
	void ArmTick();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|Arm", meta=(UIMin="0", ClampMin="0", ForceUnits="sec"))
	float ArmPhysicAnimRecoveringDuration = 0.5f;

private:
	UPROPERTY(Transient)
	UPhysicalAnimationComponent* _PhysicAnimComp;

	UPROPERTY(Transient)
	bool _bArmIsRagdolled;

	UPROPERTY(Transient)
	bool _bBlendOutToUnphysicalized;

	UPROPERTY(Transient)
	float _ArmTogglePhysicAnimTimestamp;

#pragma endregion Arm

#pragma region Cable
	//------------------

public:

	UFUNCTION(BlueprintCallable)
	FORCEINLINE float GetAlphaTense() const{return _AlphaTense;}

protected:
	//Status	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Cable|Rope",
		meta=(ToolTip="Cable list array, each added cable including the first one will be stored here"))
	TArray<UCableComponent*> CableListArray;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Cable|Rope",
		meta=(ToolTip="Attached cables array, each attached cable point will be stored here."))
	TArray<UCableComponent*> CableAttachedArray;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Cable|Cable",
		meta=(ToolTip="Cable caps array, each added cap will be stored here"))
	TArray<UStaticMeshComponent*> CableCapArray;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Cable|Point",
		meta=(ToolTip="Cable points array, each hit component with the location will be stored here."))
	TArray<USceneComponent*> CablePointComponents;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Cable|Point",
		meta=(ToolTip="Cable points alphas, alpha value to detach for each point."))
	TArray<float> CablePointUnwrapAlphaArray;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Cable|Point",
		meta=(ToolTip="Default First Cable lenght"))
	UCurveFloat* CableTensCurve = nullptr;

	//Parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable",
		meta=(ToolTip=
			"Enable using of substep constant timer ticks instead of event tick, more stable tracing but issues like cable flickering"
		))
	bool bCanUseSubstepTick = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Point",
		meta=(ToolTip="Static Mesh use for Caps, basically sphere"))
	UStaticMesh* CapsMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Point",
		meta=(ToolTip="New Caps Material Instance"))
	UMaterialInterface* CableCapsMaterialInst = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Point",
		meta=(UIMin="0", ClampMin="0", ToolTip="Static Mesh scale multiplicator use for Caps (CableWeight * this)"))
	float CapsScaleMultiplicator = 0.0105;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope",
		meta=(ToolTip=
			"Use cable shared settings to the start cable, like width, length, basically all settings exlcuding the ones that cannot be changed at runtime, like segments, and etc."
		))
	bool bCableUseSharedSettings = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope",
		meta=(UIMin="0", ClampMin="0", ToolTip=
			"Cable error tolerance for wrapping, so there will be no duplicate points around already added ones, keep this low for smooth wrapping."
		))
	float CableWrapErrorTolerance = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope",
		meta=(UIMin="0", ClampMin="0", ToolTip=
			"Cable unwrap trace distance, between start point and second cable point, this value should be around from 5 to 20 for effective work."
		))
	float CableUnwrapDistance = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope",
		meta=(UIMin="0", ClampMin="0", ToolTip=
			"Cable unwrap error multiplier, when trace finds the closest point, this value should be less than 'unwrap distance' for effective work."
		))
	float CableUnwrapErrorMultiplier = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope",
		meta=(UIMin="0", ClampMin="0", ForceUnits="s", ToolTip=
			"The delay alpha frames for start/end points, before unwrapping the cable points, to prevent flickering cycles of wrap/unwrap, this should be around 3-7 for effective work."
		))
	float CableUnwrapFirstFrameDelay = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope",
		meta=(UIMin="0", ClampMin="0", ForceUnits="s", ToolTip=
			"The delay alpha frames for start/end points, before unwrapping the cable points, to prevent flickering cycles of wrap/unwrap, this should be around 3-7 for effective work."
		))
	float CableUnwrapLastFrameDelay = 4.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope|Tense", meta=(UIMin="1", ClampMin="1",  ToolTip="Min length cable determine by divide Distance Between Cable by it"))
	float CableMinLengthDivider = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope|Tense", meta=(UIMin="1", ClampMin="1",  ToolTip="Max length cable determine by multiplie Distance Between Cable by it"))
	float CableMaxLengthMultiplicator = 1.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope|Tense", meta=(UIMin="1", ClampMin="1", UIMax="16", ClampMax="16", ToolTip="Max layer depth of Adapt cable tense"))
	FFloatInterval CableSolverRange = FFloatInterval(8,16);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope|Tense",
		meta=(UIMin="0", ClampMin="0", ForceUnits="cm", ToolTip="Min && Max distance from Cable max tense. Is used by winde to give some soft or hard to rope so is used in (+ and -)"))
	FFloatInterval CablePullSlackDistanceRange = FFloatInterval(50.0f,500.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Debug",
		meta=(ToolTip="Change New Cable Material color randomly"))
	bool bDebugMaterialColors = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Debug",
		meta=(ToolTip="New Cable Material Instance"))
	bool bDisableCableCodeLogic = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Debug",
		meta=(ToolTip="New Cable Material Instance"))
	UMaterialInterface* CableDebugMaterialInst = nullptr;

	//Functions
	UFUNCTION()
	void SubstepTickLowLatency();

	UFUNCTION()
	void SubstepTickHighLatency();

	UFUNCTION()
	void CableWraping();

	UFUNCTION()
	void ConfigLastAndSetupNewCable(UCableComponent* lastCable, const FSCableWarpParams& currentTraceCableWarp,
		UCableComponent*& newCable, const bool
		bReverseLoc) const;

	UFUNCTION()
	void ConfigCableToFirstCableSettings(UCableComponent* newCable) const;
	
	UFUNCTION()
	void SetupCableMaterial(UCableComponent* newCable) const;

	UFUNCTION()
	void WrapCableAddByFirst();

	UFUNCTION()
	void WrapCableAddByLast();

	UFUNCTION()
	void UnwrapCableByFirst();

	UFUNCTION()
	void UnwrapCableByLast();

	UFUNCTION(BlueprintCallable)
	bool TraceCableUnwrap(const UCableComponent* pastCable, const UCableComponent* currentCable,
		const bool& bReverseLoc, FHitResult& outHit) const;

	UFUNCTION(BlueprintCallable)
	FSCableWarpParams TraceCableWrap(const UCableComponent* cable, const bool bReverseLoc) const;

	UFUNCTION(BlueprintCallable)
	void AddSphereCaps(const FSCableWarpParams& currentTraceParams, const bool bIsAddByFirst);

	//Check if this location is not existing already in "cable points locations", error tolerance to determine how close another wrap point can be added
	UFUNCTION(BlueprintCallable)
	bool CheckPointLocation(const FVector& targetLoc,
		const float& errorTolerance);
	
	UFUNCTION()
	void AdaptCableTense(const float alphaTense);

private:

	UPROPERTY(Transient)
	FTimerHandle _LowSubstepTickHandler;

	UPROPERTY(Transient)
	FTimerHandle _HighSubstepTickHandler;

	UPROPERTY(Transient)
	float _FirstCableDefaultLenght;

	UPROPERTY(DuplicateTransient)
	float _CablePullSlackDistance = CablePullSlackDistanceRange.Min;

	UPROPERTY(Transient)
	float _DistOnAttachWithRange;
	
	UPROPERTY(Transient)
	float _AlphaTense;

#pragma endregion Cable

#pragma region Grapple
	//------------------

public:
	UFUNCTION()
	void HookObject();

	UFUNCTION()
	void DettachHook();
	
	UFUNCTION(BlueprintCallable)
	FORCEINLINE UMeshComponent* GetAttachedMesh() const { return _AttachedMesh; }

	UFUNCTION(BlueprintPure)
	FORCEINLINE bool IsObjectHooked() const { return IsValid(_AttachedMesh); }

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate OnHookObjectFailed;
	
protected:
	UFUNCTION()
	void AttachCableToHookThrower(UCableComponent* cableToAttach = nullptr) const;

	UPROPERTY(VisibleInstanceOnly, Transient, Category="Parameter|Hook", meta=(UIMin=0.0f, ClampMin=0.0f, ForceUnits="cm"))
	float LaserActivationDistanceThreshold = 100.0f;

private:
	//Status
	UPROPERTY(VisibleInstanceOnly, Transient, Category="Status|Hook")
	UMeshComponent* _AttachedMesh;

	UPROPERTY(Transient)
	bool _bHookTrajectoryImpactIsBad;

#pragma region Pull
	//------------------

#pragma region Winde
	//------------------

public:
	UFUNCTION()
	void WindeHook(const FInputActionInstance& inputActionInstance);

	UFUNCTION()
	void ResetWinde(const FInputActionInstance& inputActionInstance);

	UFUNCTION(BlueprintCallable)
	FORCEINLINE bool IsCableWinderInUse() const{return _bCableWinderIsActive;}

protected:

	UFUNCTION()
	void ResetWindeHook();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull|Winde",
		meta=(UIMin="0", ClampMin="0", ToolTip="Max input Winde iteration to do for reach max tense or slack"))
	float WindeMaxInputWeight = 10;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull|Winde",
		meta=(UIMin="0", ClampMin="0", ForceUnits="s", ToolTip="Max Force Winde Weight curve"))
	UCurveFloat* WindePullingCurve;

private:

	UPROPERTY(VisibleInstanceOnly, Transient, Category="Status|Hook|Pull")
	bool _bCableWinderIsActive;

	UPROPERTY(VisibleInstanceOnly, Transient, Category="Status|Hook|Pull")
	float _CableWindeInputValue;

	UPROPERTY(Transient)
	float _AlphaWinde;
	
	UPROPERTY(Transient)
	float _WindeInputAxis1DValue;
		
	//------------------
#pragma endregion Winde

public:
	UFUNCTION(BlueprintPure)
	FORCEINLINE float GetForceWeight() const { return _ForceWeight; }

	UFUNCTION(BlueprintPure)
	FORCEINLINE float GetMaxForceWeight() const { return MaxForceWeight; }

	UFUNCTION(BlueprintPure)
	FORCEINLINE bool IsCablePowerPull() const{ return _bCablePowerPull;}

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Bool OnHookObject;

protected:
	UFUNCTION()
	void DetermineForceWeight(const float alpha);

	UFUNCTION()
	float CalculatePullAlpha(const float baseToMeshDist);

	UFUNCTION()
	void CheckingIfObjectIsBlocked();

	UFUNCTION()
	void ForceDettachByFall();

	UFUNCTION()
	void PowerCablePull();
	
	UFUNCTION()
	void OnAttachedSameLocTimerEndEventReceived();

	UFUNCTION()
	void OnPushTimerEndEventReceived(const FTimerHandle selfHandler, const FVector& currentPushDir, const float pushAccel);

	//Parameters	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull", meta=(UIMin="10", ClampMin="10", ForceUnits="cm/s", ToolTip="Max Force Weight for Pulling object to Player"))
	float MaxForceWeight = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull", meta=(UIMin="10", ClampMin="10", ForceUnits="cm/s", ToolTip="Caps weight multply by CapsNum for add to MaxForceWeight"))
	float ForceCapsWeight = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull", meta=(UIMin="10", ClampMin="10", ForceUnits="cm/s", ToolTip="Maximum Force authorized"))
	float ForceWeightMaxThreshold = 150000.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Hook|Pull", meta=(UIMin="100", ClampMin="100", ForceUnits="kg"))
	float MaxPullWeight = 10000.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull", meta=(UIMin="0", ClampMin="0", ForceUnits="deg",  ToolTip="Maximum random Yaw Offset applicate to the pull direction"))
	float PullingMaxRandomYawOffset = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull|Unblock", meta=(UIMax="0.0", ClampMax="0.0", ToolTip="Player VelocityZ Or ObjectHooked VelocityZ Min falling velocityZ to Break"))
	float VelocityZToleranceToBreak = -150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull|Unblock", meta=(UIMin="0.01", ClampMin="0.01", ForceUnits="s", ToolTip="Player VelocityZ to ObjectHooked VelocityZ check lactency"))
	float BreakByFallCheckLatency = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull|Unblock", meta=(UIMin="0.01", ClampMin="0.01", ForceUnits="s", ToolTip="Latency between unblock Push steps"))
	float UnblockPushLatency = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull|Unblock", meta=(UIMin="1", ClampMin="1", ToolTip="Unblock maximum iteration steps"))
	int32 UnblockMaxIterationCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull|Unblock", meta=(UIMin="1", ClampMin="1", ForceUnits="cm", ToolTip= "Max Distance threshold between old and new attached object loc to consider object is at same location between frames last and actual frame"))
	float AttachedMaxDistThreshold = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull|Unblock", meta=(UIMin="0.01", ClampMin="0.01", ForceUnits="s", ToolTip="Max duration authorized for Attached to stay at same location before switching to unblocking pull method"))
	float AttachedSameLocMaxDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull|Unblock", meta=(UIMin="1", ClampMin="1", ToolTip="Divider of the default pull force weight when unblock logic is running"))
	float UnblockDefaultPullforceDivider = 6.0f;

private:
	UPROPERTY(Transient)
	FHitResult _CurrentHookHitResult;

	UPROPERTY(Transient)
	double _DistanceOnAttach;
	
	UPROPERTY(VisibleInstanceOnly, Transient, Category="Status|Hook|Pull")
	bool _bCablePowerPull;

	UPROPERTY(Transient)
	float _ForceWeight;
	
	UPROPERTY(VisibleInstanceOnly, Transient, Category="Status|Hook|Pull")
	bool _bAttachObjectIsBlocked;
	
	UPROPERTY(Transient)
	FVector _LastAttachedActorLoc;

	UPROPERTY(Transient)
	FTimerHandle _AttachedSameLocTimer;

	UPROPERTY(Transient)
	TArray<FTimerHandle> _UnblockTimerTimerArray;

	UPROPERTY(Transient)
	bool bHasTriggerBreakByFall;
	
	//------------------
#pragma endregion Pull
	
	//------------------
#pragma endregion Grapple

#pragma region Swing
	//------------------

public:
	UFUNCTION()
	void SwingTick();

	UFUNCTION()
	void OnTriggerSwing(const bool bActivate);
	
	UFUNCTION()
	void OnSwingPhysic();
	
	UFUNCTION()
	void ForceUpdateMasterConstraint();

	UFUNCTION()
	void ImpulseConstraintAttach() const;

	UFUNCTION()
	void ForceInvertSwingDirection() { _VelocityToAbsFwd = _VelocityToAbsFwd * -1; };

	FORCEINLINE bool IsPlayerSwinging() const { return _bPlayerIsSwinging; }

	UFUNCTION()
	void SetSwingPlayerLastLoc(const FVector& swingPlayerLastLoc) { _SwingPlayerLastLoc = swingPlayerLastLoc; }

protected:	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing",
		meta=(ForceUnits="s", ToolTip=
			"Swing Max duration || In physic context it's tduration of LinearZ Decrementation "))
	float SwingMaxDuration = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing",
	meta=(ForceUnits="s", ToolTip=
		"Swing Max duration || In physic context it's tduration of LinearZ Decrementation "))
	FFloatInterval SwingLinearLimitRange = FFloatInterval(15,750);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing",
	meta=(ToolTip="Swing force multiplicator"))
	float SwingCustomAirControlMultiplier = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing",
		meta=(ToolTip="Swing force multiplicator"))
	float SwingMaxAirControl = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing", meta=(UIMin="1.0", ClampMin="1.0",
		ToolTip="Swing impluse force multiplicator. It multiplcate the enter velocity for exit swing by jump launch OR enter in Swing"))
	float SwingImpulseMultiplier = 2.0f;

private:
	UPROPERTY(Transient, meta=(ToolTip="Is player currently swinging"))
	bool _bPlayerIsSwinging = false;

	UPROPERTY(Transient, meta=(ToolTip="Swing force multiplicator"))
	float _SwingStartTimestamp = TNumericLimits<float>::Min();

	UPROPERTY(Transient, meta=(ToolTip="Swing input force alpha"))
	float _SwingAlpha = 0.0f;
	
	UPROPERTY(Transient)
	bool _OrientationIsReset;

	UPROPERTY(Transient)
	float _VelocityToAbsFwd;

	UPROPERTY(Transient)
	float _SwingImpulseForce;

	UPROPERTY(Transient)
	FVector _SwingPlayerLastLoc;


#pragma endregion Swing
};
