// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_SlowmoComponent.h"

#include "PS_PlayerCameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Character/PC/PS_Character.h"
#include "ProjectSlice/FunctionLibrary/PSFl.h"

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
		_PlayerSlowmoAlpha = _bSlowming ? curvePlayerAlpha : 1.0f - curvePlayerAlpha;
		_SlowmoPostProcessAlpha = _PlayerSlowmoAlpha;
		UCurveFloat* currentCurve = SlowmoPostProcessCurves.IsValidIndex(_bSlowming ? 0 : 1) ? SlowmoPostProcessCurves[_bSlowming ? 0 : 1] : nullptr ;
		if(IsValid(currentCurve))
		{
			_SlowmoPostProcessAlpha = currentCurve->GetFloatValue(_PlayerSlowmoAlpha);
		}
	
		//Set time Dilation
		float globalDilationTarget = _bSlowming ? GlobalTimeDilationTarget : 1.0f;
		float playerDilationTarget = _bSlowming ? PlayerTimeDilationTarget : 1.0f;
		
		float globalTimeDilation = FMath::Lerp(_StartGlobalTimeDilation,globalDilationTarget,curveGlobalAlpha);		
		_CurrentPlayerTimeDilation = FMath::Lerp(_StartPlayerTimeDilation,playerDilationTarget,curvePlayerAlpha);
		
		const float max = playerDilationTarget / globalTimeDilation;
		_CurrentPlayerTimeDilation = FMath::Clamp(((globalTimeDilation > KINDA_SMALL_NUMBER) ? (_CurrentPlayerTimeDilation / globalTimeDilation) : 1.0f), 0.0f, max);
		
		UGameplayStatics::SetGlobalTimeDilation(GetWorld(), globalTimeDilation);
		_PlayerCharacter->SetPlayerTimeDilation(_CurrentPlayerTimeDilation);
		if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: alpha %f, alphaGlobal %f, globalDilation %f, customTimeDilation %f, playerTimeDilation %f "),  __FUNCTION__,alpha, alpha, UGameplayStatics::GetGlobalTimeDilation(GetWorld()), _PlayerCharacter->CustomTimeDilation, _PlayerCharacter->GetActorTimeDilation());

		//Exit or trigger Slowmo timer
		if(alpha >= 1.0f)
		{
			//Exit if transit to FadeOut
			if(!_bSlowming)
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

	_bSlowming = !_bSlowming;
	GetWorld()->GetTimerManager().ClearTimer(_SlowmoTimerHandle);

	//For each dilated actor update custom dilation
	const TArray<FSSlowmotionData> actorsArray = _ActorsDilated;
	if (!_bSlowming)
	{
		for (FSSlowmotionData actorDilated : actorsArray)
		{
			if (!IsValid(actorDilated.Actor)) continue;
			
			UpdateObjectDilation(actorDilated.Actor);
		}
	}

	//Start player dilation &&& global dilation transition
	_bIsSlowmoTransiting = true;
	SetComponentTickEnabled(true);

	//Trigger FOV slowmo, duration is SlowmoTransitionDuration
	UPS_PlayerCameraComponent* playerCam = Cast<UPS_PlayerCameraComponent>(_PlayerCharacter->GetFirstPersonCameraComponent());
	UCurveFloat* slowmoCurve = SlowmoFOVCurves[_bSlowming ? 0 : 1];
	if(!IsValid(slowmoCurve)) slowmoCurve = SlowmoFOVCurves[0];
		
	playerCam->SetupFOVInterp(_bSlowming ? TargetFOV : playerCam->GetDefaultFOV(), _bSlowming ? SlowmoTransitionDuration : SlowmoFOVResetDuration, slowmoCurve);

	OnSlowmoEvent.Broadcast(_bSlowming);
	
	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: %s"), __FUNCTION__, _bSlowming ? TEXT("On") :  TEXT("Off"));
}

void UPS_SlowmoComponent::UpdateObjectDilation(AActor* actorToUpdate)
{
	if (!IsValid(actorToUpdate) || actorToUpdate->IsActorBeingDestroyed()) return;

	//Get Index in Array list
	int32 currentIndex = _ActorsDilated.IndexOfByKey(actorToUpdate);
	if (!_ActorsDilated.IsValidIndex(currentIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("%S :: %s is not in _ActorsDilated array "),__FUNCTION__, *actorToUpdate->GetActorNameOrLabel());
		return;
	}

	//Work var
	const float timeDilation = InteractedObjectTimeDilationTarget * GetRealPlayerTimeDilationTarget();
	const float currentTargetDilation = _bSlowming ? timeDilation : 1.0f;
	if (actorToUpdate->CustomTimeDilation == currentTargetDilation) return;

	//Apply dilation
	actorToUpdate->CustomTimeDilation = currentTargetDilation;

	//Debug
	if (bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: %s dilation: %f "),__FUNCTION__, *actorToUpdate->GetActorNameOrLabel(), currentTargetDilation);

	//Stock or Erase to Queue
	if (_bSlowming)
	{
		//Set Slowmo data
		FSSlowmotionData newActorDilated;
		newActorDilated.Actor = actorToUpdate;
		newActorDilated.EnterVelocity = actorToUpdate->GetVelocity();

		//Add object to list
		_ActorsDilated.AddUnique(newActorDilated);
	}
	else
	{
		//Clamp Physic Vel of Dilated object who's currently moving on slowmo stop
		TArray<UPrimitiveComponent*> actorsToClampVel;
		UPSFl::GetActivePhysicsComponents(actorToUpdate, actorsToClampVel);
		for (UPrimitiveComponent* actorToClampVelElement : actorsToClampVel)
		{
			if (!IsValid(actorToClampVelElement)) continue;
				
			FVector newVel = UKismetMathLibrary::ClampVectorSize(actorToClampVelElement->GetComponentVelocity(), 0.0f, _ActorsDilated[currentIndex].EnterVelocity.Length());
			actorToClampVelElement->SetPhysicsLinearVelocity(newVel);
		}

		//Remove object from list
		_ActorsDilated.RemoveAt(currentIndex);
	}
}




