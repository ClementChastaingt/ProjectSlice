// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PS_ComponentsManager.generated.h"


class AProjectSliceCharacter;

UENUM(BlueprintType)
enum class ECustomComponentType : uint8
{
	NONE = 0 UMETA(DisplayName = "None"),
	PARKOUR = 1 UMETA(DisplayName = "Parkour"),
	WEAPON = 2 UMETA(DisplayName = "Weapon"),
	HOOK = 3 UMETA(DisplayName = "Hook"),
	PROC_ANIM = 4 UMETA(DisplayName = "Procedural Anim"),
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTSLICE_API UPS_ComponentsManager : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPS_ComponentsManager();

	


protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parameters", meta=(ToolTip="Array who make link between BP instance of component and player instance"))
	TMap<ECustomComponentType,UObject*> ComponentArray;

private:
	UPROPERTY(Transient)
	AProjectSliceCharacter* _PlayerCharacter;
	
};
