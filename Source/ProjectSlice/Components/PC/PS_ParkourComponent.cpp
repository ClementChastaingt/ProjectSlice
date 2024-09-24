// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ParkourComponent.h"

#include "PS_PlayerCameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/FunctionLibrary/PSFl.h"
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
	
	if(!bIsStooping && !bIsMantling && !bIsLedging && IsComponentTickEnabled())
		SetComponentTickEnabled(false);
		
	//-----Smooth crouching-----
	Stooping();

	//-----Mantling move -----
	MantleTick();

	//-----Ledge move -----
	LedgeTick();
	
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
	
	//Clamp Max Velocity
	FVector wallRunVel = _WallRunEnterVelocity + WallRunSpeedBoost;
	wallRunVel = UPSFl::ClampVelocity(_WallRunEnterVelocity, _PlayerCharacter->GetVelocity(),wallRunVel,_PlayerCharacter->GetDefaultMaxWalkSpeed() * MaxWallRunSpeedMultiplicator);
	
	VelocityWeight = FMath::Lerp(_WallRunEnterVelocity.Length(), wallRunVel.Length(),curveForceAlpha);
	
	//-----Fake Gravity Velocity-----
	float curveGravityAlpha = alphaWallRun;
	if(IsValid(WallRunGravityCurve))
		curveGravityAlpha = WallRunGravityCurve->GetFloatValue(alphaWallRun);
	
	FVector customWallDirection = WallRunDirection;
	customWallDirection.Z = FMath::Lerp(WallRunDirection.Z, WallRunDirection.Z + 0.5,curveGravityAlpha);
	customWallDirection.Normalize();

	//-----Velocity Stick to Wall-----
	const FVector newPlayerVelocity = customWallDirection * VelocityWeight * (_PlayerController->GetMoveInput().Y > 0.0 ? _PlayerController->GetMoveInput().Y : WallRunNoInputVelocity);
	_PlayerCharacter->GetCharacterMovement()->Velocity = newPlayerVelocity;
	
	//Stop WallRun if he was too long
	if(alphaWallRun >= 1 || newPlayerVelocity.IsNearlyZero(0.01))
	{
		if(bDebugWallRun)UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun Stop by Velocity"));
		OnWallRunStop();
	}
	
	//Debug
	if(bDebugWallRun) UE_LOG(LogTemp, Log, TEXT("UTZParkourComp :: WallRun alpha %f,  _WallRunEnterVelocity %f, VelocityWeight %f"),alphaWallRun, _WallRunEnterVelocity, VelocityWeight);
	
	//----Camera Tilt-----
	_PlayerCharacter->GetFirstPersonCameraComponent()->CameraRollTilt(WallRunSeconds, StartWallRunTimestamp);
}

