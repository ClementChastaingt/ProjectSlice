// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_PlayerCameraComponent.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "ProjectSlice/Character/PC/PS_Character.h"
#include "ProjectSlice/Data/PS_Constants.h"
#include "ProjectSlice/FunctionLibrary/PSFL_CameraShake.h"

UPS_PlayerCameraComponent::UPS_PlayerCameraComponent()
{
	SetWorldShakeOverrided(EScreenShakeType::IMPACT);
}

void UPS_PlayerCameraComponent::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SetWorldShakeOverrided(EScreenShakeType::IMPACT);
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
	_DefaultCameraRot = _PlayerController->GetControlRotation().Clamp();

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
	GlassesTick(DeltaTime);
	
	FieldOfViewTick();
	
	//-----CameraRollTilt smooth reset-----                                                                
	CameraRollTilt(DeltaTime);                           
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

#pragma region CameraShake
//------------------

void UPS_PlayerCameraComponent::ShakeCamera(const EScreenShakeType& shakeType, const float& scale, const FVector& epicenter)
{
	if(!IsValid(_PlayerController) || ShakesParams.IsEmpty()) return;

	//Get current shake params
	if (!ShakesParams.Contains(shakeType))
	{
		if (bDebugCameraShake) UE_LOG(LogTemp, Warning, TEXT("%S :: shakeType %s params doesn't exist, return"),__FUNCTION__, *UEnum::GetValueAsString(shakeType));
		return;
	}
	FSShakeParams currentShakesParams = *ShakesParams.Find(shakeType);

	//Find scale
	const float scaleClamped = FMath::Clamp(scale, 0.0f, 1.0f);

	//Validity checks
	if (!IsValid(currentShakesParams.CameraShake))
	{
		if (bDebugCameraShake) UE_LOG(LogTemp, Warning, TEXT("%S :: cameraShakeInst already exist OR currentShakesParams are Invalid, return"),__FUNCTION__);
		return;
	}

	//Start shake
	UCameraShakeBase* newShake = nullptr;
	if (currentShakesParams.bIsAWorldShake && !epicenter.IsZero())
	{
		UGameplayStatics::PlayWorldCameraShake(GetWorld(), currentShakesParams.CameraShake,
		epicenter, currentShakesParams.WorldShakeParams.InnerRadius, currentShakesParams.WorldShakeParams.OuterRadius,
		currentShakesParams.WorldShakeParams.Falloff, currentShakesParams.WorldShakeParams.bOrientShakeTowardsEpicenter);
	}
	else
	{
		newShake = _PlayerController->PlayerCameraManager->StartCameraShake(currentShakesParams.CameraShake, scaleClamped);
	}
	_CameraShakesInst.Add(shakeType, newShake);

	//Debug
	if (bDebugCameraShake) UE_LOG(LogTemp, Log, TEXT("%S :: CameraShaketype: %s, scale: %f, %s"), __FUNCTION__, *UEnum::GetValueAsString(shakeType), scaleClamped, *currentShakesParams.ToString());
}

void UPS_PlayerCameraComponent::StopCameraShake(const EScreenShakeType& shakeType,const bool& bImmediately)
{
	if(!IsValid(_PlayerController) || _CameraShakesInst.IsEmpty()) return;

	//Get current shake params
	if (!ShakesParams.Contains(shakeType))
	{
		if (bDebugCameraShake) UE_LOG(LogTemp, Warning, TEXT("%S :: shakeType %s params doesn't exist, return"),__FUNCTION__, *UEnum::GetValueAsString(shakeType));
		return;
	}

	// Vérification sécurisée pour _CameraShakesInst
	UCameraShakeBase** cameraShakeInstPtr = _CameraShakesInst.Find(shakeType);
	if (!cameraShakeInstPtr || !*cameraShakeInstPtr)
	{
		if (bDebugCameraShake) UE_LOG(LogTemp, Warning, TEXT("%S :: CameraShake instance not found for type: %s"), __FUNCTION__, *UEnum::GetValueAsString(shakeType));
		return;
	}
	UCameraShakeBase* cameraShakeInst = *cameraShakeInstPtr;
	if (!IsValid(cameraShakeInst)) return;
	
	_PlayerController->PlayerCameraManager->StopCameraShake(cameraShakeInst, bImmediately);

	//Debug
	if (bDebugCameraShake) UE_LOG(LogTemp, Log, TEXT("%S ::  %s :: CameraShaketype: %s, inst %s"), __FUNCTION__, bImmediately ? TEXT("Immediately") :  TEXT(""),
		*UEnum::GetValueAsString(shakeType), *GetNameSafe(cameraShakeInst));

	_CameraShakesInst.Remove(shakeType);
	
}

