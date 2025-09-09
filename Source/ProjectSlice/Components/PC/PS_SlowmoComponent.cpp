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
		const float alpha = UKismetMathLibrary::MapRangeClamped(_SlowmoTime, _StartSlowmoTimestamp ,_StartSlowmoTimestamp + SlowmoTransitionDuration,0.0,1.0);
	
		//Curve alpha
		float curveGlobalAlpha = alpha;
		if(IsValid(SlowmoGlobalDilationCurve))
		{
			curveGlobalAlpha = SlowmoGlobalDilationCurve->GetFloatValue(alpha);
		}

		float curvePlayerAlpha = alpha;
		if(IsValid(SlowmoPlayerDilationCurve))
		{
			curvePlayerAlpha = SlowmoPlayerDilationCurve->GetFloatValue(alpha);
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
		
		const float max = playerDilationTarget / globalTimeDilation;
		playerTimeDilation = FMath::Clamp(((globalTimeDilation > KINDA_SMALL_NUMBER) ? (playerTimeDilation / globalTimeDilation) : 1.0f), 0.0f, max);
		
		UGameplayStatics::SetGlobalTimeDilation(GetWorld(), globalTimeDilation);
		_PlayerCharacter->SetPlayerTimeDilation(playerTimeDilation);
		if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: alpha %f, alphaGlobal %f, globalDilation %f, customTimeDilation %f, playerTimeDilation %f "),  __FUNCTION__,alpha, alpha, UGameplayStatics::GetGlobalTimeDilation(GetWorld()), _PlayerCharacter->CustomTimeDilation, _PlayerCharacter->GetActorTimeDilation());

		//Exit or trigger Slowmo timer
		if(alpha >= 1.0f)
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

	//For each dilated actor update custom dilation
	if (!_bSlowmoActive)
	{
		for (AActor* actorDilated : _ActorsDilated)
		{
			UpdateObjectDilation(actorDilated);
		}
	}

	//Start player dilation &&& global dilation transition
	_bIsSlowmoTransiting = true;
	SetComponentTickEnabled(true);

	//Trigger FOV slowmo, duration is SlowmoTransitionDuration
	UPS_PlayerCameraComponent* playerCam = Cast<UPS_PlayerCameraComponent>(_PlayerCharacter->GetFirstPersonCameraComponent());
	UCurveFloat* slowmoCurve = SlowmoFOVCurves[_bSlowmoActive ? 0 : 1];
	if(!IsValid(slowmoCurve)) slowmoCurve = SlowmoFOVCurves[0];
		
	playerCam->SetupFOVInterp(_bSlowmoActive ? TargetFOV : playerCam->GetDefaultFOV(), _bSlowmoActive ? SlowmoTransitionDuration : SlowmoFOVResetDuration, slowmoCurve);

	OnSlowmoEvent.Broadcast(_bSlowmoActive);
	
	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: %s"), __FUNCTION__, _bSlowmoActive ? TEXT("On") :  TEXT("Off"));
}

void UPS_SlowmoComponent::UpdateObjectDilation(AActor* actorToUpdate)
{
	if (!IsValid(actorToUpdate) || actorToUpdate->IsActorBeingDestroyed()) return;
	
	float timeDilation = GetInteractedObjectTimeDilationTarget() / GetGlobalTimeDilationTarget();
	const float currentTargetDilation = _bSlowmoActive ? timeDilation : 1.0f;
	if (actorToUpdate->CustomTimeDilation == currentTargetDilation) return;

	//Apply dilation
	actorToUpdate->CustomTimeDilation = currentTargetDilation;

	//Debug
	if (bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: %s dilation: %f "),__FUNCTION__, *actorToUpdate->GetActorNameOrLabel(), currentTargetDilation);

	//Stock or Erase to Queue
	if (_bSlowmoActive)
	{
		_ActorsDilated.AddUnique(actorToUpdate);
	}
	else
	{
		_ActorsDilated.Remove(actorToUpdate);
	}
	
}




