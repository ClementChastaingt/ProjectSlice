// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PS_HookComponent.generated.h"


class AProjectSlicePlayerController;
class UCableComponent;
class AProjectSliceCharacter;

USTRUCT(BlueprintType, Category = "Struct")
struct FSCableWarpParams
{
	GENERATED_BODY()

	FSCableWarpParams(){};
	
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
	UStaticMeshComponent* HookThrower = nullptr;
	
	UPROPERTY(VisibleAnywhere,  Category="Component", meta = (AllowPrivateAccess = "true"))
	UCableComponent* FirstCable= nullptr;
	
	UPROPERTY(VisibleInstanceOnly, Category="Component", meta = (AllowPrivateAccess = "true"))
	UPhysicsConstraintComponent* HookPhysicConstraint = nullptr;

	/** FirstPerson ConstraintAttach */
	UPROPERTY(VisibleDefaultsOnly, Category=Root)
	UStaticMeshComponent* ConstraintAttachSlave;
	
	/** FirstPerson ConstraintAttach */
	UPROPERTY(VisibleDefaultsOnly, Category=Root)
	UStaticMeshComponent* ConstraintAttachMaster;
	
	
	// UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	// UStaticMeshComponent* HookMesh = nullptr;

public:
	/** Sets default values for this component's properties */
	UPS_HookComponent();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Returns FirstPersonCameraComponent subobject **/
	UStaticMeshComponent* GetConstraintAttachSlave() const { return ConstraintAttachSlave; }

	/** Returns FirstPersonCameraComponent subobject **/
	UStaticMeshComponent* GetConstraintAttachMaster() const { return ConstraintAttachMaster; }


protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebug = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebugTick = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebugDrawLine = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebugSwing = false;
	
private:
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;
	
	UPROPERTY(Transient)
	AProjectSlicePlayerController* _PlayerController;

#pragma region Rope
	//------------------

public:
	//------------------
protected:
	//Status
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Cable|Rope", meta=(ToolTip="start adding from first cable only if there is more than one cable. Basically the next cable should be added from end first, then we can start extending also from start, by inserting points between."))
	bool bIsAddByFirst = false;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Cable|Rope", meta=(ToolTip="Cable list array, each added cable including the first one will be stored here"))
	TArray<UCableComponent*> CableListArray;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Cable|Rope", meta=(ToolTip="Attached cables array, each attached cable point will be stored here."))
	TArray<UCableComponent*> CableAttachedArray;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Cable|Cable", meta=(ToolTip="Cable caps array, each added cap will be stored here"))
	TArray<UStaticMeshComponent*> CableCapArray;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Cable|Point", meta=(ToolTip="Cable points array, each hit component with the location will be stored here."))
	TArray<USceneComponent*> CablePointComponents;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Cable|Point", meta=(ToolTip="Cable points array, each attached point will be stored here."))
	TArray<FVector> CablePointLocations;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Cable|Point", meta=(ToolTip="Cable points alphas, alpha value to detach for each point."))
	TArray<float> CablePointUnwrapAlphaArray;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Cable|Point", meta=(ToolTip="Default First Cable lenght"))
	float FirstCableDefaultLenght = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Cable|Point", meta=(ToolTip="Default First Cable lenght"))
	UCurveFloat* CableTensCurve = nullptr;
	