void UPS_ParkourComponent::OnWallRunStart(AActor* otherActor)
{	
	if(bIsCrouched) return;
	
	//Activate Only if in Air
	if(!_PlayerCharacter->GetCharacterMovement()->IsFalling() && !_PlayerCharacter->GetCharacterMovement()->IsFlying() && !bForceWallRun) return;

	if(bDebugWallRun)
	{
		UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("%S :: StartWallRun"),__FUNCTION__));
	}
	
	//WallRun Logic activation
	const UPS_PlayerCameraComponent* playerCam = _PlayerCharacter->GetFirstPersonCameraComponent();
	const float angleObjectFwdToCamFwd = otherActor->GetActorForwardVector().Dot(playerCam->GetForwardVector());
		
	
	//Find WallOrientation from player
	FHitResult outHitRight, outHitLeft;
	const TArray<AActor*> actorsToIgnore= {_PlayerCharacter};
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), _PlayerCharacter->GetActorLocation(), _PlayerCharacter->GetActorLocation() + _PlayerCharacter->GetActorRightVector() * 200, UEngineTypes::ConvertToTraceType(ECC_Visibility),
										  false, actorsToIgnore, true ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outHitLeft, true, FColor::Blue);

	UKismetSystemLibrary::LineTraceSingle(GetWorld(), _PlayerCharacter->GetActorLocation(), _PlayerCharacter->GetActorLocation() - _PlayerCharacter->GetActorRightVector() * 200, UEngineTypes::ConvertToTraceType(ECC_Visibility),
									  false, actorsToIgnore, true ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outHitRight, true, FColor::Blue);

	if(!bIsWallRunning)
	{
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
			UE_LOG(LogTemp, Error, TEXT("%S :: playerToWallOrientation == 0"), __FUNCTION__);
			playerToWallOrientation = 0;
			return;
		}

		WallToPlayerOrientation = FMath::Sign(angleObjectFwdToCamFwd * playerToWallOrientation);
	}
	
	//Determine Orientations
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
	if(!bIsWallRunning)
	{
		StartWallRunTimestamp = GetWorld()->GetTimeSeconds();
		WallRunSeconds = StartWallRunTimestamp;
		GetWorld()->GetTimerManager().UnPauseTimer(WallRunTimerHandle);
	}

	//--------Camera_Tilt Setup--------
	_PlayerCharacter->GetFirstPersonCameraComponent()->SetupCameraTilt(false, ETiltUsage::WALL_RUN, CameraTiltOrientation);

	//Setup work var
	_WallRunEnterVelocity = _PlayerCharacter->GetCharacterMovement()->GetLastUpdateVelocity();	
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
	_PlayerCharacter->GetFirstPersonCameraComponent()->SetupCameraTilt(true, ETiltUsage::WALL_RUN);
	
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

	//Determine direction && force
	const float playerFwdToWallRightAngle = UKismetMathLibrary::DegAcos(_PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector().Dot(Wall->GetActorRightVector() * WallToPlayerOrientation));
	const FVector jumpDir = playerFwdToWallRightAngle < JumpOffPlayerFwdDirThresholdAngle ? _PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector() : Wall->GetActorRightVector() * WallToPlayerOrientation;

	const FVector jumpForce = jumpDir * JumpOffForceMultiplicator;
	
	if(bDebugWallRunJump)
	{
		UE_LOG(LogTemp, Log, TEXT("%S :: jumpDir use %s (%f°)"),__FUNCTION__, playerFwdToWallRightAngle < JumpOffPlayerFwdDirThresholdAngle ? TEXT("Weapon Forward") : TEXT("Wall Right"), playerFwdToWallRightAngle);
		DrawDebugDirectionalArrow(GetWorld(), Wall->GetActorLocation(), Wall->GetActorLocation() + Wall->GetActorRightVector() * 200, 10.0f, FColor::Orange, false, 2, 10, 3);
	}

	//Clamp Max Velocity
	FVector jumpTargetVel = jumpForce;
	UPSFl::ClampVelocity(jumpDir,jumpForce,_PlayerCharacter->GetDefaultMaxWalkSpeed() * MaxWallRunSpeedMultiplicator);
	
	//Launch chara
	_PlayerCharacter->LaunchCharacter(jumpTargetVel,false,false);
	
}
//------------------
#pragma endregion WallRun


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
	if(!IsValid(_PlayerController) || !IsValid(GetWorld())) return false;

	const ACharacter* defaultCharacter = _PlayerCharacter->GetClass()->GetDefaultObject<ACharacter>();
	
	FVector start = _PlayerCharacter->GetActorLocation();
	FVector end = _PlayerCharacter->GetActorLocation();
	end.Z = _PlayerCharacter->GetActorLocation().Z + defaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

	FHitResult outHit;
	const TArray<AActor*> actorsToIgnore= {_PlayerCharacter};
	UKismetSystemLibrary::SphereTraceSingle(GetWorld(),start,end,defaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(),UEngineTypes::ConvertToTraceType(ECC_Visibility), false,actorsToIgnore, bDebugSlide ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outHit, true);

	const bool canStand = !outHit.bBlockingHit && !outHit.bStartPenetrating;
	if(!canStand && !_PlayerController->IsCrouchInputTrigger())
		GetWorld()->GetTimerManager().UnPauseTimer(CanStandTimerHandle);
	else
		GetWorld()->GetTimerManager().PauseTimer(CanStandTimerHandle);

	return canStand;
}

