// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ParkourComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Data/PS_GlobalType.h"
#include "ProjectSlice/Data/PS_TraceChannels.h"
#include "ProjectSlice/PC/PS_Character.h"


// Sets default values for this component's properties
UPS_ParkourComponent::UPS_ParkourComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

}


// Called when the game starts
void UPS_ParkourComponent::BeginPlay()
{
	Super::BeginPlay();

	//Init default Variable
	_PlayerCharacter = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if(!IsValid(_PlayerCharacter)) return;
	
	_PlayerController = Cast<AProjectSlicePlayerController>(_PlayerCharacter->GetController());
	if(!IsValid(_PlayerController))return;

	DefaulGroundFriction = _PlayerCharacter->GetCharacterMovement()->GroundFriction;
	DefaultBrakingDeceleration = _PlayerCharacter->GetCharacterMovement()->BrakingDecelerationWalking;
	
	//Link Event Receiver
	this->OnComponentBeginOverlap.AddUniqueDynamic(this,&UPS_ParkourComponent::OnParkourDetectorBeginOverlapEventReceived);
	this->OnComponentEndOverlap.AddUniqueDynamic(this,&UPS_ParkourComponent::OnParkourDetectorEndOverlapEventReceived);
	_PlayerCharacter->MovementModeChangedDelegate.AddUniqueDynamic(this,&UPS_ParkourComponent::OnMovementModeChangedEventReceived);
	
	//Custom Tick
	SetComponentTickEnabled(false);
	if (IsValid(GetWorld()))
	{
		FTimerDelegate wallRunTick_TimerDelegate;
		wallRunTick_TimerDelegate.BindUObject(this, &UPS_ParkourComponent::WallRunTick);
		GetWorld()->GetTimerManager().SetTimer(WallRunTimerHandle, wallRunTick_TimerDelegate, CustomTickRate, true);
		GetWorld()->GetTimerManager().PauseTimer(WallRunTimerHandle);

		FTimerDelegate slideTick_TimerDelegate;
		slideTick_TimerDelegate.BindUObject(this, &UPS_ParkourComponent::SlideTick);
		GetWorld()->GetTimerManager().SetTimer(SlideTimerHandle, slideTick_TimerDelegate, CustomTickRate, true);
		GetWorld()->GetTimerManager().PauseTimer(SlideTimerHandle);

		FTimerDelegate canStandTick_TimerDelegate;
		canStandTick_TimerDelegate.BindUObject(this, &UPS_ParkourComponent::CanStandTick);
		GetWorld()->GetTimerManager().SetTimer(CanStandTimerHandle, canStandTick_TimerDelegate, CanStandTickRate, true);
		GetWorld()->GetTimerManager().PauseTimer(CanStandTimerHandle);
	}
}


// Called every frame
void UPS_ParkourComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if(!bIsResetingCameraTilt && !bIsStooping && !bIsMantling)
		SetComponentTickEnabled(false);
		
	//-----Smooth crouching-----
	Stooping();

	//-----Mantling move -----
	MantleTick();

	//-----CameraTilt smooth reset-----
	CameraTilt(GetWorld()->GetTimeSeconds(), StartCameraTiltResetTimestamp);

	
	
}


#pragma region WallRun
//------------------

