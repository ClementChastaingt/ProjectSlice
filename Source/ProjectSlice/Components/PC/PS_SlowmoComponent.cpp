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

	DefaultPlayerTimeDilation = _PlayerCharacter->GetActorTimeDilation();
	if(IsValid(GetWorld()))
	{
		DefaultGlobalTimeDilation = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
	}



}

// Called every frame
void UPS_SlowmoComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if(!bIsSlowmoTransiting && IsComponentTickEnabled())
		SetComponentTickEnabled(false);
	
	SlowmoTransition();
}

void UPS_SlowmoComponent::SlowmoTransition()
{
	if(bIsSlowmoTransiting)
	{
		if(!IsValid(_PlayerCharacter) || !IsValid(GetWorld())) return;

		SlowmoTime = SlowmoTime + GetWorld()->DeltaRealTimeSeconds;

		//Time Dilation alpha
		const float alpha = UKismetMathLibrary::MapRangeClamped(SlowmoTime, StartSlowmoTimestamp ,StartSlowmoTimestamp + SlowmoTransitionDuration,0.0,1.0);
		float curveAlpha = alpha;
		if(IsValid(SlowmoCurve))
		{
			curveAlpha = SlowmoCurve->GetFloatValue(alpha);
		}

		//Set PostProccess alpha
		SlowmoAlpha = bSlowmoActive ? curveAlpha : 1.0f - curveAlpha;
		SlowmoPostProcessAlpha = SlowmoAlpha;
		UCurveFloat* currentCurve = SlowmoPostProcessCurves.IsValidIndex(bSlowmoActive ? 0 : 1) ? SlowmoPostProcessCurves[bSlowmoActive ? 0 : 1] : nullptr ;
		if(IsValid(currentCurve))
		{
			SlowmoPostProcessAlpha = currentCurve->GetFloatValue(SlowmoAlpha);
		}
	
		//Set time Dilation
		float globalDilationTarget = bSlowmoActive ? GlobalTimeDilationTarget : 1.0f;
		float playerDilationTarget = bSlowmoActive ? PlayerTimeDilationTarget : 1.0f;
		
		float globalTimeDilation = FMath::Lerp(StartGlobalTimeDilation,globalDilationTarget,curveAlpha);		
		float playerTimeDilation = FMath::Lerp(StartPlayerTimeDilation,playerDilationTarget,curveAlpha);

		UGameplayStatics::SetGlobalTimeDilation(GetWorld(), globalTimeDilation);
		_PlayerCharacter->SetPlayerTimeDilation(playerTimeDilation / globalTimeDilation);

		if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: alpha %f, globalDilation %f, customTimeDilation %f, playerTimeDilation %f "),  __FUNCTION__,alpha, UGameplayStatics::GetGlobalTimeDilation(GetWorld()), _PlayerCharacter->CustomTimeDilation, _PlayerCharacter->GetActorTimeDilation());

		//Exit or trigger Slowmo timer
		if(alpha >= 1.0f)
		{
			//Exit
			if(!bSlowmoActive)
			{
				OnStopSlowmo();
				return;
			}
			
			bIsSlowmoTransiting = false;
			
			FTimerDelegate stopSlowmo_TimerDelegate;
			stopSlowmo_TimerDelegate.BindUObject(this, &UPS_SlowmoComponent::OnTriggerSlowmo);
			GetWorld()->GetTimerManager().SetTimer(SlowmoTimerHandle, stopSlowmo_TimerDelegate, SlowmoDuration, false);
		}
	}

}

void UPS_SlowmoComponent::OnStopSlowmo()
{
	UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__);
	
	//Desactivate slowmo PP layer
	OnStopSlowmoEvent.Broadcast();

	//Reset next Tick for avoid blink effect
	FTimerDelegate resetSlowmo;
	resetSlowmo.BindUFunction(this, FName("SetIsSlowmoTransiting"), false);
	GetWorld()->GetTimerManager().SetTimerForNextTick(resetSlowmo);
	
}

void UPS_SlowmoComponent::OnTriggerSlowmo()
{
	if(!IsValid(GetWorld()) || bIsSlowmoTransiting)
		return;
	
	StartGlobalTimeDilation = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
	StartPlayerTimeDilation = _PlayerCharacter->CustomTimeDilation;
	
	StartSlowmoTimestamp = GetWorld()->GetAudioTimeSeconds();
	SlowmoTime = StartSlowmoTimestamp;

	bSlowmoActive = !bSlowmoActive;
	GetWorld()->GetTimerManager().ClearTimer(SlowmoTimerHandle);
	
	bIsSlowmoTransiting = true;
	SetComponentTickEnabled(true);

	//Trigger FOV slowmo, duration is SlowmoTransitionDuration
	UPS_PlayerCameraComponent* playerCam = Cast<UPS_PlayerCameraComponent>(_PlayerCharacter->GetFirstPersonCameraComponent());
	UCurveFloat* slowmoCurve = SlowmoFOVCurves[bSlowmoActive ? 0 : 1];
	if(!IsValid(slowmoCurve)) slowmoCurve = SlowmoFOVCurves[0];
		
	playerCam->SetupFOVInterp(bSlowmoActive ? TargetFOV : playerCam->GetDefaultFOV(), bSlowmoActive ? SlowmoTransitionDuration : SlowmoFOVResetDuration, slowmoCurve);

	OnSlowmoEvent.Broadcast(bSlowmoActive);
	
	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("%S :: %s"), __FUNCTION__, bSlowmoActive ? TEXT("On") :  TEXT("Off"));
	
}