void UPS_ParkourComponent::CanStandTick()
{
	if(!IsValid(_PlayerController)) return;

	if(!_PlayerController->IsCrouchInputTrigger())
		OnCrouch();
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
		if(bIsCrouched && enterSpeed >= _PlayerCharacter->GetCharacterMovement()->MaxWalkSpeedCrouched && _PlayerController->GetMoveInput().SquaredLength() > 0.0f)
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
	
	//--------Configure Movement Behaviour-------
	_PlayerController->SetIgnoreMoveInput(true);
	SlideDirection = _PlayerCharacter->GetCharacterMovement()->GetLastInputVector() * _PlayerCharacter->GetCharacterMovement()->GetLastUpdateVelocity().Length(); 
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
	OutSlopePitchDegreeAngle = 0;
	OutSlopeRollDegreeAngle = 0;

	
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
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetCharacterMovement()) || !IsValid(GetWorld())) return;
	
	if(!bIsSliding) return;

	SlideSeconds = SlideSeconds + CustomTickRate;
	UCharacterMovementComponent* characterMovement = _PlayerCharacter->GetCharacterMovement();
	

	//-----Velocity-----
	SlideAlpha = UKismetMathLibrary::MapRangeClamped(SlideSeconds, StartSlideTimestamp, StartSlideTimestamp + TimeToMaxBrakingDeceleration, 0,1);
	float curveDecAlpha = SlideAlpha;
	float curveAccAlpha = SlideAlpha;
	
	//Impulse on Slope else VelocityCurve
	if(IsValid(SlideBrakingDecelerationCurve))
		curveDecAlpha = SlideBrakingDecelerationCurve->GetFloatValue(SlideAlpha);

	if(IsValid(SlideAccelerationCurve))
		curveAccAlpha = SlideAccelerationCurve->GetFloatValue(SlideAlpha);

	//Use Impulse
	FVector minSlideVel = SlideDirection;

	//TODO :: Dash application
	// FVector slideVel = characterMovement->CurrentFloor.HitResult.Normal * 1500000.0f
	// characterMovement->AddImpulse((slideVel * GetWorld()->DeltaRealTimeSeconds) * _PlayerCharacter->CustomTimeDilation);
	

	//Impulse on Slope else VelocityCurve
	FVector slideVel = CalculateFloorInflucence(characterMovement->CurrentFloor.HitResult.Normal) * 1500000.0f;
	
	if(slideVel.SquaredLength() > minSlideVel.SquaredLength())
	{
		// if(SlideSeconds <= 0.0f) bIsSlidingOnSlope = true;
		
		characterMovement->AddImpulse(slideVel * GetWorld()->DeltaRealTimeSeconds * _PlayerCharacter->CustomTimeDilation);
		if(bDebugSlide) UE_LOG(LogTemp, Log, TEXT("Velocity Impulse: %f"), _PlayerCharacter->GetCharacterMovement()->Velocity.Length());

		//Determine if can use slide on slope
		UKismetMathLibrary::GetSlopeDegreeAngles(GetRightVector(),characterMovement->CurrentFloor.HitResult.Normal, GetUpVector(),OutSlopePitchDegreeAngle,OutSlopeRollDegreeAngle);
		
		//Clamp Max Velocity
		const float rangedPitchMultiplicator = UKismetMathLibrary::MapRangeClamped(OutSlopePitchDegreeAngle,90,characterMovement->GetWalkableFloorAngle(),1.0,SlopeForceDecelerationWeight);
		UE_LOG(LogTemp, Error, TEXT("rangedPitchMultiplicator %f, OutSlopePitchDegreeAngle %f, OutSlopeRollDegreeAngle %f"), rangedPitchMultiplicator, OutSlopePitchDegreeAngle, OutSlopeRollDegreeAngle);
		_PlayerCharacter->GetCharacterMovement()->Velocity = UPSFl::ClampVelocity(slideVel, _PlayerCharacter->GetVelocity(),slideVel * SlideSpeedBoost, _PlayerCharacter->GetDefaultMaxWalkSpeed() * (MaxSlideSpeedMultiplicator * (OutSlopePitchDegreeAngle < 0 ? 1.0f : rangedPitchMultiplicator)));
		
	}
	//Use Velocity
	else if(SlideAlpha < 1 && OutSlopePitchDegreeAngle <= 0)
	{
		//Clamp Max Velocity
		FVector slideTargetVel = UPSFl::ClampVelocity(minSlideVel, _PlayerCharacter->GetVelocity(),minSlideVel * SlideSpeedBoost,_PlayerCharacter->GetDefaultMaxWalkSpeed() * MaxSlideSpeedMultiplicator);

		_PlayerCharacter->GetCharacterMovement()->Velocity = FMath::Lerp(minSlideVel, slideTargetVel, curveAccAlpha);
		if(bDebugSlide) UE_LOG(LogTemp, Log, TEXT("Velocity Force: %f"), _PlayerCharacter->GetCharacterMovement()->Velocity.Length());
	}

	//Change BrakingVel
	_PlayerCharacter->GetCharacterMovement()->BrakingDecelerationWalking = FMath::Lerp(0, MaxBrakingDecelerationSlide, curveDecAlpha);
	
	if(bDebugSlide) UE_LOG(LogTemp, Log, TEXT("MaxWalkSpeed :%f, floorInflucen : %s, brakDec: %f"), _PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed, *CalculateFloorInflucence(characterMovement->CurrentFloor.HitResult.Normal).ToString(), _PlayerCharacter->GetCharacterMovement()->BrakingDecelerationWalking);
		

	//-----Stop Slide-----
	if(_PlayerCharacter->GetVelocity().Size2D() < characterMovement->MaxWalkSpeedCrouched)
	{
		if(bDebugSlide) UE_LOG(LogTemp, Warning, TEXT("OnStopSlide by velocity %f"), _PlayerCharacter->GetVelocity().Size2D());
		OnStopSlide();
	}
	
}

