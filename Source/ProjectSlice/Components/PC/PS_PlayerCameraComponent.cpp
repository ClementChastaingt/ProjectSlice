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
	CameraRollTilt();                           
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
                                                                                                                                                                                                                                                
void UPS_PlayerCameraComponent::SetupCameraTilt(const bool& bIsReset, const int32& targetOrientation)                                                                                                                                                    
{
	CurrentCameraTiltOrientation = targetOrientation;
	StartCameraRot = _PlayerController->GetControlRotation().Clamp();
	StartCameraTiltTimestamp = GetWorld()->GetTimeSeconds();
	
	_bIsResetingCameraTilt = bIsReset;
	LastAngleCamToWall = _PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector().Dot(_PlayerCharacter->GetParkourComponent()->GetWallRunDirection());
	_bIsCameraTiltingByInterp = true;
	
	if(bDebugCameraTilt)                                                                                                                                                                                                                        
		UE_LOG(LogTemp, Warning, TEXT("%S :: bIsReset %i,  StartCameraRot %s, CurrentCameraTiltOrientation %f"), __FUNCTION__, bIsReset, *StartCameraRot.ToString(), CurrentCameraTiltOrientation);               
                                                                                                                                                                                                                                        
}

void UPS_PlayerCameraComponent::ForceUpdateTargetTilt()
{
	if (bDebugCameraTilt) UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__);
	
	CurrentCameraTiltOrientation = CurrentCameraTiltOrientation * -1;
	
	StartCameraTiltTimestamp = GetWorld()->GetTimeSeconds();
	StartCameraRot = _PlayerController->GetControlRotation();

	_bIsCameraTiltingByInterp = false;

}

void UPS_PlayerCameraComponent::CameraRollTilt()                                                                                                                                                              
{
	if(!IsValid(_PlayerCharacter)
		|| !IsValid(_PlayerCharacter->GetFirstPersonCameraComponent())
		|| !IsValid(_PlayerCharacter->GetParkourComponent())
		|| !IsValid(GetWorld()) ) return;

	if(!_PlayerCharacter->GetParkourComponent()->IsWallRunning() && !_bIsResetingCameraTilt) return;
	
	//Determine start
	const FRotator currentRot = _PlayerController->GetControlRotation();
	const float angleCamToWall = _PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector().Dot(_PlayerCharacter->GetParkourComponent()->GetWallRunDirection());
	const bool bHaveToUpdateTiltOrientation = FMath::Sign(angleCamToWall) != FMath::Sign(LastAngleCamToWall);
	
	//Determine Target Roll
	if(bHaveToUpdateTiltOrientation)
		ForceUpdateTargetTilt();
	
	const float orientation = CurrentCameraTiltOrientation * FMath::Abs(angleCamToWall);
	const FRotator newRotTarget = _bIsResetingCameraTilt ? FRotator(StartCameraRot.Pitch, StartCameraRot.Yaw, DefaultCameraRot.Roll) : FRotator(DefaultCameraRot.Pitch,DefaultCameraRot.Yaw, 20.0 * orientation);

	if(bDebugCameraTiltTick) UE_LOG(LogTemp, Log, TEXT("angleCamToWall %f, LastAngleCamToWall %f, CurrentCameraTiltOrientation %f, orientation %f"), angleCamToWall,LastAngleCamToWall, CurrentCameraTiltOrientation, orientation);

	LastAngleCamToWall = angleCamToWall;
	
	if(!IsValid(_PlayerController) || !IsValid(GetWorld())) return;
	                                                                                                                                                                                                                                            
	//Alpha
	float targetTiltTime = StartCameraTiltTimestamp + (bHaveToUpdateTiltOrientation ? CameraTiltDuration / 2 : CameraTiltDuration);
	const float alphaTilt = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetTimeSeconds(), StartCameraTiltTimestamp, targetTiltTime, 0,1);                                                                                                                
	
	//Interp                                                                                                                                                                                                                                    
	float curveTiltAlpha = alphaTilt;                                                                                                                                                                                                           
	if(IsValid(CameraTiltCurve))                                                                                                                                                                                                                
		curveTiltAlpha = CameraTiltCurve->GetFloatValue(alphaTilt);                                                                                                                                                                             
	
	//New Tilt Rot
	const FRotator newRot = (_bIsCameraTiltingByInterp ? FMath::Lerp(StartCameraRot,newRotTarget, curveTiltAlpha) : newRotTarget);
	                                                                                                                                                                                                                                            
	//Rotate             
	_PlayerController->SetControlRotation(FRotator(currentRot.Pitch, currentRot.Yaw, newRot.Roll));                                                                                                                                                                                              
	if(bDebugCameraTiltTick) UE_LOG(LogTemp, Log, TEXT("UTZParkourComp :: newRot: %f, alphaTilt %f, CurrentCameraTiltOrientation %"),newRot.Roll, alphaTilt);

	//If Camera tilt already finished stop                                                                                                                                                                                                      
	if(alphaTilt >= 1)                                                                                                                                                                                                                          
	{
		_bIsCameraTilted = !_bIsResetingCameraTilt;
		_bIsCameraTiltingByInterp = false;
	}                                                                                                                                                                                                                                           
              
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