void UPS_PlayerCameraComponent::UpdateCameraShakeScale(const EScreenShakeType& shakeType, const float& scale)
{
	if(_CameraShakesInst.IsEmpty() || ShakesParams.IsEmpty()) return;

	//Get current shake params
	if (!ShakesParams.Contains(shakeType))
	{
		if (bDebugCameraShake) UE_LOG(LogTemp, Warning, TEXT("%S :: shakeType %s params doesn't exist, return"),__FUNCTION__, *UEnum::GetValueAsString(shakeType));
		return;
	}

	// Vérification sécurisée pour _CameraShakesInst
	UCameraShakeBase** cameraShakeInstPtr = _CameraShakesInst.Find(shakeType);
	if (!cameraShakeInstPtr || !*cameraShakeInstPtr)
	{
		if (bDebugCameraShake) UE_LOG(LogTemp, Warning, TEXT("%S :: CameraShake instance not found for type: %s"), __FUNCTION__, *UEnum::GetValueAsString(shakeType));
		return;
	}
	UCameraShakeBase* cameraShakeInst = *cameraShakeInstPtr;

	// Vérification sécurisée pour ShakesParams
	FSShakeParams* currentShakeParamsPtr = ShakesParams.Find(shakeType);
	if (!currentShakeParamsPtr)
	{
		if (bDebugCameraShake) UE_LOG(LogTemp, Warning, TEXT("%S :: ShakeParams not found for type: %s"), __FUNCTION__, *UEnum::GetValueAsString(shakeType));
		return;
	}
	
	if (!IsValid(cameraShakeInst)) return;
	cameraShakeInst->ShakeScale = FMath::Clamp(scale, 0.0f, 1.0f);

	if (bDebugCameraShake)
		UE_LOG(LogTemp, Log, TEXT("%S :: CameraShaketype: %s, inst %s, scale: %f"), __FUNCTION__, *UEnum::GetValueAsString(shakeType), *GetNameSafe(cameraShakeInst), cameraShakeInst->ShakeScale );

}

void UPS_PlayerCameraComponent::WorldShakeCamera(const EScreenShakeType& shakeType, const FVector& epicenter,
	const FSWorldShakeParams& worldShakeParams)
{
	if(!IsValid(_PlayerController) || ShakesParams.IsEmpty()) return;

	//Get current shake params
	if (!ShakesParams.Contains(shakeType))
	{
		if (bDebugCameraShake) UE_LOG(LogTemp, Warning, TEXT("%S :: shakeType %s params doesn't exist, return"),__FUNCTION__, *UEnum::GetValueAsString(shakeType));
		return;
	}
	
	FSShakeParams currentShakesParams = *ShakesParams.Find(shakeType);
	
	UGameplayStatics::PlayWorldCameraShake(GetWorld(), currentShakesParams.CameraShake,
		epicenter, worldShakeParams.InnerRadius, worldShakeParams.OuterRadius,
		worldShakeParams.Falloff, worldShakeParams.bOrientShakeTowardsEpicenter);

	if (bDebugCameraShake)
		UE_LOG(LogTemp, Log, TEXT("%S :: CameraShaketype: %s, %s"), __FUNCTION__, *UEnum::GetValueAsString(shakeType), *worldShakeParams.ToString());

}

