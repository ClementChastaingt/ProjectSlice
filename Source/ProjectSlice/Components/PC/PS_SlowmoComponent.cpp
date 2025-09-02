// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_SlowmoComponent.h"

#include "PS_PlayerCameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Character/PC/PS_Character.h"

// Sets default values for this component's properties
UPS_SlowmoComponent::UPS_SlowmoComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
	// ...
}

// Called when the game starts
void UPS_SlowmoComponent::BeginPlay()
{
	Super::BeginPlay();

	SetComponentTickEnabled(false);

	//Init default Variable
	_PlayerCharacter = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if(!IsValid(_PlayerCharacter)) return;
	
	_PlayerController = Cast<AProjectSlicePlayerController>(_PlayerCharacter->GetController());
	if(!IsValid(_PlayerController))return;

	_DefaultPlayerTimeDilation = _PlayerCharacter->GetActorTimeDilation();
	if(IsValid(GetWorld()))
	{
		_DefaultGlobalTimeDilation = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
	}



}

// Called every frame
void UPS_SlowmoComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if(!_bIsSlowmoTransiting && IsComponentTickEnabled())
		SetComponentTickEnabled(false);
	
	SlowmoTransition();
}

void UPS_SlowmoComponent::SlowmoTransition()
{
	if(_bIsSlowmoTransiting)
	{
		if(!IsValid(_PlayerCharacter) || !IsValid(GetWorld())) return;

		_SlowmoTime = _SlowmoTime + GetWorld()->DeltaRealTimeSeconds;

		//Time Dilation alpha
		const float alphaGlobal = UKismetMathLibrary::MapRangeClamped(_SlowmoTime, _StartSlowmoTimestamp ,_StartSlowmoTimestamp + SlowmoGlobalTransitionDuration,0.0,1.0);
		const float alphaPlayer = UKismetMathLibrary::MapRangeClamped(_SlowmoTime, _StartSlowmoTimestamp ,_StartSlowmoTimestamp + SlowmoPlayerTransitionDuration,0.0,1.0);
	
		//Curve alpha
		float curveGlobalAlpha = alphaGlobal;
		if(IsValid(SlowmoGlobalDilationCurve))
		{
			curveGlobalAlpha = SlowmoGlobalDilationCurve->GetFloatValue(alphaGlobal);
		}

		float curvePlayerAlpha = alphaPlayer;
		if(IsValid(SlowmoPlayerDilationCurve))
		{
			curvePlayerAlpha = SlowmoPlayerDilationCurve->GetFloatValue(alphaPlayer);
		}

		//Set PostProccess alpha
		_PlayerSlowmoAlpha = _bSlowmoActive ? curvePlayerAlpha : 1.0f - curvePlayerAlpha;
		_SlowmoPostProcessAlpha = _PlayerSlowmoAlpha;
		UCurveFloat* currentCurve = SlowmoPostProcessCurves.IsValidIndex(_bSlowmoActive ? 0 : 1) ? SlowmoPostProcessCurves[_bSlowmoActive ? 0 : 1] : nullptr ;
		if(IsValid(currentCurve))
		{
			_SlowmoPostProcessAlpha = currentCurve->GetFloatValue(_PlayerSlowmoAlpha);
		}
	
		//Set time Dilation
		float globalDilationTarget = _bSlowmoActive ? GlobalTimeDilationTarget : 1.0f;
		float playerDilationTarget = _bSlowmoActive ? PlayerTimeDilationTarget : 1.0f;
		
		float globalTimeDilation = FMath::Lerp(_StartGlobalTimeDilation,globalDilationTarget,curveGlobalAlpha);		
		float playerTimeDilation = FMath::Lerp(_StartPlayerTimeDilation,playerDilationTarget,curvePlayerAlpha);

		UGameplayStatics::SetGlobalTimeDilation(GetWorld(), globalTimeDilation);
		_PlayerCharacter->SetPlayerTimeDilation(playerTimeDilation * globalTimeDilation);
		//_PlayerCharacter->SetPlayerTimeDilation(globalTimeDilation == 0.0f ? playerTimeDilation : playerTimeDilation / globalTimeDilation);

		if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: alphaPlayer %f, alphaGlobal %f, globalDilation %f, customTimeDilation %f, playerTimeDilation %f "),  __FUNCTION__,alphaPlayer, alphaGlobal, UGameplayStatics::GetGlobalTimeDilation(GetWorld()), _PlayerCharacter->CustomTimeDilation, _PlayerCharacter->GetActorTimeDilation());

		//Exit or trigger Slowmo timer
		if(alphaPlayer >= 1.0f && alphaGlobal >= 1.0f)
		{
			//Exit if transit to FadeOut
			if(!_bSlowmoActive)
			{
				OnStopSlowmo();
				return;
			}

			//Else trigger timer for inactivation
			_bIsSlowmoTransiting = false;
			
			FTimerDelegate stopSlowmo_TimerDelegate;
			stopSlowmo_TimerDelegate.BindUObject(this, &UPS_SlowmoComponent::OnTriggerSlowmo);
			GetWorld()->GetTimerManager().SetTimer(_SlowmoTimerHandle, stopSlowmo_TimerDelegate, SlowmoDuration, false);
		}
	}
}

void UPS_SlowmoComponent::OnStopSlowmo()
{
	if (bDebug) UE_LOG(LogTemp, Log, TEXT("%S"), __FUNCTION__);
	
	//Desactivate slowmo PP layer
	OnStopSlowmoEvent.Broadcast();

	//Reset next Tick for avoid blink effect
	FTimerDelegate resetSlowmo;
	resetSlowmo.BindUFunction(this, FName("SetIsSlowmoTransiting"), false);
	GetWorld()->GetTimerManager().SetTimerForNextTick(resetSlowmo);
}

void UPS_SlowmoComponent::OnTriggerSlowmo()
{
	if(!IsValid(GetWorld()) /*|| bIsSlowmoTransiting*/)
		return;
	
	_StartGlobalTimeDilation = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
	_StartPlayerTimeDilation = _PlayerCharacter->CustomTimeDilation;
	
	_StartSlowmoTimestamp = GetWorld()->GetAudioTimeSeconds();
	_SlowmoTime = _StartSlowmoTimestamp;

	_bSlowmoActive = !_bSlowmoActive;
	GetWorld()->GetTimerManager().ClearTimer(_SlowmoTimerHandle);
	
	_bIsSlowmoTransiting = true;
	SetComponentTickEnabled(true);

	//Trigger FOV slowmo, duration is SlowmoTransitionDuration
	UPS_PlayerCameraComponent* playerCam = Cast<UPS_PlayerCameraComponent>(_PlayerCharacter->GetFirstPersonCameraComponent());
	UCurveFloat* slowmoCurve = SlowmoFOVCurves[_bSlowmoActive ? 0 : 1];
	if(!IsValid(slowmoCurve)) slowmoCurve = SlowmoFOVCurves[0];
		
	playerCam->SetupFOVInterp(_bSlowmoActive ? TargetFOV : playerCam->GetDefaultFOV(), _bSlowmoActive ? SlowmoGlobalTransitionDuration : SlowmoFOVResetDuration, slowmoCurve);

	OnSlowmoEvent.Broadcast(_bSlowmoActive);
	
	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: %s"), __FUNCTION__, _bSlowmoActive ? TEXT("On") :  TEXT("Off"));
}





