// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_PlayerCameraComponent.h"

#include "Kismet/KismetMaterialLibrary.h"

UPS_PlayerCameraComponent::UPS_PlayerCameraComponent()
{
}

void UPS_PlayerCameraComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UPS_PlayerCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
}

#pragma region Post-Process
//------------------

void UPS_PlayerCameraComponent::CreatePostProcessMaterial(const UMaterialInterface* material,
	UMaterialInstanceDynamic* outMatInst, FWeightedBlendable& outWeightedBlendable) const
{
	if(IsValid(material))
	{
		UMaterialInstanceDynamic* matInst = UKismetMaterialLibrary::CreateDynamicMaterialInstance(GetWorld(), SlowmoMaterial);
		if(!IsValid(matInst)) return;

		outMatInst = matInst;
		outWeightedBlendable = FWeightedBlendable(1.0f, matInst);		
	}
}


void UPS_PlayerCameraComponent::InitPostProcess()
{
	if(!IsValid(GetWorld())) return;

	TArray<FWeightedBlendable> outWeightedBlendable
	CreatePostProcessMaterial(SlowmoMaterial, SlowmoMatInst, TODO)) return;

	
}

//------------------
#pragma endregion Post-Process