void UPS_PlayerCameraComponent::SetWorldShakeOverrided(const EScreenShakeType& shakeType)
{
	if(ShakesParams.IsEmpty()) return;

	//Get current shake params
	if (!ShakesParams.Contains(shakeType))
	{
		UE_LOG(LogTemp, Warning, TEXT("%S :: shakeType %s params doesn't exist, return"),__FUNCTION__, *UEnum::GetValueAsString(shakeType));
		return;
	}
	
	 ShakesParams.Find(shakeType)->SetIsWorldShakeOverrided(true);
}

//------------------
#pragma endregion CameraShake

#pragma region Camera_Tilt                                                                                                                                                                                                                      
//------------------                                                                                                                                                                                                                            

bool UPS_PlayerCameraComponent::StartCameraTilt(const ETiltType& tiltType, const int32& targetOrientation)                                                                                                                                                    
{
	if (_bIsTilting && _CurrentTiltType == tiltType)
	{
		if (bDebugCameraTilt) UE_LOG(LogTemp, Warning, TEXT("%S :: tilting already"), __FUNCTION__); 
		return false;
	}

	if (!IsValid(_PlayerCharacter)
		|| !IsValid(_PlayerCharacter->GetFirstPersonCameraComponent())
		|| !IsValid(_PlayerCharacter->GetParkourComponent())) return false;

	//Setup work var
	_TargetTiltOrientation = targetOrientation;
	_StartCameraRot = _PlayerController->GetControlRotation().Clamp();
	_StartCameraTiltTimestamp = GetWorld()->GetTimeSeconds();
	_bIsResetingCameraTilt = false;

	//Setup roll map var
	_CurrentTiltType = tiltType;
	_CurrentCameraTiltRollParams = CameraTiltRollParams.Contains(_CurrentTiltType) ? *CameraTiltRollParams.Find(_CurrentTiltType) : FSCameraTiltParams();

	//Specifics setup
	_LastAngleCamToTarget = GetAngleCamToTarget();
	
	//Start interp
	_bIsTilting = true;

	//Debug
	if(bDebugCameraTilt)                                                                                                                                                                                                                        
		UE_LOG(LogTemp, Warning, TEXT("%S :: StartCameraRot %s, CurrentCameraTiltOrientation %f"), __FUNCTION__, *_StartCameraRot.ToString(), _TargetTiltOrientation);               

	//Exit
	return true;
}

void UPS_PlayerCameraComponent::StopCameraTilt(const ETiltType& tiltType)
{
	if (tiltType != _CurrentTiltType) return;
	
	if (bDebugCameraTilt) UE_LOG(LogTemp, Log, TEXT("%S"), __FUNCTION__);
	
	_bIsResetingCameraTilt = true;
	_StartCameraTiltTimestamp = GetWorld()->GetTimeSeconds();
	
	_bIsTilting = true;
}

void UPS_PlayerCameraComponent::ForceUpdateTargetTilt()
{
	if (bDebugCameraTilt) UE_LOG(LogTemp, Log, TEXT("%S"), __FUNCTION__);
	
	_TargetTiltOrientation = _TargetTiltOrientation * -1;
	
	_StartCameraTiltTimestamp = GetWorld()->GetTimeSeconds();
	_StartCameraRot = _PlayerController->GetControlRotation();

}

