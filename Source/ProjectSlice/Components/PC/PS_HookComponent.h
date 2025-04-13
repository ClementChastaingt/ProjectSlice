// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebug = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebugTick = false;

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
	void OnInitWeaponEventReceived();

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|Arm")
	float ArmRepulseStrenght = 10.0f;

#pragma endregion Arm

#pragma region Cable
	//------------------

public:
	UFUNCTION(BlueprintCallable)
	FORCEINLINE float GetAlphaTense() const { return _AlphaTense; }

protected:
	//Status	
	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Cable|Rope", meta=(ToolTip="start adding from first cable only if there is more than one cable. Basically the next cable should be added from end first, then we can start extending also from start, by inserting points between."))
	// bool bIsAddByFirst = false;

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
		meta=(ToolTip="Cable points array, each attached point will be stored here."))
	TArray<FVector> CablePointLocations;

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
		meta=(ToolTip=
			"Use cable sphere caps, basically spawns dynamic mesh points on corners with new cable points to make smoother looks"
		))
	bool bCanUseSphereCaps = true;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope",
		meta=(UIMin="0", ClampMin="0", ForceUnits="cm", ToolTip="Max distance from Cable was max tense"))
	float CablePullSlackDistance = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope",
		meta=(UIMin="0", ClampMin="0", ForceUnits="cm", ToolTip="Max distance from Cable was max tense"))
	float CableBreakTensDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope",
		meta=(UIMin="0", ClampMin="0", ForceUnits="cm", ToolTip="Max mass threshold support by Cable before break"))
	float CableMaxTensVelocityThreshold = 1000.0f;

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
	void SubstepTick();

	UFUNCTION()
	void CableWraping();

	UFUNCTION()
	void ConfigLastAndSetupNewCable(UCableComponent* lastCable, const FSCableWarpParams& currentTraceCableWarp,
		UCableComponent*& newCable, const bool
		bReverseLoc) const;

	UFUNCTION()
	void ConfigCableToFirstCableSettings(UCableComponent* newCable) const;
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
	bool CheckPointLocation(const TArray<FVector>& pointsLocation, const FVector& targetLoc,
		const float& errorTolerance);

	UFUNCTION()
	void AdaptCableTens();

private:
	UPROPERTY(Transient)
	float _AlphaTense;

	UPROPERTY(Transient)
	FTimerHandle _SubstepTickHandler;

	UPROPERTY(Transient)
	float _FirstCableDefaultLenght;

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
	
protected:
	UFUNCTION()
	void AttachCableToHookThrower(UCableComponent* cableToAttach = nullptr) const;

private:
	//Status
	UPROPERTY(VisibleInstanceOnly, Transient, Category="Status|Hook")
	UMeshComponent* _AttachedMesh;

#pragma region Pull
	//------------------

#pragma region Winde
	//------------------

public:
	UFUNCTION()
	void WindeHook(const FInputActionInstance& inputActionInstance);

	UFUNCTION()
	void StopWindeHook();
	
	UFUNCTION(BlueprintPure)
	FORCEINLINE bool IsCableWinderPull() const { return _bCableWinderPull; }
protected:

	UFUNCTION()
	void ResetWindeHook();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull|Winde",
		meta=(UIMin="0", ClampMin="0", ForceUnits="s", ToolTip=
			"Duration for reach Max Force Winde Weight by winde holding"))
	float MaxWindePullingDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull|Winde",
		meta=(UIMin="0", ClampMin="0", ForceUnits="s", ToolTip="Max Force Winde Weight curve"))
	UCurveFloat* WindePullingCurve;