FVector UPS_ParkourComponent::CalculateFloorInflucence(const FVector& floorNormal) const
{
	if(floorNormal.Equals(FVector(0,0,1))) return FVector::ZeroVector;

	FVector out = floorNormal.Cross(floorNormal.Cross(FVector(0,0,1)));
	out.Normalize();

	DrawDebugDirectionalArrow(GetWorld(), GetComponentLocation(), GetComponentLocation() + out * 500, 5.0f, FColor::Blue, false, 2, 10, 2);
	
	return out;
}


//------------------
#pragma endregion Slide

#pragma region Mantle
//------------------

bool UPS_ParkourComponent::CanMantle(const FHitResult& inFwdHit)
{
	if(!IsValid(GetWorld())) return false;

	if(bIsCrouched) OnCrouch();
	
	FHitResult outHitHgt, outCapsHit;
	const TArray<AActor*> actorsToIgnore= {_PlayerCharacter};
	const float capsuleOffset = _PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius();

	//Height Trace
	FVector startHgt = inFwdHit.Location + inFwdHit.Normal * -1 * capsuleOffset;
	startHgt.Z = startHgt.Z + MaxMantleHeight;
	FVector endHgt = inFwdHit.Location + inFwdHit.Normal * -1 * capsuleOffset;
		
	UKismetSystemLibrary::LineTraceSingle(GetWorld(),startHgt,endHgt,UEngineTypes::ConvertToTraceType(ECC_Visibility), false,actorsToIgnore, bDebugMantle ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outHitHgt, true, FColor::Orange);
	
	if(!outHitHgt.bBlockingHit || outHitHgt.bStartPenetrating) return false;
		
	//TODO :: Do this a func in characMovmeent
	const bool bIsInAir = _PlayerCharacter->GetCharacterMovement()->IsFalling() || _PlayerCharacter->GetCharacterMovement()->IsFlying();
		
	//If try by meet edge check if not too low
	const bool bComeFromAirAndTooLowToEdge = bIsInAir && outHitHgt.Location.Z - _PlayerCharacter->GetActorLocation().Z > _PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	if(bComeFromAirAndTooLowToEdge)
	{
		UE_LOG(LogTemp, Warning, TEXT("%S :: bComeFromAirAndTooLowToEdge true"), __FUNCTION__);
		return false;
	}
	
	//If come from Jump who can pass without Mantle
	const bool bPlayerUpperThanTarget = bIsInAir && _PlayerCharacter->GetMesh()->GetComponentLocation().Z < outHitHgt.Location.Z  && outHitHgt.Location.Z  < _PlayerCharacter->GetActorLocation().Z;

	DrawDebugPoint(GetWorld(), _PlayerCharacter->GetMesh()->GetComponentLocation(), 20.f, FColor::Green, true);

	UE_LOG(LogTemp, Error, TEXT("bIsInAir %i, bPlayerUpperThanTarget %i"), bIsInAir, outHitHgt.Location.Z < _PlayerCharacter->GetMesh()->GetComponentLocation().Z);
	
	//Force Landing by Ledge
	if(bPlayerUpperThanTarget && !_PlayerCharacter->GetCharacterMovement()->bPerformingJumpOff)
	{
		OnStartLedge(outHitHgt.Location);
		return false;
	}
	
	//Capsule trace for check if have enough place for player
	FVector capsLoc = outHitHgt.Location;
	capsLoc.Z = outHitHgt.Location.Z + _PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() + MantleCapsuletHeightTestOffset;

	UKismetSystemLibrary::CapsuleTraceSingle(GetWorld(),capsLoc,capsLoc,_PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), _PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), UEngineTypes::ConvertToTraceType(ECC_Visibility), false,actorsToIgnore, bDebugMantle ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outCapsHit, true);
	if(outCapsHit.bBlockingHit) return false;

	//Set TargetLoc && TargteROt for 1st and 2nd phase
	_TargetMantleSnapLoc = inFwdHit.Location + inFwdHit.Normal * (_PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius() / 2);
	_TargetMantleSnapLoc.Z = outHitHgt.Location.Z - MantleSnapOffset;
	_TargetMantlePullUpLoc = capsLoc;
	_TargetMantleRot =  (inFwdHit.Normal * -1).Rotation();
	
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
	_StartMantleRot = _PlayerCharacter->GetControlRotation();
	_TargetMantleRot = FRotator(_StartMantleRot.Pitch, _TargetMantleRot.Yaw, _StartMantleRot.Roll);
	
	//Start movement
	SetComponentTickEnabled(bIsMantling);
}

