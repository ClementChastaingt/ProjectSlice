// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ProceduralAnimComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Character/PC/PS_Character.h"
#include "ProjectSlice/Components/PC/PS_PlayerCameraComponent.h"
#include "ProjectSlice/Data/PS_GlobalType.h"

//TODO :: Harmonize for futur common usage with NME_Character

// Sets default values for this component's properties
UPS_ProceduralAnimComponent::UPS_ProceduralAnimComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
	// ...
}


// Called when the game starts
void UPS_ProceduralAnimComponent::BeginPlay()
{
	Super::BeginPlay();

	//Init default Variable
	if(!IsValid(GetWorld())) return;
	
	_PlayerCharacter = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if(!IsValid(_PlayerCharacter)) return;
	
	_PlayerController = Cast<AProjectSlicePlayerController>(_PlayerCharacter->GetController());
	if(!IsValid(_PlayerController))return;

	//Setup comp var
	if(IsValid(_PlayerCharacter->GetParkourComponent()))
		_ParkourComponent = _PlayerCharacter->GetParkourComponent();
	
	if(IsValid(_PlayerCharacter->GetForceComponent()))
		_ForceComponent = _PlayerCharacter->GetForceComponent();
	
	//Setup Callback
	if(IsValid(_ParkourComponent))
	{
		_ParkourComponent->OnDashEvent.AddUniqueDynamic(this, &UPS_ProceduralAnimComponent::DashDip);
	}
	if(IsValid(_ForceComponent))
	{
		_ForceComponent->OnPushEvent.AddUniqueDynamic(this, &UPS_ProceduralAnimComponent::OnPushEventReceived);
	}
		
}

void UPS_ProceduralAnimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	//Unbind delegate
	if(IsValid(_ParkourComponent))
	{
		_ParkourComponent->OnDashEvent.RemoveDynamic(this, &UPS_ProceduralAnimComponent::DashDip);
	}
}

// Called every frame
void UPS_ProceduralAnimComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	Dip();
	
	ApplyScrewMovement();
	
	HandShake(DeltaTime);
}

#pragma region Dip
//------------------

void UPS_ProceduralAnimComponent::StartDip(const float speed, const float strenght)
{
	if(!IsValid(GetWorld()) || bIsDipping) return;

	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__);
	
	DipStrenght = strenght;
	DipSpeed = speed;
	DipStartTimestamp = GetWorld()->GetTimeSeconds();

	bIsDipping = true;
	
}

void UPS_ProceduralAnimComponent::LandingDip()
{
	if(!IsValid(_PlayerCharacter)) return;

	const float lastUpdateVel = UKismetMathLibrary::Vector4_Size(FVector(0,0,_PlayerCharacter->GetCharacterMovement()->GetLastUpdateVelocity().Z));
	const float dipStrenght = FMath::Clamp(UKismetMathLibrary::NormalizeToRange(lastUpdateVel, 0.0f, _PlayerCharacter->GetCharacterMovement()->JumpZVelocity),0.0f,1.0f);
	
	StartDip(3.0f, dipStrenght);
}

void UPS_ProceduralAnimComponent::DashDip()
{
	if(!IsValid(_PlayerCharacter)) return;
	
}

void UPS_ProceduralAnimComponent::Dip()
{
	if(!bIsDipping || !IsValid(_PlayerCharacter)) return;

	//Calculate dip alpha
	const float alpha = FMath::Clamp(((GetWorld()->GetTimeSeconds() - DipStartTimestamp) / DipDuration) * DipSpeed, 0, 1.0f);
	
	float curveForceAlpha = alpha;
	if(IsValid(DipCurve))
		curveForceAlpha = DipCurve->GetFloatValue(alpha);

	DipAlpha = curveForceAlpha * DipStrenght;
	if(bDebugDip) UE_LOG(LogTemp, Log, TEXT("%S :: dipAlpha %f, alpha %f"), __FUNCTION__, DipAlpha, alpha);

	//Move player loc
	USceneComponent* playerRoot = _PlayerCharacter->GetFirstPersonRoot();	
	FVector newCamLoc = playerRoot->GetRelativeLocation();
	
	newCamLoc.Z = FMath::Lerp(0,-10, DipAlpha);
	playerRoot->SetRelativeLocation(newCamLoc);
	
	//Stop dip
	if(alpha >= 1.0f)
	{
		bIsDipping = false;
		DipAlpha = 0.0f;
		if(bDebug) UE_LOG(LogTemp, Log, TEXT("UPS_ProceduralAnimComponent :: Stop Dip"));
	}		
}