void UPS_PlayerCameraComponent::CameraRollTilt(const float& deltaTime)                                                                                                                                                              
{
	if(!IsValid(_PlayerCharacter)
		|| !IsValid(GetWorld())
		|| !CameraTiltRollParams.Contains(_CurrentTiltType)) return;

	if(!_bIsTilting) return;
	
	//Determine start
	const FRotator currentRot = _PlayerController->GetControlRotation();
	const float angleCamToTarget = GetAngleCamToTarget();
	const bool bHaveToUpdateTiltOrientation = FMath::Sign(angleCamToTarget) != FMath::Sign(_LastAngleCamToTarget);
	
	//Determine Target Roll
	if(bHaveToUpdateTiltOrientation)
		ForceUpdateTargetTilt();
	
	const float orientation = _TargetTiltOrientation * FMath::Abs(angleCamToTarget);
	const float rollTarget = _CurrentCameraTiltRollParams.CameraTilRollTarget;
	const FRotator newRotTarget = _bIsResetingCameraTilt ? FRotator(_StartCameraRot.Pitch, _StartCameraRot.Yaw, _DefaultCameraRot.Roll) : FRotator(_DefaultCameraRot.Pitch,_DefaultCameraRot.Yaw,rollTarget * orientation);
	_LastAngleCamToTarget = angleCamToTarget;
	
	if(!IsValid(_PlayerController) || !IsValid(GetWorld())) return;
	                                                                                                                                                                                                                                            
	//Alpha
	float targetTiltTime = _StartCameraTiltTimestamp + (bHaveToUpdateTiltOrientation ? _CurrentCameraTiltRollParams.CameraTiltDuration / 2 : _CurrentCameraTiltRollParams.CameraTiltDuration);
	const float alphaTilt = _CurrentCameraTiltRollParams.bUseDynTiltUpdating && !_bIsResetingCameraTilt? _AlphaTiltWeight : UKismetMathLibrary::MapRangeClamped(GetWorld()->GetTimeSeconds(), _StartCameraTiltTimestamp, targetTiltTime, 0,1);                                                                                                                
	
	//Interp                                                                                                                                                                                                                                    
	float curveTiltAlpha = alphaTilt;                                                                                                                                                                                                  
	if(IsValid(CameraTiltCurve))                                                                                                                                                                                                                
		curveTiltAlpha = CameraTiltCurve->GetFloatValue(alphaTilt);                                                                                                                                                                             
	
	//New Tilt Rot
	FRotator newRot =  FMath::Lerp(_StartCameraRot,newRotTarget, curveTiltAlpha);
	newRot = UKismetMathLibrary::RInterpTo(_CurrentCameraRot, newRot, deltaTime, CameraTiltSmoothingSpeed);
	_CurrentCameraRot = newRot;
	                                                                                                                                                                                                                                            
	//Rotate             
	_PlayerController->SetControlRotation(FRotator(currentRot.Pitch, currentRot.Yaw, _CurrentCameraRot.Roll));                                                                                                                                                                                              
	if(bDebugCameraTiltTick) UE_LOG(LogTemp, Log, TEXT("%S :: newRot: %f, alphaTilt %f, angleCamToTarget %f, LastAngleCamToTarget %f, CurrentCameraTiltOrientation %f, orientation %f"),__FUNCTION__, newRot.Roll, alphaTilt, angleCamToTarget, _LastAngleCamToTarget, _TargetTiltOrientation, orientation);

	//If Camera tilt already finished stop                                                                                                                                                                                                      
	if(alphaTilt >= 1)                                                                                                                                                                                                                          
	{
		OnCameraTiltMovementEnded();
	}                                                                                                                                                                                                                                           
              
}

void UPS_PlayerCameraComponent::OnCameraTiltMovementEnded()
{		
	if(_bIsResetingCameraTilt) _bIsTilting = false;

	if (bDebugCameraTilt) UE_LOG(LogTemp, Warning, TEXT("%S :: _bIsTilting %i, _bIsResetingCameraTilt %i"), __FUNCTION__, _bIsTilting, _bIsResetingCameraTilt  );

	//Reset work var
	if (!_bIsTilting)
	{
		_bIsResetingCameraTilt = false;
		_CurrentCameraTiltRollParams = FSCameraTiltParams();

		_TargetTiltOrientation = 0.0f;
		_StartCameraRot = FRotator::ZeroRotator;
		_StartCameraTiltTimestamp = TNumericLimits<float>::Lowest();
		_bIsResetingCameraTilt = false;
	}
	
}