	//Parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Point", meta=(ToolTip="Use cable sphere caps, basically spawns dynamic mesh points on corners with new cable points to make smoother looks"))
	bool bCanUseSphereCaps = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Point", meta=(ToolTip="Static Mesh use for Caps, basically sphere"))
	UStaticMesh* CapsMesh = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Point", meta=(ToolTip="New Caps Material Instance"))
	UMaterialInterface* CableCapsMaterialInst = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Point", meta=(UIMin="0", ClampMin="0", ToolTip="Static Mesh scale multiplicator use for Caps (CableWeight * this)"))
	float CapsScaleMultiplicator = 0.0105;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope", meta=(ToolTip="Use cable shared settings to the start cable, like width, length, basically all settings exlcuding the ones that cannot be changed at runtime, like segments, and etc."))
	bool bCableUseSharedSettings = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope", meta=(UIMin="0", ClampMin="0", ToolTip="Cable error tolerance for wrapping, so there will be no duplicate points around already added ones, keep this low for smooth wrapping."))
	float CableWrapErrorTolerance = 0.002;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope", meta=(UIMin="0", ClampMin="0", ToolTip="Cable unwrap error multiplier, when trace finds the closest point, this value should be less than 'unwrap distance' for effective work."))
	float CableUnwrapErrorMultiplier = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope", meta=(UIMin="0", ClampMin="0", ToolTip="Cable unwrap trace distance, between start point and second cable point, this value should be around from 5 to 20 for effective work."))
	float CableUnwrapDistance = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope", meta=(UIMin="0", ClampMin="0", ToolTip="The delay alpha frames for start/end points, before unwrapping the cable points, to prevent flickering cycles of wrap/unwrap, this should be around 3-7 for effective work."))
	float CableUnwrapFirstFrameDelay = 4.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope", meta=(UIMin="0", ClampMin="0", ToolTip="The delay alpha frames for start/end points, before unwrapping the cable points, to prevent flickering cycles of wrap/unwrap, this should be around 3-7 for effective work."))
	float CableUnwrapLastFrameDelay = 3.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Rope", meta=(UIMin="0", ClampMin="0", ForceUnits="cm", ToolTip="Max distance from Cable was max tense"))
	float CableMaxTensDistance = 500.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Debug", meta=(ToolTip="Change New Cable Material color randomly"))
	bool bDebugMaterialColors = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Debug", meta=(ToolTip="New Cable Material Instance"))
	UMaterialInterface* CableDebugMaterialInst = nullptr;

	//Functions
	UFUNCTION()
	void CableWraping();
	
	UFUNCTION()
	void WrapCable();
	
	UFUNCTION()
	void UnwrapCableByFirst();

	UFUNCTION()
	void UnwrapCableByLast();

	UFUNCTION()
	FSCableWarpParams TraceCableWrap(const USceneComponent* cable, const bool bReverseLoc) const;

	UFUNCTION()
	void AddSphereCaps(const FSCableWarpParams& currentTraceParams);

	//Check if this location is not existing already in "cable points locations", error tolerance to determine how close another wrap point can be added
	UFUNCTION()
	bool CheckPointLocation(const FVector& targetLoc, const float& errorTolerance);
	
	UFUNCTION()
	void AdaptCableTens();
	
private:
	
	UFUNCTION()
	void AdaptFirstCableLocByAngle(UCableComponent* const attachCable);

#pragma endregion Rope

#pragma region Grapple
	//------------------

public:
	UFUNCTION()
	void OnSlowmoTriggerEventReceived(const bool bIsSlowed);
	
	UFUNCTION()
	void OnAttachWeapon();

	UFUNCTION()
	void OnInitWeaponEventReceived();
	
	UFUNCTION()
	void HookObject();
	
	UFUNCTION()
	void WindeHook();
	
	UFUNCTION()
	void StopWindeHook();
	
	UFUNCTION()
	void DettachHook();

	
	UFUNCTION(BlueprintPure)
	FORCEINLINE float GetForceWeight() const{return ForceWeight;}
	
	UFUNCTION(BlueprintPure)
	FORCEINLINE float GetMaxForceWeight() const{return MaxForceWeight;}

	UFUNCTION(BlueprintPure)
	FORCEINLINE bool IsObjectHooked() const{return bObjectHook;}
	
	FORCEINLINE UMeshComponent* GetAttachedMesh() const{return AttachedMesh;}

	UFUNCTION(BlueprintPure)
	FORCEINLINE bool IsCableWinderPull() const{return bCableWinderPull;}

protected:
	//Status
	UPROPERTY()
	UMeshComponent* AttachedMesh = nullptr;
	
	UPROPERTY(VisibleInstanceOnly, Category="Status|Hook")
	FHitResult CurrentHookHitResult;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Hook")
	float DefaultGravityScale = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Hook")
	float DefaultAirControl = 0.0f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Hook",  meta=(ForceUnits="cm",ToolTip="Distance between player and object grappled on attaching"))
	double DistanceOnAttach = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Hook",  meta=(ToolTip="Is currently hook an object"))
	bool bObjectHook = false;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Hook",  meta=(ToolTip="Is currently pull by Rope Tens"))
	bool bCablePowerPull = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Hook",  meta=(ToolTip="Is currently pull by Rope Winder effect"))
	bool bCableWinderPull = false;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Hook",  meta=(ToolTip="Winde start time"))
	float CableStartWindeTimestamp = TNumericLimits<float>::Min();
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Hook",  meta=(UIMin="0", ClampMin="0", ForceUnits="cm",ToolTip="Current Pull Force"))
	float ForceWeight = 1.0f;