//------------------
#pragma endregion Dip

#pragma region Walking
//------------------

void UPS_ProceduralAnimComponent::SetLagPositionAndAirTilt()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(GetWorld()))
		return;

	FVector LagVector;
	LagVector.X = UKismetMathLibrary::SafeDivide(_PlayerCharacter->GetVelocity().Dot(_PlayerCharacter->GetActorRightVector()),  _PlayerCharacter->GetCharacterMovement()->GetMaxSpeed());
	LagVector.Y =  UKismetMathLibrary::SafeDivide(_PlayerCharacter->GetVelocity().Dot(_PlayerCharacter->GetActorForwardVector()) , _PlayerCharacter->GetCharacterMovement()->GetMaxSpeed() * -1);
	LagVector.Z =  UKismetMathLibrary::SafeDivide(_PlayerCharacter->GetVelocity().Dot(_PlayerCharacter->GetActorUpVector()) , _PlayerCharacter->GetCharacterMovement()->JumpZVelocity * -1);

	const float deltaTime = GetWorld()->GetDeltaSeconds();
	LocationLagPosition = UKismetMathLibrary::VInterpTo(LocationLagPosition, UKismetMathLibrary::ClampVectorSize(LagVector * 2, 0.0f, 4.0f), deltaTime, (1 / deltaTime) / VelocityLagSmoothingSpeed);

	//In Air Animation
	if(!_ParkourComponent->IsWallRunning())
	{
		InAirTilt = UKismetMathLibrary::RInterpTo(InAirTilt, FRotator(0.0f, LocationLagPosition.Z * -2.0f,0.0f), deltaTime, (1 / deltaTime) / AirTiltLagSmoothingSpeed);
		InAirOffset = UKismetMathLibrary::VInterpTo(InAirOffset,FVector(LocationLagPosition.Z * 0.5f, 0.0f, 0.0f), deltaTime, (1 / deltaTime) / AirTiltLagSmoothingSpeed);
	}
	
}

void UPS_ProceduralAnimComponent::StartWalkingAnim()
{
	OnStartWalkFeedbackEvent.Broadcast();
}

void UPS_ProceduralAnimComponent::StartWalkingAnimWithDelay(const float delay)
{
	FTimerHandle startWalkingAnimHandler;
	FTimerDelegate startWalkingAnim_TimerDelegate;
	startWalkingAnim_TimerDelegate.BindUObject(this, &UPS_ProceduralAnimComponent::StartWalkingAnim);
	GetWorld()->GetTimerManager().SetTimer(startWalkingAnimHandler, startWalkingAnim_TimerDelegate, delay, false);
}

void UPS_ProceduralAnimComponent::StopWalkingAnim()
{
	OnStopWalkFeedbackEvent.Broadcast();
}