void UPS_ParkourComponent::WallRunTick()
{
	//-----WallRunTick-----
	if(!bIsWallRunning || !IsValid(_PlayerCharacter) || !IsValid(_PlayerController)|| !IsValid(GetWorld())) return;
	
	//WallRun timer setup
	WallRunSeconds = WallRunSeconds + CustomTickRate;
	const float endTimestamp = StartWallRunTimestamp + WallRunTimeToFall;
	const float alphaWallRun = UKismetMathLibrary::MapRangeClamped(WallRunSeconds, StartWallRunTimestamp, endTimestamp, 0,1);

	//-----Velocity Weight-----
	//Setup alpha use for Force interp
	float curveForceAlpha = alphaWallRun;
	if(IsValid(WallRunGravityCurve))
		curveForceAlpha = WallRunForceCurve->GetFloatValue(alphaWallRun);
	
	VelocityWeight = FMath::Lerp(EnterVelocity, EnterVelocity + WallRunForceBoost,curveForceAlpha);
	
	//-----Fake Gravity Velocity-----
	float curveGravityAlpha = alphaWallRun;
	if(IsValid(WallRunGravityCurve))
		curveGravityAlpha = WallRunGravityCurve->GetFloatValue(alphaWallRun);
	
	FVector customWallDirection = WallRunDirection;
	customWallDirection.Z = FMath::Lerp(WallRunDirection.Z, WallRunDirection.Z + 0.5,curveGravityAlpha);
	customWallDirection.Normalize();

	//-----Velocity Stick to Wall-----
	const FVector newPlayerVelocity = customWallDirection * VelocityWeight * (_PlayerController->GetMoveInput().Y > 0.0 ? _PlayerController->GetMoveInput().Y : 0.0);
	_PlayerCharacter->GetCharacterMovement()->Velocity = newPlayerVelocity;
	
	//Stop WallRun if he was too long
	if(alphaWallRun >= 1 || newPlayerVelocity.IsNearlyZero(0.01))
	{
		if(bDebugWallRun)UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun Stop by Velocity"));
		OnWallRunStop();
	}
	
	//Debug
	if(bDebugWallRun) UE_LOG(LogTemp, Log, TEXT("UTZParkourComp :: WallRun alpha %f,  EnterVelocity %f, VelocityWeight %f"),alphaWallRun, EnterVelocity, VelocityWeight);
	
	//----Camera Tilt-----
	CameraTilt(WallRunSeconds, StartWallRunTimestamp);
}

void UPS_ParkourComponent::OnWallRunStart(AActor* otherActor)
{
	if(bDebugWallRun) UE_LOG(LogTemp, Warning, TEXT("%S, bIsWallRunning %i"), __FUNCTION__, bIsWallRunning);
	
	if(bIsWallRunning) return;
	
	//-----OnWallRunStart-----	
	//Activate Only if in Air
	if(!_PlayerCharacter->GetCharacterMovement()->IsFalling() && !_PlayerCharacter->GetCharacterMovement()->IsFlying() && !bForceWallRun) return;
	
	//WallRun Logic activation
	const UCameraComponent* playerCam = _PlayerCharacter->GetFirstPersonCameraComponent();
	const float angleObjectFwdToCamFwd = otherActor->GetActorForwardVector().Dot(playerCam->GetForwardVector());
	
	//Check Enter angle
	// if(FMath::Abs(angleObjectFwdToCamFwd) < MinEnterAngle/10 || bComeFromAir)
	// {
	// 	if(bDebug)UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun abort by WallDotPlayerCam angle %f "), angleObjectFwdToCamFwd * 10);
	// 	return;
	// }

	if(bDebugWallRun)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun start"));
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("StartWallRun")));
	}
	
	//Find WallOrientation from player
	FHitResult outHitRight, outHitLeft;
	const TArray<AActor*> actorsToIgnore= {_PlayerCharacter};
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), _PlayerCharacter->GetActorLocation(), _PlayerCharacter->GetActorLocation() + _PlayerCharacter->GetActorRightVector() * 200, UEngineTypes::ConvertToTraceType(ECC_Parkour),
										  false, actorsToIgnore, true ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outHitLeft, true, FColor::Blue);

	UKismetSystemLibrary::LineTraceSingle(GetWorld(), _PlayerCharacter->GetActorLocation(), _PlayerCharacter->GetActorLocation() - _PlayerCharacter->GetActorRightVector() * 200, UEngineTypes::ConvertToTraceType(ECC_Parkour),
									  false, actorsToIgnore, true ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outHitRight, true, FColor::Blue);
	
	float playerToWallOrientation;
	if(outHitRight.bBlockingHit && outHitRight.GetActor() == otherActor)
	{
		playerToWallOrientation = 1;
	}else if(outHitLeft.bBlockingHit && outHitLeft.GetActor() == otherActor)
	{
		playerToWallOrientation = -1;
	}
	else
	{
		playerToWallOrientation = 0;
		return;
	}
	
	//Determine Orientations
	WallToPlayerOrientation = FMath::Sign(angleObjectFwdToCamFwd * playerToWallOrientation);
	CameraTiltOrientation = WallToPlayerOrientation * FMath::Sign(angleObjectFwdToCamFwd);
	WallRunDirection = otherActor->GetActorForwardVector() * FMath::Sign(angleObjectFwdToCamFwd);

	if(bDebugWallRun)
	{
		DrawDebugDirectionalArrow(GetWorld(), _PlayerCharacter->GetActorLocation() , _PlayerCharacter->GetActorLocation() + WallRunDirection * 200, 5.0f, FColor::Yellow, false, 2, 10, 2);
		DrawDebugDirectionalArrow(GetWorld(), otherActor->GetActorLocation(), otherActor->GetActorLocation() + otherActor->GetActorRightVector() * 200, 10.0f, FColor::Green, false, 2, 10, 3);
		DrawDebugDirectionalArrow(GetWorld(), otherActor->GetActorLocation(), otherActor->GetActorLocation() + otherActor->GetActorForwardVector() * 200, 10.0f, FColor::Red, false, 2, 10, 3);
	}

	//Constraint init
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintEnabled(true);
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0,0,0));

	//Timer init
	StartWallRunTimestamp = GetWorld()->GetTimeSeconds();
	WallRunSeconds = StartWallRunTimestamp;
	GetWorld()->GetTimerManager().UnPauseTimer(WallRunTimerHandle);

	//--------Camera_Tilt Setup--------
	SetupCameraTilt(false, CameraTiltOrientation * WallRunCameraAngle);

	//Setup work var
	EnterVelocity = _PlayerCharacter->GetCharacterMovement()->GetLastUpdateVelocity().Length();	
	Wall = otherActor;
	bForceWallRun = false;
	bIsWallRunning = true;
	
}