void UPS_PlayerCameraComponent::UpdateRollTiltTarget(const float alpha, const int32& orientation, const float startRoll)
{
	if (!_bIsTilting)
	{
		return;
	}
	
	_AlphaTiltWeight =  alpha;
	_TargetTiltOrientation = orientation;
	
	if (bDebugCameraTilt) UE_LOG(LogTemp, Log, TEXT("%S :: _AlphaTiltWeight %f, orientation %i"), __FUNCTION__, _AlphaTiltWeight, orientation);
}

float UPS_PlayerCameraComponent::GetAngleCamToTarget()
{
	if (!IsValid(_PlayerCharacter)
		|| !IsValid(_PlayerCharacter->GetFirstPersonCameraComponent())
		|| !IsValid(_PlayerCharacter->GetParkourComponent())) return 0.0f;
	
	float angleCamToTarget = 0.0f;
	switch (_CurrentTiltType)
	{
		case ETiltType::WALLRUN:
			angleCamToTarget = _PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector().Dot(_PlayerCharacter->GetParkourComponent()->GetWallRunDirection());
			break;
		case ETiltType::SLIDE:
			angleCamToTarget = _PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector().Dot(_PlayerCharacter->GetParkourComponent()->GetSteeredSlideDirection().GetSafeNormal());
			break;
	}
	
	return angleCamToTarget;
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
		_WeightedBlendableArray.Add(outWeightedBlendable);

		if(bDebugPostProcess) UE_LOG(LogTemp, Warning, TEXT("%S :: outMatInst %s, BlendableArray %i"), __FUNCTION__, *outMatInst->GetName(), _WeightedBlendableArray.Num());
	}
}

void UPS_PlayerCameraComponent::InitPostProcess()
{
	if(!IsValid(GetWorld())) return;

	//Glasses
	CreatePostProcessMaterial(FishEyeMaterial, _FishEyeMatInst);
	CreatePostProcessMaterial(GlassesMaterial, _GlassesMatInst);
	CreatePostProcessMaterial(VignetteMaterial, _VignetteMatInst);
	CreatePostProcessMaterial(OutlineMaterial, _OutlineMatInst);

	//Slowmo
	CreatePostProcessMaterial(SlowmoMaterial, _SlowmoMatInst);

	//Dash
	CreatePostProcessMaterial(DashMaterial, _DashMatInst);

	//Depth
	CreatePostProcessMaterial(DepthMaterial, _DepthMatInst);
	if(_WeightedBlendableArray.IsValidIndex(6)) _WeightedBlendableArray[6].Weight = 1.0f;

	//Update postprocess visibility
	UpdateWeightedBlendPostProcess();

	//Setup PP default var
	_DefaultPostProcessSettings = _PlayerCharacter->GetFirstPersonCameraComponent()->PostProcessSettings;
}

void UPS_PlayerCameraComponent::UpdateWeightedBlendPostProcess()
{
	const FWeightedBlendables currentWeightedBlendables = FWeightedBlendables(_WeightedBlendableArray);
	PostProcessSettings.WeightedBlendables = currentWeightedBlendables;
}

#pragma region Slowmo
//------------------

void UPS_PlayerCameraComponent::SlowmoTick()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetSlowmoComponent())) return;
	
	const UPS_SlowmoComponent* slowmo = _PlayerCharacter->GetSlowmoComponent();
	
	if(IsValid(slowmo) && IsValid(_SlowmoMatInst))
	{
		const float alpha = slowmo->GetPlayerSlowmoAlpha();
		if(slowmo->IsIsSlowmoTransiting())
		{
			if(_WeightedBlendableArray.IsValidIndex(4) && _WeightedBlendableArray[4].Weight != 1.0f)
			{
				_WeightedBlendableArray[4].Weight = 1.0f;
				UpdateWeightedBlendPostProcess();
			}
			_SlowmoMatInst->SetScalarParameterValue(FName("DeltaTime"),alpha);
			_SlowmoMatInst->SetScalarParameterValue(FName("DeltaBump"),_PlayerCharacter->GetSlowmoComponent()->GetSlowmoPostProcessAlpha());
			_SlowmoMatInst->SetScalarParameterValue(FName("Intensity"),alpha);    
		}
	}	
}