void UPS_ProceduralAnimComponent::Walking(const float& leftRightAlpha, const float& upDownAlpha,  const float& rollAlpha)
{
	if(!IsValid(_PlayerCharacter)) return;
	
	//Left/Right && Up/down
	const float newWalkAnimPosX = FMath::Lerp(WalkingLeftRightOffest * -1,WalkingLeftRightOffest, leftRightAlpha);
	const float newWalkAnimPosZ = FMath::Lerp(WalkingDownOffest,WalkingUpOffest, upDownAlpha);
	WalkAnimPos = FVector(newWalkAnimPosX, 0.0f, newWalkAnimPosZ);

	//Roll rot
	FRotator startWalkAnimRot = WalkAnimRot;
	FRotator targetWalkAnimRot = WalkAnimRot;
	startWalkAnimRot.Pitch = 1;
	targetWalkAnimRot.Pitch = -1;
	WalkAnimRot = UKismetMathLibrary::RLerp(startWalkAnimRot,targetWalkAnimRot, rollAlpha, true);
	
	//Find WalkAnim Alpha
	const UCharacterMovementComponent* playerMovementComp = _PlayerCharacter->GetCharacterMovement();
	//TODO :: Do a var when custom mode in place for tweak in BP the proc walk move forbidden state 
	const bool bIsWalkProcAnimDesactive = (playerMovementComp->MovementMode == MOVE_Falling && !_ParkourComponent->IsWallRunning())
	|| playerMovementComp->IsCrouching()
	|| _ParkourComponent->IsDashing()
	|| playerMovementComp->MovementMode == MOVE_None;
		
	WalkAnimAlpha = (bIsWalkProcAnimDesactive ? 0.0f : UKismetMathLibrary::NormalizeToRange(_PlayerCharacter->GetVelocity().Length() / _PlayerCharacter->CustomTimeDilation, 0.0f, playerMovementComp->GetMaxSpeed()));
	WalkingSpeed = FMath::Lerp(0.0f,WalkingMaxSpeed, WalkAnimAlpha);
	
	
}

//------------------
#pragma endregion Walking

#pragma region Sway
//------------------

void UPS_ProceduralAnimComponent::ApplyLookSwayAndOffset(const FRotator& camRotPrev)
{
	if(!IsValid(_PlayerCharacter) || !IsValid(GetWorld()))
		return;

	//Determine Offset ViewModel based on Camera Pitch
	const float alphaPitchOffset = UKismetMathLibrary::NormalizeToRange(UKismetMathLibrary::NormalizedDeltaRotator(_PlayerCharacter->GetControlRotation(), _PlayerCharacter->GetActorRotation()).Pitch, -90.0f,90.0f);
	PitchOffsetPos = FVector(0.0f, FMath::Lerp(MaxPitchOffset.Y, -MaxPitchOffset.Y, alphaPitchOffset), FMath::Lerp(MaxPitchOffset.Z,-MaxPitchOffset.Z,alphaPitchOffset));
	
	const float alphaRelativeLoc = FMath::Clamp(UKismetMathLibrary::NormalizeToRange(alphaPitchOffset, 0.0f, 0.5f), 0.0, 1.0);

	FVector newPlayerPos = _PlayerCharacter->GetFirstPersonRoot()->GetRelativeLocation();
	newPlayerPos.X = FMath::Lerp(MaxPitchOffset.X, 0.0f, alphaRelativeLoc);
	_PlayerCharacter->GetFirstPersonRoot()->SetRelativeLocation(newPlayerPos);

	//Rotation rate and smoothing for Camera Sway
	CurrentCamRot =_PlayerCharacter->GetFirstPersonCameraComponent()->GetComponentRotation();
	
	FRotator newCamRotNorm = UKismetMathLibrary::NormalizedDeltaRotator(CurrentCamRot, camRotPrev);
	FRotator targetCamRotRate;
	targetCamRotRate.Roll = FMath::Clamp(newCamRotNorm.Pitch * -1, -5.0f,5.0f);
	targetCamRotRate.Pitch = 0.0f;
	targetCamRotRate.Yaw = FMath::Clamp(newCamRotNorm.Yaw, -5.0f,5.0f);

	const float deltaTime = GetWorld()->GetDeltaSeconds();
	
	const float alpha = FMath::Clamp(FMath::Abs(_PlayerController->GetMoveInput().Y * -1), 0.0f, 1.0f);
	const float camRotRateSpeedGun = FMath::Lerp(SwayLagSmoothingSpeedGun, SwayLagSmoothingSpeedGun * 2, alpha);
	const float camRotRateSpeedHook = FMath::Lerp(SwayLagSmoothingSpeedHook, SwayLagSmoothingSpeedHook * 2, alpha);
	
	const float rotAlphaGun = (1.0/deltaTime) / camRotRateSpeedGun;
	CamRotRateGun = (FMath::RInterpTo(CamRotRateGun, targetCamRotRate, deltaTime, rotAlphaGun));
	
	const float rotAlphaHook = (1.0/deltaTime) / camRotRateSpeedHook;
	CamRotRateHook = (FMath::RInterpTo(CamRotRateHook, targetCamRotRate, deltaTime, rotAlphaHook));

	//Counteract weapon sway rotation
	CamRotOffsetGun.X =  FMath::Lerp(-MaxCamRotOffset.X, MaxCamRotOffset.X,UKismetMathLibrary::NormalizeToRange(CamRotRateGun.Yaw, -5.0f,5.0f));
	CamRotOffsetGun.Y = 0.0f;
	CamRotOffsetGun.Z = FMath::Lerp(-MaxCamRotOffset.Z, MaxCamRotOffset.Z,UKismetMathLibrary::NormalizeToRange(CamRotRateGun.Roll, -5.0f,5.0f));

	CamRotOffsetHook.X =  FMath::Lerp(-MaxCamRotOffset.X, MaxCamRotOffset.X,UKismetMathLibrary::NormalizeToRange(CamRotRateHook.Yaw, -5.0f,5.0f));
	CamRotOffsetHook.Y = 0.0f;
	CamRotOffsetHook.Z = FMath::Lerp(-MaxCamRotOffset.Z, MaxCamRotOffset.Z,UKismetMathLibrary::NormalizeToRange(CamRotRateHook.Roll, -5.0f,5.0f));
		
}