private:
	UPROPERTY(VisibleInstanceOnly, Transient, Category="Status|Hook|Pull")
	bool _bCableWinderPull;

	UPROPERTY(Transient)
	float _CableWindeInputValue;

	UPROPERTY(Transient)
	float _CableStartWindeTimestamp;

	UPROPERTY(Transient)
	FTimerHandle _CableWindeMouseCooldown;

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
	float CalculatePullAlpha(const float baseToMeshDist, const float distanceOnAttachByTensorCount);

	UFUNCTION()
	void CheckingIfObjectIsBlocked();

	UFUNCTION()
	void PowerCablePull();
	
	UFUNCTION()
	void OnAttachedSameLocTimerEndEventReceived();

	UFUNCTION()
	void OnUnblockPushTimerEndEventReceived(const FTimerHandle selfHandler, const FVector& currentPushDir, const float pushAccel);

	//Parameters	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull", meta=(UIMin="10", ClampMin="10", ForceUnits="cm/s", ToolTip="Max Force Weight for Pulling object to Player"))
	float MaxForceWeight = 10000.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Hook|Pull", meta=(UIMin="100", ClampMin="100", ForceUnits="kg"))
	float MaxPullWeight = 10000.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull", meta=(UIMin="0", ClampMin="0", ForceUnits="cm", ToolTip="Distance for reach Max Force Weight by distance to object"))
	float MaxForcePullingDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull", meta=(UIMin="0", ClampMin="0", ForceUnits="deg",  ToolTip="Maximum random Yaw Offset applicate to the pull direction"))
	float PullingMaxRandomYawOffset = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull", meta=(UIMin="0.01", ClampMin="0.01", ForceUnits="s", ToolTip="Unblock Push Latency"))
	float UnblockPushLatency = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull", meta=(UIMin="1", ClampMin="1", ForceUnits="cm", ToolTip= "Max Distance threshold between old and new attached object loc to consider object is at same location between frames last and actual frame"))
	float AttachedMaxDistThreshold = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull", meta=(UIMin="0.01", ClampMin="0.01", ForceUnits="s", ToolTip="Max duration authorized for Attached to stay at same location before switching to unblocking pull method"))
	float AttachedSameLocMaxDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Pull", meta=(UIMin="0.1", ClampMin="0.1", ToolTip="When object is blocked, this var is the multiplier of ForceWeight to use for move away push"))
	float MoveAwayForceMultiplicator= 5.0f;

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
	void OnSwingForce();

	UFUNCTION()
	void OnSwingPhysic();

	UFUNCTION()
	void ImpulseConstraintAttach() const;

	UFUNCTION()
	void ForceInvertSwingDirection() { _VelocityToAbsFwd = _VelocityToAbsFwd * -1; };

	FORCEINLINE bool IsPlayerSwinging() const { return bPlayerIsSwinging; }

	UFUNCTION()
	void SetSwingPlayerLastLoc(const FVector& swingPlayerLastLoc) { _SwingPlayerLastLoc = swingPlayerLastLoc; }

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Hook|Swing",
		meta=(ToolTip="Is player currently swinging"))
	bool bPlayerIsSwinging = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Hook|Swing",
		meta=(ToolTip="Swing force multiplicator"))
	float SwingStartTimestamp = TNumericLimits<float>::Min();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Hook|Swing",
		meta=(ToolTip="Swing input force alpha"))
	float SwingAlpha = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing",
		meta=(ToolTip="Use physic constraint swing or force"))
	bool bSwingIsPhysical = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing",
		meta=(ForceUnits="cm", ToolTip="Swing distance on the lower position of the trajectory", EditCondition=
			"!bSwingIsPhysical", EditConditionHides))
	float SwingMaxDistance = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing",
		meta=(ForceUnits="s", ToolTip=
			"Swing Max duration || In physic context it's tduration of LinearZ Decrementation "))
	float SwingMaxDuration = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing",
		meta=(ToolTip="Swing force multiplicator"))
	float SwingMaxAirControl = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing",
		meta=(ToolTip="Swing force multiplicator"))
	float SwingCustomAirControlMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing",
		meta=(ToolTip="Swing input weight divider", EditCondition="!bSwingIsPhysical", EditConditionHides))
	float SwingInputScaleDivider = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing",
		meta=(ToolTip="Use physic constraint swing or force", EditCondition="bSwingIsPhysical", EditConditionHides))
	float MinLinearLimitZ = 500.0f;

private:
	UPROPERTY(Transient)
	FVector _SwingStartLoc;

	UPROPERTY(Transient)
	FVector _SwingStartFwd;

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
