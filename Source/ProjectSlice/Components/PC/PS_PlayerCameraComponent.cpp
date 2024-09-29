// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_PlayerCameraComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
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
	DefaultCameraRot = _PlayerController->GetControlRotation().Clamp();

	if(IsValid(_PlayerCharacter->GetSlowmoComponent()))
	{
		_PlayerCharacter->GetSlowmoComponent()->OnStopSlowmoEvent.AddUniqueDynamic(this, &UPS_PlayerCameraComponent::OnStopSlowmoEventReceiver);
	}

	InitPostProcess();
}


void UPS_PlayerCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//-----Post-Process-----//                    
	SlowmoTick();
	DashTick();
	
	FieldOfViewTick();
	
	//-----CameraRollTilt smooth reset-----                                                                
	if(bIsResetingCameraTilt)
		CameraRollTilt(GetWorld()->GetTimeSeconds(), StartCameraTiltResetTimestamp);                           
}

#pragma region FOV
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
#pragma endregion FOV

#pragma region Camera_Tilt                                                                                                                                                                                                                      
//------------------                                                                                                                                                                                                                            
                                                                                                                                                                                                                                                
void UPS_PlayerCameraComponent::SetupCameraTilt(const bool& bIsReset, const ETiltUsage usage, const int32& targetOrientation)                                                                                                                                                    
{
	bIsResetingCameraTilt = bIsReset;
	CurrentCameraTiltOrientation = targetOrientation;
	CurrentUsageType = usage;
	
	StartCameraRot = _PlayerController->GetControlRotation().Clamp();
	TargetCameraRot =  bIsResetingCameraTilt ? FRotator(StartCameraRot.Pitch, StartCameraRot.Yaw, DefaultCameraRot.Roll) : StartCameraRot + (/*CameraTiltRotAmplitude.FindRef(usage))*/ FRotator(0.0,0.0,20.0) * CurrentCameraTiltOrientation);
	
	if(bIsResetingCameraTilt)                                                                                                                                                                                                                   
		StartCameraTiltResetTimestamp = GetWorld()->GetTimeSeconds();      
                                                                                                                                                                                                                                                
	if(bDebugCameraTilt)                                                                                                                                                                                                                        
		UE_LOG(LogTemp, Warning, TEXT("%S bIsReset %i, CameraTiltRotAmplitude %s,  StartCameraRot %s, TargetCameraRot %s"), __FUNCTION__, bIsReset, *CameraTiltRotAmplitude.FindRef(usage).ToString(), *StartCameraRot.ToString(),*TargetCameraRot.ToString());               
	
                                                                                                                                                                     ;                                                                                                                                                                                                          
                                                                                                                                                                                                                                        
}                                                                                                                                                                                                                                               
                                                                                                                                                                                                                                                
void UPS_PlayerCameraComponent::CameraRollTilt(float currentSeconds, const float startTime)                                                                                                                                                              
{                                                                                                                                                                                                                                               
	if(!IsValid(_PlayerController) || !IsValid(GetWorld())) return;                                                                                                                                                                             
	                                                                                                                                                                                                                                            
	//Alpha                                                                                                                                                                                                                                     
	const float alphaTilt = UKismetMathLibrary::MapRangeClamped(currentSeconds, startTime, startTime + CameraTiltDuration, 0,1);                                                                                                                
	                                                                                                                                                                                                                                            
	//If Camera tilt already finished stop                                                                                                                                                                                                      
	if(alphaTilt >= 1)                                                                                                                                                                                                                          
	{                                                                                                                                                                                                                                           
		bIsResetingCameraTilt = false;                                                                                                                                                                                                         
		return;                                                                                                                                                                                                                                 
	}                                                                                                                                                                                                                                           
                                                                                                                                                                                                                                                
	//Interp                                                                                                                                                                                                                                    
	float curveTiltAlpha = alphaTilt;                                                                                                                                                                                                           
	if(IsValid(CameraTiltCurve))                                                                                                                                                                                                                
		curveTiltAlpha = CameraTiltCurve->GetFloatValue(alphaTilt);                                                                                                                                                                             
	
	//Target Rot
	const FRotator newRotTarget = TargetCameraRot;                                                                                                               
	const FRotator newRot = FMath::Lerp(StartCameraRot,newRotTarget, curveTiltAlpha);
	                                                                                                                                                                                                                                            
	//Rotate             
	const FRotator currentRot = _PlayerController->GetControlRotation();                                                                                                                                                                                                                       
	_PlayerController->SetControlRotation(FRotator(currentRot.Pitch, currentRot.Yaw, newRot.Roll));                                                                                                                                                                                              
	if(bDebugCameraTilt) UE_LOG(LogTemp, Log, TEXT("UTZParkourComp :: CurrentRoll: %f, alphaTilt %f"), _PlayerController->GetControlRotation().Roll, alphaTilt);                                                                                
}                                                                                                                                                                                                                                               
                                                                                                                                                                                                                                                
