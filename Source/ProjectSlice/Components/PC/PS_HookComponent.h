// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PS_HookComponent.generated.h"


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

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_HookComponent : public USceneComponent
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, Category="Component", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* HookThrower = nullptr;
	
	UPROPERTY(VisibleAnywhere,  Category="Component", meta = (AllowPrivateAccess = "true"))
	UCableComponent* FirstCable= nullptr;
	
	// UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	// UStaticMeshComponent* HookMesh = nullptr;

public:
	/** Sets default values for this component's properties */
	UPS_HookComponent();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bDebug = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bDebugTick = false;
	
private:
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;
	
	UPROPERTY(Transient)
	APlayerController* _PlayerController;


#pragma region Grapple
	//------------------

public:
	UFUNCTION()
	void OnAttachWeapon();

	UFUNCTION()
	void OnInitWeaponEventReceived();

	UFUNCTION()
	void Grapple();

	UFUNCTION()
	void DettachGrapple();
	
	UMeshComponent* GetAttachedMesh() const{return AttachedMesh;}


protected:
	//Status
	UPROPERTY()
	UMeshComponent* AttachedMesh = nullptr;
	
	UPROPERTY(VisibleInstanceOnly, Category="Status|Hook")
	FHitResult CurrentHookHitResult;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status|Hook")
	double DistanceOnAttach = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Hook")
	bool bCablePowerPull = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|Hook")
	float ForceWeight = 1.0f;

	//Parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Hook")
	float HookingMaxDistance = 1000.0f;

	UFUNCTION()
	void PowerCablePull();


	
private:
	//------------------

#pragma endregion Grapple


#pragma region Rope
	//------------------

public:
	//------------------
protected:
	//Status
	UPROPERTY(VisibleAnywhere, Category="Status|Cable|Rope", meta=(ToolTip="start adding from first cable only if there is more than one cable. Basically the next cable should be added from end first, then we can start extending also from start, by inserting points between."))
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

	//Parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Point", meta=(ToolTip="Use cable sphere caps, basically spawns dynamic mesh points on corners with new cable points to make smoother looks"))
	bool bCanUseSphereCaps = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Point", meta=(ToolTip="Static Mesh use for Caps, basically sphere"))
	UStaticMesh* CapsMesh = nullptr;

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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Debug", meta=(ToolTip="Change New Cable Material color randomly"))
	bool bDebugMaterialColors = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Cable|Debug", meta=(ToolTip="New Cable Material Instance"))
	UMaterialInterface* CableDebugMaterialInst = nullptr;

	//Functions
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
	
private:
	//------------------

#pragma endregion Rope
};