//------------------
#pragma endregion Sway

#pragma region Hook
//------------------

void UPS_ProceduralAnimComponent::ApplyWindingVibration(const float alpha)
{
	if(!IsValid(_PlayerCharacter) || !IsValid(GetWorld()))
		return;
		
	const float maxHookOffset = UKismetMathLibrary::MapRangeClamped(alpha, 0.0f, 1.0f, 0.0, HookLocMaxOffset);
	HookLocOffset = FVector(0.0f, FMath::RandRange(-maxHookOffset, maxHookOffset), FMath::RandRange(-maxHookOffset, maxHookOffset));

	//UE_LOG(LogTemp, Error, TEXT("HookLocOffset %f"), HookLocOffset.Length());
}

#pragma endregion Hook

#pragma region Screw
//------------------

void UPS_ProceduralAnimComponent::OnPushEventReceived(bool bLoading)
{
	if(bLoading)
		StartScrewMovement();
	else
		StartResetScrewMovement();
}

void UPS_ProceduralAnimComponent::StartScrewMovement()
{
	if(!IsValid(_ForceComponent)) return;
	
	_ScrewLoadMoveDuration = _ForceComponent->GetMaxPushForceTime() / 4.0f;

	_bMoveScrew = true;
	_bIsReseting = false;

	//Reset reset starting position
	_LastScrewLocOffset = FVector::ZeroVector;
	_LastScrewRotOffset = FRotator::ZeroRotator;
}

void UPS_ProceduralAnimComponent::StartResetScrewMovement()
{
	_bMoveScrew = true;
	_bIsReseting = true;

	//Get starting position
	_LastScrewLocOffset = ScrewLocOffset;
	_LastScrewRotOffset = ScrewRotOffset;
	
}