//------------------                                                                                                                                                                                                                            
#pragma endregion Camera_Tilt                                                                                                                                                                                                                   

#pragma region Post-Process
//------------------

void UPS_PlayerCameraComponent::CreatePostProcessMaterial(UMaterialInterface* const material, UPARAM(Ref) UMaterialInstanceDynamic*& outMatInst)
{
	if(IsValid(material))
	{		
		UMaterialInstanceDynamic* matInst  = UKismetMaterialLibrary::CreateDynamicMaterialInstance(GetWorld(), material);
		if(!IsValid(matInst)) return;

		outMatInst = matInst;
		
		FWeightedBlendable outWeightedBlendable = FWeightedBlendable(0.0f, matInst);
		WeightedBlendableArray.Add(outWeightedBlendable);

		if(bDebugPostProcess) UE_LOG(LogTemp, Warning, TEXT("%S :: outMatInst %s, BlendableArray %i"), __FUNCTION__, *outMatInst->GetName(), WeightedBlendableArray.Num());
	}
}

void UPS_PlayerCameraComponent::InitPostProcess()
{
	if(!IsValid(GetWorld())) return;
	
	CreatePostProcessMaterial(SlowmoMaterial, SlowmoMatInst);
	CreatePostProcessMaterial(DashMaterial, DashMatInst);

	UpdateWeightedBlendPostProcess();
}

void UPS_PlayerCameraComponent::UpdateWeightedBlendPostProcess()
{
	const FWeightedBlendables currentWeightedBlendables = FWeightedBlendables(WeightedBlendableArray);
	PostProcessSettings.WeightedBlendables = currentWeightedBlendables;
}

#pragma region Slowmo
//------------------

void UPS_PlayerCameraComponent::SlowmoTick()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetSlowmoComponent())) return;
	
	const UPS_SlowmoComponent* slowmo = _PlayerCharacter->GetSlowmoComponent();
	
	if(IsValid(slowmo) && IsValid(SlowmoMatInst))
	{
		const float alpha = slowmo->GetSlowmoAlpha();
		if(slowmo->IsIsSlowmoTransiting())
		{
			if(WeightedBlendableArray.IsValidIndex(0) && WeightedBlendableArray[0].Weight != 1.0f)
			{
				WeightedBlendableArray[0].Weight = 1.0f;
				UpdateWeightedBlendPostProcess();
			}
			SlowmoMatInst->SetScalarParameterValue(FName("DeltaTime"),alpha);
			SlowmoMatInst->SetScalarParameterValue(FName("DeltaBump"),_PlayerCharacter->GetSlowmoComponent()->GetSlowmoPostProcessAlpha());
			SlowmoMatInst->SetScalarParameterValue(FName("Intensity"),alpha);    
		}
	}	
}

void UPS_PlayerCameraComponent::OnStopSlowmoEventReceiver()
{
	if(!IsValid(SlowmoMatInst)) return;

	if(WeightedBlendableArray.IsValidIndex(0))
	{
		WeightedBlendableArray[0].Weight = 0.0f;
		UpdateWeightedBlendPostProcess();
	}
	
	SlowmoMatInst->SetScalarParameterValue(FName("DeltaTime"),0.0f);
	SlowmoMatInst->SetScalarParameterValue(FName("DeltaBump"),0.0f);
	SlowmoMatInst->SetScalarParameterValue(FName("Intensity"),0.0f);    
}

void UPS_PlayerCameraComponent::DashTick() const
{
	if(DashTimerHandle.IsValid() && IsValid(DashMatInst))
	{
		const float alpha = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetAudioTimeSeconds(), _DashStartTimestamp, _DashStartTimestamp + DashDuration, 0.0f, 1.0f);
		DashMatInst->SetScalarParameterValue(FName("Density"), alpha * 5);
	}
}

//------------------
#pragma endregion Slowmo

#pragma region Dash
//------------------

void UPS_PlayerCameraComponent::OnTriggerDash(const bool bActivate)
{
	if(!IsValid(DashMatInst) || !IsValid(GetWorld())) return;

	//Blend PostProcess
	if(WeightedBlendableArray.IsValidIndex(1)) WeightedBlendableArray[1].Weight = bActivate ? 1.0f : 0.0f;
	UpdateWeightedBlendPostProcess();

	//Desactivation
	if(!GetWorld()->GetTimerManager().IsTimerActive(DashTimerHandle))
	{
		FTimerDelegate dash_TimerDelegate;
		dash_TimerDelegate.BindUObject(this, &UPS_PlayerCameraComponent::OnTriggerDash, false);
		GetWorld()->GetTimerManager().SetTimer(DashTimerHandle, dash_TimerDelegate, DashDuration, false);
	}
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(DashTimerHandle);
		return;
	}

	//Set mat params
	_DashStartTimestamp = GetWorld()->GetAudioTimeSeconds();
}

//------------------
#pragma endregion Dash

//------------------
#pragma endregion Post-Process