void UPS_ParkourComponent::OnWallRunStop()
{
	if(!bIsWallRunning) return;
	
	//-----OnWallRunStop-----
	if(bDebugWallRun) UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun stop"));
	
	GetWorld()->GetTimerManager().PauseTimer(WallRunTimerHandle);
	WallRunSeconds = 0.0f;

	//-----Reset Variables-----
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintEnabled(false);
	
	VelocityWeight = 1.0f;

	//--------Camera_Tilt--------
	SetupCameraTilt(true, CameraTiltOrientation * WallRunCameraAngle);
	
	_PlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	bIsWallRunning = false;
}

void UPS_ParkourComponent::JumpOffWallRun()
{
	if(!bIsWallRunning) return;

	if(!IsValid(Wall) || !IsValid(_PlayerCharacter)) return;

	if(bDebugWallRunJump)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun jump off"));
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("WallRun Jump Off")));
	}

	OnWallRunStop();
	const FVector jumpForce =  Wall->GetActorRightVector() * WallToPlayerOrientation * JumpOffForceMultiplicator;
	if(bDebugWallRunJump) DrawDebugDirectionalArrow(GetWorld(), Wall->GetActorLocation(), Wall->GetActorLocation() + Wall->GetActorRightVector() * 200, 10.0f, FColor::Orange, false, 2, 10, 3);

	_PlayerCharacter->LaunchCharacter(jumpForce,false,false);	
}
//------------------
#pragma endregion WallRun

#pragma region Camera_Tilt
//------------------

void UPS_ParkourComponent::SetupCameraTilt(const bool bIsReset, const FRotator& targetAngle)
{
	StartCameraRot = _PlayerController->GetControlRotation().Clamp();
	DefaultCameraRot = StartCameraRot + UKismetMathLibrary::NegateRotator(targetAngle);
	TargetCameraRot = StartCameraRot + targetAngle;

	if(bDebugCameraTilt)
		UE_LOG(LogTemp, Warning, TEXT("%S bIsReset %i, StartCameraRot %s, DefaultCameraRot %s, TargetCameraRot %s"),__FUNCTION__, bIsReset, *StartCameraRot.ToString(),*DefaultCameraRot.ToString(),*TargetCameraRot.ToString());
	
	//TODO :: Only good for WallRun reset
	bIsResetingCameraTilt = bIsReset;
	if(bIsResetingCameraTilt)
	{
		DefaultCameraRot.Pitch = 0;
		DefaultCameraRot.Roll = 0;
		StartCameraTiltResetTimestamp = GetWorld()->GetTimeSeconds();
		SetComponentTickEnabled(true);
	}

}