void UPS_PlayerCameraComponent::OnStopSlowmoEventReceiver()
{
	if(!IsValid(_SlowmoMatInst)) return;

	if(_WeightedBlendableArray.IsValidIndex(4))
	{
		_WeightedBlendableArray[4].Weight = 0.0f;
		UpdateWeightedBlendPostProcess();
	}
	
	_SlowmoMatInst->SetScalarParameterValue(FName("DeltaTime"),0.0f);
	_SlowmoMatInst->SetScalarParameterValue(FName("DeltaBump"),0.0f);
	_SlowmoMatInst->SetScalarParameterValue(FName("Intensity"),0.0f);
}

void UPS_PlayerCameraComponent::DashTick() const
{
	if(_DashTimerHandle.IsValid() && IsValid(_DashMatInst))
	{
		const float alpha = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetAudioTimeSeconds(), _DashStartTimestamp, _DashStartTimestamp + DashDuration, 0.0f, 1.0f);
		_DashMatInst->SetScalarParameterValue(FName("Density"), alpha * 5);
	}
}

//------------------
#pragma endregion Slowmo

#pragma region Dash
//------------------

void UPS_PlayerCameraComponent::TriggerDash(const bool bActivate)
{
	if(!IsValid(_DashMatInst) || !IsValid(GetWorld())) return;

	//Blend PostProcess
	if(_WeightedBlendableArray.IsValidIndex(5)) _WeightedBlendableArray[5].Weight = bActivate ? 1.0f : 0.0f;
	UpdateWeightedBlendPostProcess();

	//Desactivation
	if(!GetWorld()->GetTimerManager().IsTimerActive(_DashTimerHandle))
	{
		FTimerDelegate dash_TimerDelegate;
		dash_TimerDelegate.BindUObject(this, &UPS_PlayerCameraComponent::TriggerDash, false);
		GetWorld()->GetTimerManager().SetTimer(_DashTimerHandle, dash_TimerDelegate, DashDuration, false);
	}
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(_DashTimerHandle);
		return;
	}

	//Set mat params
	_DashStartTimestamp = GetWorld()->GetAudioTimeSeconds();
}


//------------------
#pragma endregion Dash

#pragma region Glasses
//------------------


void UPS_PlayerCameraComponent::TriggerGlasses(const bool bActivate, const bool bBlendPostProcess)
{
	if(_bIsUsingGlasses == bActivate) return;
	
	if(!IsValid(_GlassesMatInst) || !IsValid(_FishEyeMatInst) || !IsValid(_VignetteMatInst) || !IsValid(GetWorld())) return;

	//Get PostProcess settings
	_PlayerCharacter->GetFirstPersonCameraComponent()->PostProcessSettings = bActivate ? GlassesPostProcessSettings : GetDefaultPostProcessSettings();
	
	//Config camera
	// UPS_WeaponComponent* weaponComp = _PlayerCharacter->GetWeaponComponent();
	// if(bActivate && IsValid(weaponComp))
	// {
	// 	int32 sectionIndex;
	// 	//UMaterialInterface* targetFacedMat = weaponComp->GetCurrentSightedComponent()->GetMaterialFromCollisionFaceIndex(weaponComp->GetCurrentSightedFace(),sectionIndex)
	// }

	//Callback
	if(_bIsUsingGlasses != bActivate)
	{
		//Start with no delay
		if(bActivate)
		{
			//Start Fish Eye && Vignette
			if(_WeightedBlendableArray.IsValidIndex(0)) _WeightedBlendableArray[0].Weight = 1.0f;
			if(_WeightedBlendableArray.IsValidIndex(2)) _WeightedBlendableArray[2].Weight = 1.0f;		
			UpdateWeightedBlendPostProcess();
			
			//Start WBP_Glasses
			OnTriggerGlasses.Broadcast(true);
			//GetWorld()->GetTimerManager().ClearTimer(_ShakeTimerHandle);
		}
		
		//Launch CameraShake
		if (bActivate) ShakeCamera(EScreenShakeType::GLASSES, 1.0f);
		else StopCameraShake(EScreenShakeType::GLASSES);

		//Launch post process blend if needed
		if(bBlendPostProcess)
			_bIsUsingGlasses = bActivate;
		
	}
	
	//Setup var
	_CamRot = GetComponentRotation();	
	
	//Start GlassesTick
	_bGlassesIsActive = true;

}

