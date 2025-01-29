// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PS_ForceComponent.h"
#include "PS_HookComponent.h"
#include "PS_PlayerCameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ProjectSlice/Data/PS_Delegates.h"
#include "ProjectSlice/FunctionLibrary/PSCustomProcMeshLibrary.h"
#include "PS_WeaponComponent.generated.h"

class AProjectSlicePlayerController;
class UProceduralMeshComponent;
class AProjectSliceCharacter;

UCLASS(Blueprintable, BlueprintType, ClassGroup=(Component), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_WeaponComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* SightMesh = nullptr;

public:
	/** Sets default values for this component's properties */
	UPS_WeaponComponent();
	
	UFUNCTION(BlueprintCallable, Category="Weapon")
	FVector GetMuzzlePosition();
	
	/** Attaches the actor to a FirstPersonCharacter */
	UFUNCTION(BlueprintCallable, Category="Weapon")
	void AttachWeapon(AProjectSliceCharacter* TargetCharacter);

	UFUNCTION(BlueprintCallable, Category="Weapon")
	void InitWeapon(AProjectSliceCharacter* Target_PlayerCharacter);
	
	FOnPSDelegate OnWeaponInit;

protected:
	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	/** Ends gameplay for this component. */
	UFUNCTION()
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebug = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebugSlice = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebugSightRack = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|Shader")
	bool bDebugSightSliceBump = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug|Shader")
	bool bDebugSightShader = false;
	
private:
	/** The Character holding this weapon*/
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;

	UPROPERTY(Transient)
	AProjectSlicePlayerController* _PlayerController;

	UPROPERTY(Transient)
	UPS_PlayerCameraComponent* _PlayerCamera;

#pragma region Fire
	//------------------

public:

	UFUNCTION()
	void FireTriggered();

	/** Make the weapon Fire a Slice */
	UFUNCTION()
	void Fire();

protected:
	
	UPROPERTY(VisibleInstanceOnly, Category="Status")
	bool bIsHoldingFire = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Weapon", meta=(UIMin="0", ClampMin="0", ForceUnits="cm"))
	float MaxFireDistance = 5000.0f;
			
	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Weapon")
	USoundBase* FireSound;
	
	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Weapon")
	UAnimMontage* FireAnimation;

#pragma endregion Fire

#pragma region Slice
	//__________________________________________________

public:
	//__________________________________________________

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|Slice")
	bool ActivateImpulseOnSlice = true;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Slice")
	float MeltingLifeTime = 20.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Slice")
	UMaterialInterface* SliceableMaterial = nullptr;

	UFUNCTION()
	UMaterialInstanceDynamic* SetupMeltingMat(const UProceduralMeshComponent* const procMesh);

	UFUNCTION()
	void UpdateMeshTangents(UProceduralMeshComponent* const procMesh, const int32 sectionIndex);

	UFUNCTION()
	void UpdateMeltingParams(const UMaterialInstanceDynamic* sightedMatInst, UMaterialInstanceDynamic* matInstObject);

private:
	UPROPERTY(Transient)
	FSCustomSliceOutput _sliceOutput;

	UPROPERTY(Transient)
	TArray<UMaterialInstanceDynamic*> _HalfSectionMatInst;


#pragma endregion Slice

#pragma region SightRack
	//------------------
	
public:	
	UFUNCTION(BlueprintCallable)
	UStaticMeshComponent* GetSightMeshComponent() const{return SightMesh;}

	UFUNCTION(BlueprintCallable)
	FHitResult GetSightHitResult() const{return _SightHitResult;}

	UFUNCTION(BlueprintCallable)
	UPrimitiveComponent* GetCurrentSightedComponent() const{return _CurrentSightedComponent;}

	UFUNCTION(BlueprintCallable)
	int32 GetCurrentSightedFace() const{return _CurrentSightedFace;}

protected:
		
	/** Gun muzzle's offset from the characters location ::UNUSED:: */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight")
	FVector MuzzleOffset;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight")
	float MinSightRayMultiplicator = 0.1f;

private:
	
	UPROPERTY(Transient)
	bool _bSightMeshIsInUse;
	
	UPROPERTY(Transient)
	UPrimitiveComponent* _CurrentSightedComponent;

	UPROPERTY(Transient)
	FHitResult _SightHitResult;
	
	UPROPERTY(Transient)
	TArray<UMaterialInterface*> _CurrentSightedBaseMats;

	UPROPERTY(Transient)
	TArray<UMaterialInstanceDynamic*> _CurrentSightedMatInst;

	UPROPERTY(Transient)
	int32 _CurrentSightedFace;

#pragma region Rack
	//------------------

public:
	UFUNCTION()
	void TurnRack();

	UFUNCTION()
	void SetupTurnRackTargetting();

	UFUNCTION()
	void StopTurnRackTargetting();

	UFUNCTION()
	void TurnRackTarget();

	UFUNCTION(BlueprintCallable)
	FORCEINLINE FVector2D GetSightLookInput() const{return _SightLookInput;}

	UFUNCTION(BlueprintCallable)
	FORCEINLINE float GetTargetRackRoll() const{return TargetRackRotation.Roll;}

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Bool OnToggleTurnRackTargetEvent;
	
protected:
	UFUNCTION()
	void SightMeshRotation();

	UFUNCTION()
	void RackTick();
	
	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Mesh")
	bool bInterpRackRotation = false;

	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Mesh")
	float InterpRackRotStartTimestamp = TNumericLimits<float>().Lowest();
	
	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Mesh")
	FTransform RackDefaultRelativeTransform =  FTransform();
	
	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Mesh")
	FRotator StartRackRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Mesh")
	FRotator TargetRackRotation = FRotator::ZeroRotator;

	/** Rack Rotation interpolation params */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight|Move")
	float RackRotDuration = 0.5f;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight|Move")
	UCurveFloat* RackRotCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Component Tick", meta=(UIMin = 0.f, ClampMin = 0.f, ToolTip="Rack custom tick rate"))
	float RackTickRate = 0.05f;

private:
	UPROPERTY(Transient)
	FTimerHandle _RackTickTimerHandle;
	
	UPROPERTY(Transient)
	bool _bTurnRackTargetSetuped;

	UPROPERTY(Transient)
	float _LastAngleToInputTargetLoc;

	UPROPERTY(Transient)
	FVector2D _SightLookInput;

#pragma endregion Rack

#pragma region Ray_Rack
	//------------------

public:
	UFUNCTION()
	void AdaptSightMeshBound();

#pragma endregion Ray_Rack

#pragma region Shader
	//------------------

protected:
	/**Sight slice shader */
	UFUNCTION()
	void SightShaderTick();
	
	UFUNCTION()
	void ResetSightRackProperties();

	UFUNCTION()
	void ForceInitSliceBump();

	UFUNCTION()
	void SetupSliceBump();
	
	UFUNCTION()
	void SliceBump();
	
	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Mesh")
	bool bSliceBumping = false;
	
	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Shader")
	float StartSliceBumpTimestamp = TNumericLimits<float>().Min();

	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Shader")
	float CurrentCurveAlpha = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight|Shader")
	float SliceBumpDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight|Shader")
	UCurveFloat* SliceBumpCurve;

#pragma endregion Shader

#pragma endregion SightRack

#pragma region ForcePush
	//------------------
private:
	UPROPERTY(Transient)
	UPS_ForceComponent* _ForceComponent = nullptr;

#pragma endregion ForcePush
};
