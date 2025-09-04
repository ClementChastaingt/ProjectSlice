// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PS_ForceComponent.h"
#include "PS_HookComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "ProjectSlice/Data/PS_Delegates.h"
#include "ProjectSlice/Data/PS_GlobalType.h"

#include "ProjectSlice/FunctionLibrary/PSFL_CustomProcMesh.h"
#include "ProjectSlice/Interface/PS_CanGenerateImpactField.h"

#include "PS_WeaponComponent.generated.h"

using namespace UE::Geometry;

class UPS_PlayerCameraComponent;
class AProjectSlicePlayerController;
class UProceduralMeshComponent;
class AProjectSliceCharacter;


UCLASS(Blueprintable, BlueprintType, ClassGroup=(Component), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_WeaponComponent : public USkeletalMeshComponent, public IPS_CanGenerateImpactField
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleInstanceOnly, Category="Parameters|Component", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* SightMesh = nullptr;

public:
	/** Sets default values for this component's properties */
	UPS_WeaponComponent();
	
	UFUNCTION(BlueprintCallable, Category="Weapon")
	FVector GetMuzzlePosition();

	UFUNCTION(BlueprintCallable, Category="Weapon")
	FVector GetLaserPosition();
	
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
	bool bDebugChaos = false;

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

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate OnFireEvent;

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate OnFireTriggeredEvent;

protected:
	
	UPROPERTY(BlueprintReadOnly, Category="Status")
	bool bIsHoldingFire = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Weapon", meta=(UIMin="0", ClampMin="0", ForceUnits="cm"))
	float MaxFireDistance = 5000.0f;
			
	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Weapon")
	USoundBase* FireSound;
	
	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Weapon")
	UAnimMontage* FireAnimation;

	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Weapon")
	TSubclassOf<UPS_SlicedComponent> SlicedComponent;

#pragma endregion Fire

#pragma region Destruction
	//------------------

public:
	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Field OnSliceChaosFieldGeneratedEvent;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Parameters|Weapon")
	TSubclassOf<APS_FieldSystemActor> FieldSystemActor;

private:
	UPROPERTY(Transient)
	APS_FieldSystemActor* _ImpactField;
	
#pragma region IPS_CanGenerateImpactField
	//------------------

protected:
	UFUNCTION()
	virtual void GenerateImpactField(const FHitResult& targetHit, const FVector extent = FVector::Zero())override;

	virtual APS_FieldSystemActor* GetImpactField_Implementation() const override { return _ImpactField;};

	virtual TSubclassOf<APS_FieldSystemActor> GetFieldSystemClass() const override { return FieldSystemActor;};

#pragma endregion IPS_CanGenerateImpactField

	//------------------
#pragma endregion Destruction

#pragma region Slice
	//__________________________________________________

public:
	
	UFUNCTION(BlueprintCallable)
	FORCEINLINE FSCustomSliceOutput GetLastSliceOutput() const{return _LastSliceOutput;}

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
	FSCustomSliceOutput _LastSliceOutput;

	UPROPERTY(Transient)
	TArray<UMaterialInstanceDynamic*> _HalfSectionMatInst;


#pragma endregion Slice

#pragma region Sight
	//------------------
	
public:
	UFUNCTION(BlueprintCallable)
	FORCEINLINE UStaticMeshComponent* GetSightMeshComponent() const{return SightMesh;}

	UFUNCTION(BlueprintCallable)
	FORCEINLINE FHitResult GetSightHitResult() const{return _SightHitResult;}

	UFUNCTION(BlueprintCallable)
	FORCEINLINE FHitResult GetLaserHitResult() const{return _LaserHitResult;}

	UFUNCTION(BlueprintCallable)
	FORCEINLINE UPrimitiveComponent* GetCurrentSightedComponent() const{return _CurrentSightedComponent;}

	UFUNCTION(BlueprintCallable)
	FORCEINLINE int32 GetCurrentSightedFace() const{return _CurrentSightedFace;}

	UFUNCTION(BlueprintCallable)
	FORCEINLINE FVector GetSightStart() const{return _SightStart;}
	
	UFUNCTION(BlueprintCallable)
	FORCEINLINE FVector GetSightTarget() const{return _SightTarget;}

	UFUNCTION(BlueprintCallable)
	FORCEINLINE EPointedObjectType GetSightedObjectType() const{return _SightedObjectType;}

	UFUNCTION()
	void SightTick();


	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_ObjectType_Bool OnSwitchLaserMatEvent;
	

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
    FHitResult _LaserHitResult;
	
	UPROPERTY(Transient)
	TArray<UMaterialInterface*> _CurrentSightedBaseMats;

	UPROPERTY(Transient)
	TArray<UMaterialInstanceDynamic*> _CurrentSightedMatInst;

	UPROPERTY(Transient)
	int32 _CurrentSightedFace;

	UPROPERTY(Transient)
	FVector _SightStart;	

	UPROPERTY(Transient)
	FVector _SightTarget;
		
	UPROPERTY(Transient)
	FVector _LastSightTarget;

	UPROPERTY(Transient)
	EPointedObjectType _SightedObjectType;

#pragma region Laser
	//------------------

public:
	UFUNCTION(BlueprintCallable)
	FORCEINLINE FVector GetLaserTarget() const{return _LaserTarget;}

protected:
	UFUNCTION()
	void UpdateLaserColor();
	
private:
	UPROPERTY(Transient)
	FVector _LaserTarget;

	UPROPERTY(Transient)
	bool _bUseHookStartForLaser;

#pragma endregion Laser

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
	FORCEINLINE float GetTargetRackRoll() const{return _TargetRackRoll;}

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate_Bool OnToggleTurnRackTargetEvent;

	UPROPERTY(BlueprintAssignable)
	FOnPSDelegate OnTurnRackEvent;
	
protected:
	UFUNCTION()
	void SightMeshRotation();

	UFUNCTION()
	void RackTick();
	
	UPROPERTY(BlueprintReadOnly, Category="Status|Sight|Mesh")
	bool bInterpRackRotation = false;

	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Mesh")
	float InterpRackRotStartTimestamp = TNumericLimits<float>().Lowest();
	
	UPROPERTY(VisibleInstanceOnly, Category="Status|Sight|Mesh")
	FTransform RackDefaultRelativeTransform =  FTransform();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Sight|Move")
	bool bRackTargetingRotToggleSlowmo = false;

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
	
	UPROPERTY(Transient)
	float _StartRackRoll;

	UPROPERTY(Transient)
	float _TargetRackRoll;

#pragma endregion Rack

#pragma region Adaptation
	//------------------

public:
	UFUNCTION()
	void AdaptToProjectedHull(const FVector& MuzzleLoc, const FVector& ViewDir, UMeshComponent* meshComp);

	UFUNCTION()
	void AdaptSightMeshBound(const bool& bForce = false);

protected:
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bDebugRackBoundAdaptation = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|Sight|Adaptation")
	bool bDefaultAdaptationAimLaserTarget = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|Sight|Adaptation")
	FVector DefaultAdaptationScale = FVector(10.0f, 0.5f, 0.5f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters|Sight|Adaptation")
	FVector2D ProjectedAdaptationWeigth = FVector2D(1.0f, 1.0f);
	
#pragma endregion Adaptation

#pragma region Shader
	//------------------

protected:
	/**Sight slice shader */
	UFUNCTION()
	void UpdateSightRackShader();
	
	UFUNCTION()
	void ResetSightRackShaderProperties();

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

#pragma endregion Sight

#pragma region ForcePush
	//------------------
private:
	UPROPERTY(Transient)
	UPS_ForceComponent* _ForceComponent = nullptr;

#pragma endregion ForcePush
};