void UPS_ParkourComponent::CameraTilt(float currentSeconds, const float startTime)
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
	const FRotator newRotTarget = (TargetCameraRot.IsNearlyZero() || bIsResetingCameraTilt) ? DefaultCameraRot : TargetCameraRot;
	const FRotator newRot = FMath::Lerp(StartCameraRot,newRotTarget, curveTiltAlpha);
	
	//Rotate
	_PlayerController->SetControlRotation(newRot);
	if(bDebugCameraTilt) UE_LOG(LogTemp, Log, TEXT("UTZParkourComp :: CurrentRoll: %f, alphaTilt %f"), _PlayerController->GetControlRotation().Roll, alphaTilt);
}


//------------------
#pragma endregion Camera_Tilt

#pragma region Crouch
//------------------

void UPS_ParkourComponent::OnCrouch()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetCapsuleComponent()) || !IsValid(_PlayerCharacter->GetCharacterMovement()) || !IsValid(GetWorld())) return;

	if(_PlayerCharacter->GetCharacterMovement()->MovementMode != MOVE_Walking && !bIsCrouched) return;

	const ACharacter* DefaultCharacter = _PlayerCharacter->GetClass()->GetDefaultObject<ACharacter>();

	StartCrouchHeight = _PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	StartStoopTimestamp = GetWorld()->GetTimeSeconds();
	bIsStooping = false;
		
	//Crouch
	if(!bIsCrouched && _PlayerCharacter->GetCharacterMovement() && _PlayerCharacter->GetCharacterMovement()->CanEverCrouch() && _PlayerCharacter->GetRootComponent() && !_PlayerCharacter->GetRootComponent()->IsSimulatingPhysics())
	{
		// See if collision is already at desired size.
		if (_PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == _PlayerCharacter->GetCharacterMovement()->GetCrouchedHalfHeight()) return;
				
		//Activate interp
		bIsCrouched = true;
		bIsStooping = true;

		if(bDebugCrouch) UE_LOG(LogTemp, Warning, TEXT("PS_Character :: Crouched"));
	}
	//Uncrouch
	else if(CanStand())
	{		
		// See if collision is already at desired size.
		if(_PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight())return;

		//Activate interp
		bIsCrouched = false;
		bIsStooping = true;
		if(bIsSliding) OnStopSlide();

		if(bDebugCrouch) UE_LOG(LogTemp, Warning, TEXT("PS_Character :: Uncrouched"));
	}
	SetComponentTickEnabled(bIsStooping);

}

bool UPS_ParkourComponent::CanStand() const
{
	if(!IsValid(GetWorld())) return false;

	const ACharacter* defaultCharacter = _PlayerCharacter->GetClass()->GetDefaultObject<ACharacter>();
	
	FVector start = _PlayerCharacter->GetActorLocation();
	FVector end = _PlayerCharacter->GetActorLocation();
	end.Z = _PlayerCharacter->GetActorLocation().Z + defaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

	FHitResult outHit;
	const TArray<AActor*> actorsToIgnore= {_PlayerCharacter};
	UKismetSystemLibrary::SphereTraceSingle(GetWorld(),start,end,defaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(),UEngineTypes::ConvertToTraceType(ECC_Visibility), false,actorsToIgnore, bDebugSlide ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outHit, true);

	const bool canStand = !outHit.bBlockingHit && !outHit.bStartPenetrating;
	if(!canStand && !_PlayerCharacter->IsCrouchInputTrigger())
		GetWorld()->GetTimerManager().UnPauseTimer(CanStandTimerHandle);
	else
		GetWorld()->GetTimerManager().PauseTimer(CanStandTimerHandle);

	return canStand;
}