void UPS_ParkourComponent::OnStoptMantle()
{
	if(bDebugMantle) UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__);

	_PlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	
	_PlayerController->SetIgnoreMoveInput(false);
	_PlayerController->SetIgnoreLookInput(false);
	
	_MantlePhase = EMantlePhase::NONE;
	bIsMantling = false;

	SetGenerateOverlapEvents(true);
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	
}

void UPS_ParkourComponent::MantleTick()
{	
	if(!IsValid(_PlayerCharacter)) return;
		
	if(!bIsMantling) return;

	const float alphaHeight = UKismetMathLibrary::MapRangeClamped(_TargetMantleSnapLoc.Z - _StartMantleLoc.Z, -_PlayerCharacter->GetCharacterMovement()->MaxStepHeight, _PlayerCharacter->GetCharacterMovement()->GetMaxJumpHeight(), 0.0f, 1.0f);
	const float dynSnapDuration = FMath::Lerp(MantleMinSnapDuration, MantlMaxSnapDuration,alphaHeight);
	
	const float alphaSnap= UKismetMathLibrary::MapRangeClamped(GetWorld()->GetTimeSeconds(), StartMantleSnapTimestamp, StartMantleSnapTimestamp + dynSnapDuration, 0,1);
		
	//1st Phase
	if(alphaSnap < 1)
	{
		_MantlePhase = EMantlePhase::SNAP;

		float curveAlphaSnap = alphaSnap;
		if(IsValid(MantleSnapCurve))
			curveAlphaSnap = MantleSnapCurve->GetFloatValue(alphaSnap);
		
		FVector newPlayerLoc = FMath::Lerp(_StartMantleLoc,_TargetMantleSnapLoc, curveAlphaSnap);

		_PlayerCharacter->SetActorLocation(newPlayerLoc);

		//Rotate player forward object
		_PlayerController->SetControlRotation(UKismetMathLibrary::RInterpTo(_StartMantleRot.Clamp(), _TargetMantleRot.Clamp(), GetWorld()->GetTimeSeconds() - StartMantleSnapTimestamp, MantleFacingRotationSpeed));
		
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

#pragma region Ledge
//------------------


void UPS_ParkourComponent::OnStartLedge(const FVector& targetLoc)
{
	if(bDebugLedge) UE_LOG(LogTemp, Warning, TEXT ("%S"), __FUNCTION__);
	
	if(!IsValid(GetWorld())) return;

	//TODO :: Change for Custom Move mode CMOVE_Climbing when add custom move mode
	_PlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_None);
	
	_StartLedgeLoc = _PlayerCharacter->GetActorLocation();
	_TargetLedgeLoc = targetLoc;
	//FVector landLoc = _PlayerCharacter->GetActorLocation();
	_TargetLedgeLoc.Z = targetLoc.Z + _PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() + MantleCapsuletHeightTestOffset;
	
	bIsLedging = true;
	StartLedgeTimestamp = GetWorld()->GetTimeSeconds();
	
	SetComponentTickEnabled(true);
	SetGenerateOverlapEvents(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if(bDebugLedge) DrawDebugPoint(GetWorld(), _TargetLedgeLoc, 20.f, FColor::Magenta, true);

}

void UPS_ParkourComponent::OnStopLedge()
{
	if(bDebugLedge) UE_LOG(LogTemp, Warning, TEXT ("%S"), __FUNCTION__);
	
	_PlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	SetGenerateOverlapEvents(true);
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	bIsLedging = false;

}

void UPS_ParkourComponent::LedgeTick()
{
	if(!IsValid(_PlayerCharacter)) return;
	
	if(!bIsLedging) return;

	const float alpha = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetTimeSeconds(),StartLedgeTimestamp, StartLedgeTimestamp + LedgeDuration, 0.0f, 1.0f);
	float curveAlpha = alpha;

	if(IsValid(LedgePullUpCurve))
	{
		curveAlpha = LedgePullUpCurve->GetFloatValue(alpha);
	}

	FVector newLoc = FMath::Lerp(_StartLedgeLoc,_TargetLedgeLoc, curveAlpha);
	_PlayerCharacter->SetActorLocation(newLoc);
	
	if(alpha >= 1.0f)
	{
		OnStopLedge();
	}
}

//------------------
#pragma endregion Ledge

#pragma region Event_Receiver
//------------------


void UPS_ParkourComponent::OnParkourDetectorBeginOverlapEventReceived(UPrimitiveComponent* overlappedComponent,
	AActor* otherActor, UPrimitiveComponent* otherComp, int otherBodyIndex, bool bFromSweep, const FHitResult& sweepResult)
{	
	if(!IsValid(_PlayerCharacter)
		|| !IsValid(GetWorld())
		|| !IsValid(_PlayerCharacter->GetCharacterMovement())
		|| !IsValid(_PlayerCharacter->GetFirstPersonCameraComponent())
		|| !IsValid(_PlayerCharacter->GetMesh())
		|| !IsValid(otherActor)) return;

	if(bIsMantling) return;

	FHitResult outHit;
	const TArray<AActor*> actorsToIgnore;

	//TODO :: Do this a func in characMovmeent
	//TODO :: Do this a func in characMovmeent
	const bool bIsInAir =  _PlayerCharacter->GetCharacterMovement()->IsFalling() || _PlayerCharacter->GetCharacterMovement()->IsFlying();
	
	FVector starLoc = _PlayerCharacter->GetActorLocation();
	starLoc.Z = (starLoc.Z - _PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()) + (bIsInAir ? 0.0f : _PlayerCharacter->GetCharacterMovement()->MaxStepHeight);
	
	UKismetSystemLibrary::LineTraceSingle(GetWorld(), starLoc, starLoc + GetForwardVector() * GetUnscaledCapsuleRadius() * 2, UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false, actorsToIgnore, bDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outHit, true);


	if(bDebug)UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__);
	
	//Force WallRun if lineTrace don't hit
	if(outHit.bBlockingHit)
	{
		//Check angle if use LineTrace
		const float angle = UKismetMathLibrary::DegAcos(_PlayerCharacter->GetActorForwardVector().Dot(outHit.Normal * -1));

		//Debug
		if(bDebug)
			if(outHit.bBlockingHit) UE_LOG(LogTemp, Log, TEXT("%S :: angle between hit and player forward %f"), __FUNCTION__ , angle);
	
		//Choose right parkour logic to execute
		if(angle > MaxMantleAngle || bIsWallRunning)
		{
			OnWallRunStart(otherActor);
		}
		else if(CanMantle(outHit))
		{
			OnStartMantle();
		}
	}
	else
	{
		OnWallRunStart(otherActor);
	}

}