void UPS_PlayerCameraComponent::DisplayOutlineOnSightedComp(const bool bRenderCustomDepth)
{
	if(_bGlassesRenderCustomDepth == bRenderCustomDepth) return;
	
	//Activate Outline Post-Process on sighted element
	UPrimitiveComponent* sightedComp = _PlayerCharacter->GetWeaponComponent()->GetCurrentSightedComponent();
	
	if(IsValid(sightedComp))
	{
		sightedComp->SetRenderCustomDepth(bRenderCustomDepth);
		_bGlassesRenderCustomDepth = bRenderCustomDepth;
	}
	
}

void UPS_PlayerCameraComponent::OnStopGlasses()
{
	_bGlassesIsActive = false;

	//Stop completly PostProcess
	if(_WeightedBlendableArray.IsValidIndex(0)) _WeightedBlendableArray[0].Weight = 0.0f;
	if(_WeightedBlendableArray.IsValidIndex(1)) _WeightedBlendableArray[1].Weight = 0.0f;
	if(_WeightedBlendableArray.IsValidIndex(2)) _WeightedBlendableArray[2].Weight = 0.0f;
	if(_WeightedBlendableArray.IsValidIndex(3)) _WeightedBlendableArray[3].Weight = 0.0f;

	UpdateWeightedBlendPostProcess();


	//Stop WBP_Glasses after Transition delay 
	OnTriggerGlasses.Broadcast(false);
	//GetWorld()->GetTimerManager().ClearTimer(_ShakeTimerHandle);
		
}