void UPS_ParkourComponent::CanStandTick()
{
	if(!IsValid(_PlayerCharacter)) return;

	if(!_PlayerCharacter->IsCrouchInputTrigger()) OnCrouch();
}

void UPS_ParkourComponent::Stooping()
{
	if(!IsValid(_PlayerCharacter)) return;
	
	if(!bIsStooping) return;
	
	const float targetHeight = bIsCrouched ? _PlayerCharacter->GetCharacterMovement()->GetCrouchedHalfHeight() : _PlayerCharacter->GetClass()->GetDefaultObject<ACharacter>()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float alpha = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetTimeSeconds(), StartStoopTimestamp, StartStoopTimestamp + SmoothCrouchDuration, 0,1);
	
	float curveAlpha = alpha;
	if(IsValid(CrouchCurve))
		curveAlpha = CrouchCurve->GetFloatValue(alpha);

	const float currentHeight = FMath::Lerp(StartCrouchHeight,targetHeight, curveAlpha);
	CrouchAlpha = curveAlpha;
	_PlayerCharacter->GetCapsuleComponent()->SetCapsuleHalfHeight(currentHeight, true);
	
	if(curveAlpha >= 1)
	{
		const float enterSpeed = _PlayerCharacter->GetVelocity().Length();
		
		bIsCrouched ? _PlayerCharacter->Crouch() : _PlayerCharacter->UnCrouch();
		
		//Begin Slide if Velocity on enter is enough high
		if(bIsCrouched && enterSpeed > _PlayerCharacter->GetCharacterMovement()->MaxWalkSpeedCrouched)
			OnStartSlide();
		
		bIsStooping = false;
		CrouchAlpha = 0.0f;
	}
}

//------------------
#pragma endregion Crouch

#pragma region Slide
//------------------

void UPS_ParkourComponent::OnStartSlide()
{
	if(bDebugSlide) UE_LOG(LogTemp, Warning, TEXT("PS_Character :: Slide Start"));

	if(!IsValid(GetWorld()) || !IsValid(_PlayerController) || !IsValid(_PlayerCharacter)) return;
	
	bIsSliding = true;
	StartSlideTimestamp = GetWorld()->GetTimeSeconds();
	SlideSeconds = StartSlideTimestamp;
	
	//--------Camera_Tilt Setup--------
	// SetupCameraTilt(false, SlideCameraAngle);

	//--------Configure Movement Behaviour-------
	_PlayerController->SetIgnoreMoveInput(true);
	SlideDirection = _PlayerCharacter->GetCharacterMovement()->GetLastInputVector() * ((_PlayerCharacter->GetCharacterMovement()->GetLastUpdateVelocity() * FVector(1,1,0)).Length()); 
	_PlayerCharacter->GetCharacterMovement()->GroundFriction = 0.0f;
	
	GetWorld()->GetTimerManager().UnPauseTimer(SlideTimerHandle);

	OnSlideEvent.Broadcast(bIsSliding);
}

void UPS_ParkourComponent::OnStopSlide()
{
	if(bDebugSlide) UE_LOG(LogTemp, Warning, TEXT("PS_Character :: Slide Stop"));

	if(!IsValid(GetWorld()) || !IsValid(_PlayerCharacter)) return;
	
	GetWorld()->GetTimerManager().PauseTimer(SlideTimerHandle);
	SlideSeconds = 0;
	SlideAlpha = 0.0f;
	
	//--------Camera_Tilt Setup--------
	// SetupCameraTilt(true, SlideCameraAngle);
	
	//--------Configure Movement Behaviour-------
	
	_PlayerController->SetIgnoreMoveInput(false);
	_PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed = _PlayerCharacter->GetDefaultMaxWalkSpeed();
	_PlayerCharacter->GetCharacterMovement()->GroundFriction = DefaulGroundFriction;
	_PlayerCharacter->GetCharacterMovement()->BrakingDecelerationWalking = DefaultBrakingDeceleration;

	bIsSliding = false;
	if(bIsCrouched) _PlayerCharacter->Crouching();

	OnSlideEvent.Broadcast(bIsSliding);
	
}