	//Parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook",  meta=(UIMin="0", ClampMin="0", ForceUnits="cm",ToolTip="MaxDistance for HookObject Object"))
	float HookingMaxDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook",  meta=(UIMin="0", ClampMin="0", ForceUnits="cm",ToolTip="Min Offset apply to fistcable attahce dot Hookthrower mesh"))
	FVector MinCableHookOffset = FVector(1,0,0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook",  meta=(UIMin="0", ClampMin="0", ForceUnits="cm",ToolTip="Max Offset apply to RealFirstCable attahce dot Hookthrower mesh (cable angle from thrower > MaxAngleHookOffset"))
	FVector MaxCableHookOffset = FVector(4,0,0);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook",  meta=(UIMin="0", ClampMin="0", ForceUnits="deg",ToolTip="Max Offset angle apply to RealFirstCable attahce dot Hookthrower mesh"))
	float MaxAngleHookOffset = 90.0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook",  meta=(UIMin="0", ClampMin="0", ForceUnits="cm",ToolTip="Max Force Weight for Pulling object to Player"))
	float MaxForceWeight = 10000.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook",  meta=(UIMin="0", ClampMin="0", ForceUnits="cm",ToolTip="Distance for reach Max Force Weight by distance to object"))
	float MaxForcePullingDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook",  meta=(UIMin="0", ClampMin="0", ForceUnits="s",ToolTip="Duration for reach Max Force Winde Weight by winde holding"))
	float MaxWindePullingDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook",  meta=(UIMin="0", ClampMin="0", ForceUnits="s",ToolTip="Max Force Winde Weight curve"))
	UCurveFloat* WindePullingCurve;
	
	UFUNCTION()
	void PowerCablePull();	
	
private:
	//------------------

#pragma endregion Grapple

#pragma region Swing
	//------------------

public:
	UFUNCTION()
	void OnTriggerSwing(const bool bActivate);

	UFUNCTION()
	void OnSwingForce();

	UFUNCTION()
	void OnSwingPhysic();

	UFUNCTION()
	void ImpulseConstraintAttach() const;

	UFUNCTION()
	void ForceInvertSwingDirection() {_VelocityToAbsFwd = _VelocityToAbsFwd * -1;};

	FORCEINLINE bool IsPlayerSwinging() const{return bPlayerIsSwinging;}
	
	UFUNCTION()
	void OnParkourDetectorBeginOverlapEventReceived(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex, bool
		bFromSweep, const FHitResult& sweepResult);

protected:

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Hook|Swing",  meta=(ToolTip="Is player currently swinging"))
	bool bPlayerIsSwinging = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status|Hook|Swing",  meta=(ToolTip="Swing force multiplicator"))
	float SwingStartTimestamp = TNumericLimits<float>::Min();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status|Hook|Swing",  meta=(ToolTip="Swing input force alpha"))
    float SwingAlpha = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing",  meta=(ToolTip="Use physic constraint swing or force"))
	bool bSwingIsPhysical = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing",  meta=(ForceUnits="cm",ToolTip="Swing distance on the lower position of the trajectory"))
	float SwingMaxDistance = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing",  meta=(ForceUnits="sec",ToolTip="Swing force multiplicator"))
	float SwingMaxDuration = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing",  meta=(ToolTip="Swing force multiplicator"))
	float SwingMaxAirControl = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook|Swing",  meta=(ToolTip="Swing input weight divider"))
	float SwingInputScaleDivider = 2.0f;



private:
	UPROPERTY(Transient)
	FVector _SwingStartLoc;

	UPROPERTY(Transient)
	FVector _SwingStartFwd;
	
	UPROPERTY(Transient)
	bool _OrientationIsReset;

	UPROPERTY(Transient)
	float _VelocityToAbsFwd;

#pragma endregion Swing	
};

