// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_PlayerCameraComponent.h"

#include "Kismet/KismetMaterialLibrary.h"
#include "ProjectSlice/PC/PS_Character.h"

UPS_PlayerCameraComponent::UPS_PlayerCameraComponent()
{
}

void UPS_PlayerCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	//Init default Variable
	_PlayerCharacter = Cast<AProjectSliceCharacter>(GetOwner());
	if(!IsValid(_PlayerCharacter)) return;
	
	_PlayerController = Cast<AProjectSlicePlayerController>(_PlayerCharacter->GetController());
	if(!IsValid(_PlayerController)) return;

	InitPostProcess();
}


void UPS_PlayerCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SlowmoTick();
	
}

#pragma region Post-Process
//------------------

void UPS_PlayerCameraComponent::CreatePostProcessMaterial(const UMaterialInterface* material, UPARAM(Ref) UMaterialInstanceDynamic*& outMatInst)
{
	if(IsValid(material))
	{		
		UMaterialInstanceDynamic* matInst  = UKismetMaterialLibrary::CreateDynamicMaterialInstance(GetWorld(), SlowmoMaterial);
		if(!IsValid(matInst)) return;

		outMatInst = matInst;
		
		FWeightedBlendable outWeightedBlendable = FWeightedBlendable(1.0f, matInst);
		WeightedBlendableArray.Add(outWeightedBlendable);

		if(bDebugPostProcess) UE_LOG(LogTemp, Warning, TEXT("%S :: outMatInst %s, BlendableArray %i"), __FUNCTION__, *outMatInst->GetName(), WeightedBlendableArray.Num());
	}
}

void UPS_PlayerCameraComponent::InitPostProcess()
{
	if(!IsValid(GetWorld())) return;
	
	CreatePostProcessMaterial(SlowmoMaterial, SlowmoMatInst);
	
	const FWeightedBlendables currentWeightedBlendables = FWeightedBlendables(WeightedBlendableArray);
	PostProcessSettings.WeightedBlendables = currentWeightedBlendables;
}

void UPS_PlayerCameraComponent::SlowmoTick()
{
	if(IsValid(_PlayerCharacter) && IsValid(_PlayerCharacter->GetSlowmoComponent()) && IsValid(SlowmoMatInst))
	{
		const float alpha = _PlayerCharacter->GetSlowmoComponent()->GetSlowmoAlpha();
		if(_PlayerCharacter->GetSlowmoComponent()->IsIsSlowmoTransiting())
		{
			SlowmoMatInst->SetScalarParameterValue(FName("DeltaTime"),alpha);
			SlowmoMatInst->SetScalarParameterValue(FName("Intensity"),alpha);    
		}
		else if(!_PlayerCharacter->GetSlowmoComponent()->IsSlowmoActive()) 
		{
			SlowmoMatInst->SetScalarParameterValue(FName("DeltaTime"),0.0f);
			SlowmoMatInst->SetScalarParameterValue(FName("Intensity"),0.0f);   
		}
	}	
}


//------------------
#pragma endregion Post-Process

