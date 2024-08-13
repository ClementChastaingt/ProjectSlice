// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_SlowmoComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/PC/PS_Character.h"


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

}

// Called every frame
void UPS_SlowmoComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if(!bIsSlowmoTransiting && IsComponentTickEnabled())
		SetComponentTickEnabled(false);
	
	SlowmoTransition(DeltaTime);
}


void UPS_SlowmoComponent::SlowmoTransition(const float& DeltaTime)
{
	if(bIsSlowmoTransiting)
	{
		SlowmoTime = SlowmoTime + DeltaTime;
		if(!IsValid(_PlayerCharacter) || !IsValid(GetWorld())) return;

		const float alpha = UKismetMathLibrary::MapRangeClamped(SlowmoTime, StartSlowmoTimestamp ,StartSlowmoTimestamp + SlowmoTransitionDuration,0.0,1.0);
		float curveAlpha = alpha;
		if(IsValid(SlowmoCurve))
		{
			curveAlpha = SlowmoCurve->GetFloatValue(alpha);
		}
		SlowmoAlpha = bSlowmoActive ? curveAlpha : 1.0f - curveAlpha;
	
		float globalDilationTarget = bSlowmoActive ? GlobalTimeDilationTarget : 1.0f;
		float playerDilationTarget = bSlowmoActive ? PlayerTimeDilationTarget : 1.0f;
		
		float globalTimeDilation = FMath::Lerp(StartTimeDilation,globalDilationTarget,curveAlpha);		
		float playerTimeDilation = FMath::Lerp(StartTimeDilation,playerDilationTarget,curveAlpha);

		UGameplayStatics::SetGlobalTimeDilation(GetWorld(), globalTimeDilation);
		_PlayerCharacter->CustomTimeDilation = playerTimeDilation;

		UE_LOG(LogTemp, Log, TEXT("%S :: SlowmoTime %f, alpha %f, globalDilationTarget %f, playerTimeDilation %f "),  __FUNCTION__,SlowmoTime,alpha, globalDilationTarget, playerTimeDilation);

		if(alpha >= 1.0f)
		{
			bIsSlowmoTransiting = false;

			if(!bSlowmoActive) return;
			
			FTimerDelegate stopSlowmo_TimerDelegate;
			stopSlowmo_TimerDelegate.BindUObject(this, &UPS_SlowmoComponent::OnTriggerSlowmo);
			GetWorld()->GetTimerManager().SetTimer(SlowmoTimerHandle, stopSlowmo_TimerDelegate, SlowmoDuration, false);
		}
	}

}


void UPS_SlowmoComponent::OnTriggerSlowmo()
{
	if(!IsValid(GetWorld()))
		return;
	
	StartTimeDilation = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
	StartSlowmoTimestamp = GetWorld()->GetTimeSeconds();
	SlowmoTime = StartSlowmoTimestamp;

	bSlowmoActive = !bSlowmoActive;
	GetWorld()->GetTimerManager().ClearTimer(SlowmoTimerHandle);
	
	bIsSlowmoTransiting = true;
	SetComponentTickEnabled(true);

	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("%S :: %s"), __FUNCTION__, bSlowmoActive ? TEXT("On") :  TEXT("Off"));
	
}





