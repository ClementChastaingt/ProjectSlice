// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "PS_ParkourComponent.generated.h"


UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_ParkourComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_ParkourComponent();

	UPROPERTY(VisibleAnywhere, Category="Component", meta = (AllowPrivateAccess = "true"))
	UCapsuleComponent* ParkourDetector = nullptr;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bDebug = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bDebugTick = false;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;
	
	UPROPERTY(Transient)
	APlayerController* _PlayerController;

#pragma region General
	//------------------

protected:

	UFUNCTION()
	void OnParkourDetectorBeginOverlapEventReceived(UPrimitiveComponent onComponentBeginOverlap, UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex, bool bFromSweep, const FHitResult& sweepResult);
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status", meta=(ToolTip="Current Player camera instance"))
	UCameraComponent* PlayerCamera  = nullptr;

//------------------
#pragma endregion General

#pragma region WallRun
	//------------------

public:
	//------------------
protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status|WallRun", meta=(ToolTip="Is currently WallRunning"))
	bool bIsWallRunning = false;
	
private:
	//------------------

#pragma endregion WallRun
};


