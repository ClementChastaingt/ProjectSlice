// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ProceduralAnimComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Character/PC/PS_Character.h"
#include "ProjectSlice/Components/PC/PS_PlayerCameraComponent.h"


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

	
	if(IsValid(_PlayerCharacter->GetParkourComponent()))
	{
		_PlayerCharacter->GetParkourComponent()->OnDashEvent.AddUniqueDynamic(this, &UPS_ProceduralAnimComponent::DashDip);
	}
	
}

void UPS_ProceduralAnimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	//Unbind delegate
	if(IsValid(_PlayerCharacter->GetParkourComponent()))
	{
		_PlayerCharacter->GetParkourComponent()->OnDashEvent.RemoveDynamic(this, &UPS_ProceduralAnimComponent::DashDip);
	}
}

// Called every frame
void UPS_ProceduralAnimComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	Dip();
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
	if(!_PlayerCharacter->GetParkourComponent()->IsWallRunning())
	{
		InAirTilt = UKismetMathLibrary::RInterpTo(InAirTilt, FRotator(0.0f, LocationLagPosition.Z * -2.0f,0.0f), deltaTime, (1 / deltaTime) / AirTiltLagSmoothingSpeed);
		InAirOffset = UKismetMathLibrary::VInterpTo(InAirOffset,FVector(LocationLagPosition.Z * 0.5f, 0.0f, 0.0f), deltaTime, (1 / deltaTime) / AirTiltLagSmoothingSpeed);
	}
	
}

void UPS_ProceduralAnimComponent::StartWalkingAnim()
{
	OnStartWalkFeedbackEvent.Broadcast();
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
	const bool bIsWalkProcAnimDesactive = (playerMovementComp->MovementMode == MOVE_Falling && !_PlayerCharacter->GetParkourComponent()->IsWallRunning())
	|| playerMovementComp->IsCrouching()
	|| _PlayerCharacter->GetParkourComponent()->IsDashing()
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

void UPS_ProceduralAnimComponent::ApplyWindingVibration(const float alpha)
{
	if(!IsValid(_PlayerCharacter) || !IsValid(GetWorld()))
		return;
		
	const float maxHookOffset = UKismetMathLibrary::MapRangeClamped(alpha, 0.0f, 1.0f, 0.0, HookLocMaxOffset);
	HookLocOffset = FVector(0.0f, FMath::RandRange(-maxHookOffset, maxHookOffset), FMath::RandRange(-maxHookOffset, maxHookOffset));
}

//------------------
#pragma endregion Sway
	
	

