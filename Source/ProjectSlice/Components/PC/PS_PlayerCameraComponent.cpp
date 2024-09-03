// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_PlayerCameraComponent.h"

#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetMathLibrary.h"
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

	DefaultFOV = FieldOfView;

	InitPostProcess();
}


void UPS_PlayerCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SlowmoTick();

	FieldOfViewTick();
}

#pragma region General
//------------------


void UPS_PlayerCameraComponent::SetupFOVInterp(const float targetFOV, const float duration, UCurveFloat* interCurve)
{
	if(targetFOV == FieldOfView) return;
	
	StartFOV = FieldOfView;
	TargetFOV = targetFOV;
	
	StartFOVInterpTimestamp = GetWorld()->GetAudioTimeSeconds();
	FOVIntertpDuration = duration;
	FOVIntertpCurve = interCurve;
	
	bFieldOfViewInterpChange = true;

	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("%S :: StartFOV:%f, TargetFOV:%f "), __FUNCTION__, StartFOV, TargetFOV);
}

void UPS_PlayerCameraComponent::FieldOfViewTick()
{
	if(bFieldOfViewInterpChange)
	{
		if(!IsValid(GetWorld())) return;
		
		const float alpha = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetAudioTimeSeconds(), StartFOVInterpTimestamp, StartFOVInterpTimestamp + FOVIntertpDuration, 0.0f, 1.0f);
		float curveAlpha = alpha;

		if(IsValid(FOVIntertpCurve))
		{
			curveAlpha = FOVIntertpCurve->GetFloatValue(alpha);
		}
		
		SetFieldOfView(FMath::Lerp(StartFOV,TargetFOV,curveAlpha));
		
		if(alpha >= 1.0f)
		{
			bFieldOfViewInterpChange = false;
			if(bDebug) UE_LOG(LogTemp, Warning, TEXT("StopFOVInterp"));
		}

	}
}


//------------------
#pragma endregion General



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
			SlowmoMatInst->SetScalarParameterValue(FName("DeltaBump"),_PlayerCharacter->GetSlowmoComponent()->GetSlowmoPostProcessAlpha());
			SlowmoMatInst->SetScalarParameterValue(FName("Intensity"),alpha);    
		}
		else if(!_PlayerCharacter->GetSlowmoComponent()->IsSlowmoActive()) 
		{
			SlowmoMatInst->SetScalarParameterValue(FName("DeltaTime"),0.0f);
			SlowmoMatInst->SetScalarParameterValue(FName("DeltaBump"),0.0f);
			SlowmoMatInst->SetScalarParameterValue(FName("Intensity"),0.0f);   
		}
	}	
}


//------------------
#pragma endregion Post-Process