void UPS_PlayerCameraComponent::GlassesTick(const float deltaTime)
{
	if(!IsValid(_PlayerCharacter) || !IsValid(_FishEyeMatInst) || !IsValid(_VignetteMatInst)  || !IsValid(_GlassesMatInst) || !IsValid(GetWorld()) || !_bGlassesIsActive) return;
	
	//Blend PostProcess
	if(_bIsUsingGlasses ? _BlendWeight < 1.0f  : _BlendWeight > 0.0f)
	{
	   _BlendWeight = UKismetMathLibrary::FInterpTo(_BlendWeight,_bIsUsingGlasses,deltaTime,GlassesTransitionSpeed);

		//Blend Master Material Glasses
		if(_WeightedBlendableArray.IsValidIndex(1)) _WeightedBlendableArray[1].Weight = _BlendWeight;
		if(_WeightedBlendableArray.IsValidIndex(3)) _WeightedBlendableArray[3].Weight = _BlendWeight;
		UpdateWeightedBlendPostProcess();

		//Callback for Blur transition on WBP_Glasses
		OnTransitToGlasses.Broadcast(_BlendWeight);

		//Transit FishEye
		_FishEyeMatInst->SetScalarParameterValue(FName("Density"), FMath::Lerp(0.0f, 0.65f,_BlendWeight));
		_FishEyeMatInst->SetScalarParameterValue(FName("Radius"), _BlendWeight);

		//Transit Vignette
		_VignetteMatInst->SetScalarParameterValue(FName("Radius"), FMath::Lerp(0.77f, 0.67f,_BlendWeight));
		_VignetteMatInst->SetScalarParameterValue(FName("Hardness"), FMath::Lerp(1.0f, 0.9f,_BlendWeight));
		
		//Transit LCD
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), GlassesMatCollec,FName("LCD_FlickerHighBound"), FMath::Lerp(0.0f, 12.0f ,_BlendWeight));
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), GlassesMatCollec, FName("LCD_UvMulti"), FMath::Lerp(1024.0f, 256.0f ,_BlendWeight));

		//Transit Sharpen
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), GlassesMatCollec,FName("Sharpen_Intensity"), FMath::Lerp(0.0f, 2.5f ,_BlendWeight));

		//Transit FilmGrain
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), FilmGrainMatCollec, FName("alpha"), _BlendWeight);
	}
	//Stop Glasses
	else if(!_bIsUsingGlasses && _bGlassesIsActive)
	{
		OnStopGlasses();
		return;
	}

	//Setup move variables
	FRotator animRot = GetComponentRotation();
	FRotator newRot;

	//If stop moving try to return to 0.0 offset
	if(animRot.Equals(_CamRot))
		VignetteAnimParams.Rate = FVector2D::ZeroVector;

	
	//Pitch (Up//Down)
	newRot.Pitch = animRot.Pitch - _CamRot.Pitch;
	newRot.Pitch = newRot.Pitch + VignetteAnimParams.Rate.X;
	VignetteAnimParams.Rate.X = FMath::Clamp(newRot.Pitch, VignetteAnimParams.RateMinMax.X *- 1, VignetteAnimParams.RateMinMax.X);

	//Yaw (Left//Right)
	newRot.Yaw = animRot.Yaw - _CamRot.Yaw;
	newRot.Yaw = newRot.Yaw + VignetteAnimParams.Rate.Y;
	VignetteAnimParams.Rate.Y = FMath::Clamp(newRot.Yaw, VignetteAnimParams.RateMinMax.Y *- 1, VignetteAnimParams.RateMinMax.Y);
	
	//Set CamRot
	_CamRot = GetComponentRotation();

	//Interp offset
	VignetteAnimParams.VignetteOffset.X = UKismetMathLibrary::FInterpTo(VignetteAnimParams.VignetteOffset.X, VignetteAnimParams.Rate.X, deltaTime, VignetteAnimParams.AnimSpeed);
	VignetteAnimParams.VignetteOffset.Y = UKismetMathLibrary::FInterpTo(VignetteAnimParams.VignetteOffset.Y, VignetteAnimParams.Rate.Y, deltaTime, VignetteAnimParams.AnimSpeed);
	
	//Set offset into material inst
	//X = (Up//Down) // Y = (Left//Right)
	_VignetteMatInst->SetVectorParameterValue(FName("VignetteOffset"), FVector(VignetteAnimParams.VignetteOffset.Y ,VignetteAnimParams.VignetteOffset.X,0.0));

	//Switch camera shake (Idle or Walk)
	const float playerVel = _PlayerCharacter->GetVelocity().Length();
	const float alphaAmplitude = UKismetMathLibrary::MapRangeClamped(playerVel, 0.0f, _PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed, 0.1f, 1.0f);
	UpdateCameraShakeScale(EScreenShakeType::GLASSES, alphaAmplitude);
	
	//Update FilmGrain animation
	if(FilmGrainMatCollec->IsValidLowLevel())
	{
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), FilmGrainMatCollec, FILMGRAIN_R, FMath::RandRange(0.0f, 1.0f));
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), FilmGrainMatCollec, FILMGRAIN_G, FMath::RandRange(0.0f ,1.0f));
	}
	
}


//------------------

#pragma endregion Glasses


//------------------
#pragma endregion Post-Process

