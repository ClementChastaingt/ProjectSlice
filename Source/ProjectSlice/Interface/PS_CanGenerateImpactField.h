// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Field/FieldSystemActor.h"
#include "UObject/Interface.h"
#include "PS_CanGenerateImpactField.generated.h"

class APS_FieldSystemActor;
// This class does not need to be modified.
UINTERFACE()
class UPS_CanGenerateImpactField : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class PROJECTSLICE_API IPS_CanGenerateImpactField
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
	
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="CanUseImpactField")
	APS_FieldSystemActor* GetImpactField() const;
	virtual APS_FieldSystemActor* GetImpactField_Implementation() const { return nullptr; }
	
	virtual TSubclassOf<AFieldSystemActor> GetFieldSystemClass() const = 0;
	
	virtual void GenerateImpactField(const FHitResult& targetHit, const FVector extent = FVector::Zero()){};

	virtual void ResetImpactField(const bool bForce = false);

	virtual void MoveImpactField(){};
	
	virtual void OnChaosFieldStartOverlapEventReceived(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult){};
	
	virtual void OnChaosFieldEndOverlapEventReceived(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex){};
	
private:
	FVector _FieldVelOrientation = FVector::ZeroVector;
	
};