void UPS_ParkourComponent::SlideTick()
{
	if(!IsValid(_PlayerCharacter)) return;
	
	if(!bIsSliding) return;

	SlideSeconds = SlideSeconds + CustomTickRate;
	UCharacterMovementComponent* characterMovement =  _PlayerCharacter->GetCharacterMovement();

	//-----Camera_Tilt-----
	// CameraTilt(SlideSeconds,StartSlideTimestamp);


	//-----Velocity-----
	SlideAlpha = UKismetMathLibrary::MapRangeClamped(SlideSeconds, StartSlideTimestamp, StartSlideTimestamp + TimeToMaxBrakingDeceleration, 0,1);
	float curveDecAlpha = SlideAlpha;
	float curveAccAlpha = SlideAlpha;
	if(IsValid(SlideBrakingDecelerationCurve))
		curveDecAlpha = SlideBrakingDecelerationCurve->GetFloatValue(SlideAlpha);

	if(IsValid(SlideAccelerationCurve))
		curveAccAlpha = SlideAccelerationCurve->GetFloatValue(SlideAlpha);
	
	characterMovement->AddForce(CalculateFloorInflucence(characterMovement->CurrentFloor.HitResult.Normal) * SlideForceMultiplicator);
	_PlayerCharacter->GetCharacterMovement()->BrakingDecelerationWalking = FMath::Lerp(0,MaxBrakingDecelerationSlide, curveDecAlpha);

	if(SlideAlpha < 1 )
		_PlayerCharacter->GetCharacterMovement()->Velocity = FMath::Lerp(SlideDirection, SlideDirection * 2.0f,curveAccAlpha);

	if(bDebugSlide) UE_LOG(LogTemp, Log, TEXT("MaxWalkSpeed :%f, InputScale : %f, brakDec: %f"), _PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed, curveAccAlpha, _PlayerCharacter->GetCharacterMovement()->BrakingDecelerationWalking);
	
	//Clamp Velocity
	if(_PlayerCharacter->GetVelocity().Length() > _PlayerCharacter->GetDefaultMaxWalkSpeed() * SlideSpeedMultiplicator)
	{
		FVector newVel = _PlayerCharacter->GetVelocity();
		newVel.Normalize();
	
		characterMovement->Velocity = newVel * _PlayerCharacter->GetDefaultMaxWalkSpeed() * SlideSpeedMultiplicator;
	}
	

	//-----Stop Slide-----
	if(_PlayerCharacter->GetVelocity().Length() < characterMovement->MaxWalkSpeedCrouched)
		OnStopSlide();
}

FVector UPS_ParkourComponent::CalculateFloorInflucence(const FVector& floorNormal) const
{
	if(floorNormal.Equals(FVector(0,0,1))) return FVector::ZeroVector;

	FVector out = floorNormal.Cross(floorNormal.Cross(FVector(0,0,1)));
	out.Normalize();

	return out;
}


//------------------
#pragma endregion Slide

#pragma region Mantle
//------------------

