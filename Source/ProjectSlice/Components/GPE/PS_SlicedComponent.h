// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "PS_SlicedComponent.generated.h"


UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_SlicedComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_SlicedComponent(const FObjectInitializer& objectInitializer);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;

#pragma region General
//------------------

public:
	UFUNCTION()
	void InitSliceObject();

	UFUNCTION()
	void InitComponent();
	
	//Getters && Setters
	UStaticMeshComponent* GetParentMesh() const{return _RootMesh;}

protected:
	UFUNCTION()
	void OnSlicedObjectHitEventReceived(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Feedback")
	USoundBase* CrashSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Feedback")
	USoundAttenuation* CrashSoundAttenuation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Feedback", meta=(Tooltip="Velocity Range to max volume multiplier"))
	FFloatInterval VelocityRangeSound = FFloatInterval(100.0f, 2000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Feedback", meta=(UIMin="0", ClampMin="0", Tooltip="Velocity Range to max volume multiplier"))
	float VolumeRangeMin = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters|Feedback", meta=(ForceUnits="cm/s", UIMin="1", ClampMin="1"))
	float MinVelocityZForFeedback = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
	bool bDebug = false;

private:
	UPROPERTY(Transient)
	UStaticMeshComponent* _RootMesh = nullptr;

	UPROPERTY(Transient)
	UAudioComponent* _FallingAudio;

	UPROPERTY(DuplicateTransient)
	float _LastImpactSoundTime = -1.0f;

	// 100ms between sounds
	UPROPERTY(DuplicateTransient)
	float _ImpactSoundCooldown = 0.1f;
	
//------------------	
#pragma endregion General

	
	
};