void UPS_ProceduralAnimComponent::ApplyScrewMovement()
{
	if(!IsValid(_ForceComponent) || !IsValid(GetWorld())) return;

	if(!_bMoveScrew) return;

	//Alpha
	const float alpha = _bIsReseting ?
		UKismetMathLibrary::MapRangeClamped(GetWorld()->GetAudioTimeSeconds(), _ForceComponent->GetReleasePushTimestamp(),
			_ForceComponent->GetReleasePushTimestamp() + ScrewResetMoveDuration, 0.0f, 1.0f) :  _ForceComponent->GetInputTimeWeightAlpha();
	_ScrewLocAlpha = alpha;
	float curveRotAlpha = alpha;
	
	if(IsValid(ScrewLocOffsetCurve))
	{
		_ScrewLocAlpha = ScrewLocOffsetCurve->GetFloatValue(alpha);
	}
	if(IsValid(ScrewRotOffsetCurve))
	{
		curveRotAlpha = ScrewRotOffsetCurve->GetFloatValue(alpha);
	}
	
	//Loc Offset
	float startLoc = _bIsReseting ? _LastScrewLocOffset.Y : ScrewLocYOffsetRange.Min;
	float targetLoc = _bIsReseting ?  ScrewLocYOffsetRange.Min : ScrewLocYOffsetRange.Max;
	ScrewLocOffset = FVector::ZeroVector;
	ScrewLocOffset.Y = FMath::Lerp(startLoc,targetLoc, _ScrewLocAlpha);

	//Rot Offset
	float startRot = _bIsReseting ? _LastScrewRotOffset.Pitch : 0.0f;
	float targetRot = _bIsReseting ? 0.0f : 360.0f *  FMath::Lerp(1.0f, ScrewRotOffsetMultiplicator, curveRotAlpha);
	ScrewRotOffset = FRotator::ZeroRotator;
	ScrewRotOffset.Pitch = FMath::Lerp(startRot, targetRot, FMath::Sin(curveRotAlpha));

	if(bDebugForcePush)UE_LOG(LogTemp, Log, TEXT("%S :: bIsReseting %i, alpha %f, ScrewLocOffset %s, ScrewRotOffset %s"),__FUNCTION__, _bIsReseting, alpha, *ScrewLocOffset.ToString(), *ScrewRotOffset.ToString());

	//Callback
	OnScrewMoveUpdate.Broadcast();
	
	//Stop movement
	if(alpha >= 1.0f && _bIsReseting)
	{
		OnScrewResetEnd.Broadcast();
		_bMoveScrew = false;
	}

}

void UPS_ProceduralAnimComponent::HandShake(const float deltaTime)
{
	//Exit
	if(!_ForceComponent->IsPushLoading())
	{
		HandRotOffset = FRotator::ZeroRotator;
		_HandShakeTime = 0.0f;
		return;
	}

	//Curved alpha input
	float alphaFrequency = _ForceComponent->GetInputTimeWeightAlpha();
	
	// Time-based oscillation
	/*f (GetScrewLocAlpha() < 1.0f) */_HandShakeTime =_HandShakeTime + deltaTime;
	float frequency = FMath::TruncToInt(FMath::Lerp(ShakeFrequency.Min,ShakeFrequency.Max,alphaFrequency));

	// Remap sin from [-1,1] to [0,1]
	float alphaSin = (FMath::Sin(_HandShakeTime * frequency) + 1.0f) * 0.5f;
	
	//Random
	const FRotator rangeRot = FMath::Lerp(MinRotRange,MaxRotRange,alphaFrequency);
	const FVector rangeLoc = FMath::Lerp(MinLocRange,MaxLocRange,alphaFrequency);
	
	// Interpolate between two positions
	FRotator StartRot = rangeRot.GetInverse();
	FRotator EndRot = rangeRot;
	HandRotOffset = UKismetMathLibrary::RLerp(StartRot, EndRot, alphaSin, true);

	FVector StartLoc = -rangeLoc;
	FVector EndLot = rangeLoc;
	HandLocOffset = UKismetMathLibrary::VLerp(StartLoc, EndLot, alphaSin);
	
	if(bDebugForcePush)UE_LOG(LogTemp, Log, TEXT("%S :: frequency %f, _HandShakeTime %f, HandRotOffset %f, alpha %f"),__FUNCTION__, frequency, _HandShakeTime,HandRotOffset.Pitch, alphaSin);
}

//------------------

#pragma endregion Screw

	
	