bool UPS_ParkourComponent::CanMantle()
{
	if(!IsValid(GetWorld()) || bIsCrouched) return false;
	
	FHitResult outHitFwd, outHitHgt, outCapsHit;
	const TArray<AActor*> actorsToIgnore= {_PlayerCharacter};
	const float capsuleOffset = _PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius() * 2;
	
	//Forward Trace
	FVector startFwd = _PlayerCharacter->GetActorLocation();
	startFwd.Z = _PlayerCharacter->GetMesh()->GetComponentLocation().Z + _PlayerCharacter->GetCharacterMovement()->GetMaxJumpHeight();
	
	FVector endFwd = startFwd + _PlayerCharacter->GetActorForwardVector() * capsuleOffset;
	endFwd.Z = startFwd.Z;
	
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), startFwd, endFwd, UEngineTypes::ConvertToTraceType(ECC_Visibility),false, actorsToIgnore, bDebugMantle ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outHitFwd, true, FColor::Magenta);
	if(!outHitFwd.bBlockingHit) return false;
	
	//Height Trace
	FVector startHgt = outHitFwd.Location + outHitFwd.Normal * -1 * capsuleOffset;
	startHgt.Z = startHgt.Z + MaxMantleHeight;
	FVector endHgt = outHitFwd.Location + outHitFwd.Normal * -1 * capsuleOffset;
	
	UKismetSystemLibrary::LineTraceSingle(GetWorld(),startHgt,endHgt,UEngineTypes::ConvertToTraceType(ECC_Visibility), false,actorsToIgnore, bDebugMantle ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outHitHgt, true, FColor::Orange);
	if(!outHitHgt.bBlockingHit) return false;

	//Capsule trace for check if have enough place for player
	FVector capsLoc = outHitHgt.Location;
	capsLoc.Z = outHitHgt.Location.Z + _PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() + MantleCapsuletHeightTestOffset;

	UKismetSystemLibrary::CapsuleTraceSingle(GetWorld(),capsLoc,capsLoc,_PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), _PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), UEngineTypes::ConvertToTraceType(ECC_Visibility), false,actorsToIgnore, bDebugMantle ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outCapsHit, true);
	if(outCapsHit.bBlockingHit) return false;

	//Set TargetLoc for 1st and 2nd phase
	_TargetMantleSnapLoc = outHitFwd.Location + outHitFwd.Normal * (_PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius() / 2);
	_TargetMantleSnapLoc.Z = outHitHgt.Location.Z - MantleSnapOffset;
	
	_TargetMantlePullUpLoc = capsLoc;

	//Rotate player forward object
	//TODO :: rotate forward object normal
	// FRotator newRot = _PlayerController->GetControlRotation();
	// float angleNormalToFwd = UKismetMathLibrary::DegAcos(outHitFwd.Normal.Dot(_PlayerCharacter->GetActorForwardVector()));
	// newRot.Yaw = -(newRot.Yaw + angleNormalToFwd);
	// UE_LOG(LogTemp, Error, TEXT("PlayerYaw %f, newYaw %f, angle %f"),_PlayerController->GetControlRotation().Yaw, newRot.Yaw,  UKismetMathLibrary::DegAcos(outHitFwd.Normal.Dot(_PlayerCharacter->GetActorForwardVector())));
	// _PlayerController->SetControlRotation(newRot.Clamp());

	return true;
}

void UPS_ParkourComponent::OnStartMantle()
{
	if(bDebugMantle) UE_LOG(LogTemp, Warning, TEXT ("%S"), __FUNCTION__);
	
	if(!IsValid(GetWorld())) return;

	//TODO :: Change for Custom Move mode CMOVE_Climbing when add custom move mode
	_PlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_None);

	bIsMantling = true;

	//Block 3C
	SetGenerateOverlapEvents(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	_PlayerController->SetIgnoreMoveInput(true);
	_PlayerController->SetIgnoreLookInput(true);

	//Init variables
	StartMantleSnapTimestamp = GetWorld()->GetTimeSeconds();
	_StartMantleLoc = _PlayerCharacter->GetActorLocation();

	//Start movement
	SetComponentTickEnabled(bIsMantling);
}

void UPS_ParkourComponent::OnStoptMantle()
{
	if(bDebugMantle) UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__); 
	
	bIsMantling = false;
	_MantlePhase = EMantlePhase::NONE;

	SetGenerateOverlapEvents(true);
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	
	_PlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	_PlayerController->SetIgnoreMoveInput(false);
	_PlayerController->SetIgnoreLookInput(false);
}