void UPS_ParkourComponent::OnParkourDetectorEndOverlapEventReceived(UPrimitiveComponent* overlappedComponent,
                                                                    AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex)
{
	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__);

	if(bIsWallRunning && OverlappingComponents.IsEmpty())
	{
		if(bDebugWallRun) UE_LOG(LogTemp, Log, TEXT("WallRun Stop by Wall end"));
		OnWallRunStop();
	}

}

void UPS_ParkourComponent::OnMovementModeChangedEventReceived(ACharacter* character, EMovementMode prevMovementMode, uint8 previousCustomMode)
{
	if (!IsValid(_PlayerController)) return;
	
	//TODO :: Replace by CMOVE_Slide // use isInAIr from custom character movement
    const bool bForceUncrouch = prevMovementMode == MOVE_Walking && (character->GetCharacterMovement()->MovementMode == MOVE_Falling ||  character->GetCharacterMovement()->MovementMode == MOVE_Flying);
	const bool bForceCrouch = character->GetCharacterMovement()->MovementMode == MOVE_Walking && (prevMovementMode == MOVE_Falling || prevMovementMode == MOVE_Flying) && _PlayerController->IsCrouchInputTrigger();

	if(bForceUncrouch && bIsCrouched || bForceCrouch && !bIsCrouched)
	{
		if(bForceUncrouch && bIsCrouched) _PlayerController->SetIsCrouchInputTrigger(false);
		OnCrouch();
	}


}


//------------------
#pragma endregion Event_Receiver