void UPS_ParkourComponent::MantleTick()
{	
	if(!IsValid(_PlayerCharacter)) return;
		
	if(!bIsMantling) return;

	//1st phase
	const float alphaSnap= UKismetMathLibrary::MapRangeClamped(GetWorld()->GetTimeSeconds(), StartMantleSnapTimestamp, StartMantleSnapTimestamp + MantleSnapDuration, 0,1);
	
	//1sdt Phase
	if(alphaSnap < 1)
	{
		_MantlePhase = EMantlePhase::SNAP;

		float curveAlphaSnap = alphaSnap;
		if(IsValid(MantleSnapCurve))
			curveAlphaSnap = MantleSnapCurve->GetFloatValue(alphaSnap);
		
		FVector newPlayerLoc = FMath::Lerp(_StartMantleLoc,_TargetMantleSnapLoc, curveAlphaSnap);
		_PlayerCharacter->SetActorLocation(newPlayerLoc);
		StartMantlePullUpTimestamp = GetWorld()->GetTimeSeconds();
		if(bDebugMantle)
		{
			UE_LOG(LogTemp, Log, TEXT("%S :: alphaSnap %f"),__FUNCTION__, alphaSnap);
			DrawDebugPoint(GetWorld(), _TargetMantleSnapLoc, 20.f, FColor::Green, false);
		}
	}
	//2nd Phase
	else
	{
		_MantlePhase = EMantlePhase::PULL_UP;
		
		const float alphaPullUp = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetTimeSeconds(), StartMantlePullUpTimestamp, StartMantlePullUpTimestamp + MantlePullUpDuration, 0,1);
		if(bDebugMantle) UE_LOG(LogTemp, Log, TEXT("%S :: alphaPullUp %f"),__FUNCTION__, alphaPullUp);
		
		float curveAlphaXY = alphaPullUp;
		if(IsValid(MantlePullUpCurveXY))
			curveAlphaXY = MantlePullUpCurveXY->GetFloatValue(alphaPullUp);
		
		float curveAlphaZ = alphaPullUp;
		if(IsValid(MantlePullUpCurveZ))
			curveAlphaZ = MantlePullUpCurveZ->GetFloatValue(alphaPullUp);
		
		FVector newPlayerLoc = FMath::Lerp(_TargetMantleSnapLoc,_TargetMantlePullUpLoc, curveAlphaXY);
		newPlayerLoc.Z = FMath::Lerp(_TargetMantleSnapLoc.Z,_TargetMantlePullUpLoc.Z, curveAlphaZ);
		_PlayerCharacter->SetActorLocation(newPlayerLoc);
		
		if(alphaPullUp >= 1)
		{
			OnStoptMantle();
		}
	}

}


//------------------
#pragma endregion Mantle

#pragma region Event_Receiver
//------------------


void UPS_ParkourComponent::OnParkourDetectorBeginOverlapEventReceived(UPrimitiveComponent* overlappedComponent,
	AActor* otherActor, UPrimitiveComponent* otherComp, int otherBodyIndex, bool bFromSweep, const FHitResult& sweepResult)
{	
	if(!IsValid(_PlayerCharacter)
		|| !IsValid(GetWorld())
		|| !IsValid(_PlayerCharacter->GetCharacterMovement())
		|| !IsValid(_PlayerCharacter->GetFirstPersonCameraComponent()) 
		|| !IsValid(otherActor)) return;

	FHitResult outHit;
	const TArray<AActor*> actorsToIgnore;
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), GetComponentLocation(), GetComponentLocation() + GetForwardVector() * GetUnscaledCapsuleRadius() * 2, UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false, actorsToIgnore, false ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outHit, true);

	const float angle = UKismetMathLibrary::DegAcos(_PlayerCharacter->GetActorForwardVector().Dot(outHit.Normal * -1));


	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: angle between hit and player forward %f"), __FUNCTION__ , angle);

	if(angle > MaxMantleAngle)
	{
		OnWallRunStart(otherActor);
	}
	else if(CanMantle())
	{
		 OnStartMantle();
	}
}

void UPS_ParkourComponent::OnParkourDetectorEndOverlapEventReceived(UPrimitiveComponent* overlappedComponent,
                                                                    AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex)
{
	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__);

	if(bIsWallRunning)UE_LOG(LogTemp, Log, TEXT("WallRun Stop by Wall end"));
		OnWallRunStop();
}

void UPS_ParkourComponent::OnMovementModeChangedEventReceived(ACharacter* character, EMovementMode prevMovementMode,
	uint8 previousCustomMode)
{
    bComeFromAir = prevMovementMode == MOVE_Falling || prevMovementMode == MOVE_Flying;

	if(bComeFromAir && bIsCrouched) OnCrouch();
}


//------------------
#pragma endregion Event_Receiver




