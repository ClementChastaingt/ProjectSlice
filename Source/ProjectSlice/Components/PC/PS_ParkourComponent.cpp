// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ParkourComponent.h"

#include "PS_PlayerCameraComponent.h"
#include "Field/FieldSystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Character/PC/PS_Character.h"
#include "ProjectSlice/Data/PS_Constants.h"
#include "ProjectSlice/Data/PS_GlobalType.h"
#include "ProjectSlice/FunctionLibrary/PSFl.h"


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
	
	_DefaulGroundFriction = _PlayerCharacter->GetCharacterMovement()->GroundFriction;
	_DefaultBrakingDecelerationWalking = _PlayerCharacter->GetCharacterMovement()->BrakingDecelerationWalking;
	_DefaultBrakingDecelerationFalling = _PlayerCharacter->GetCharacterMovement()->BrakingDecelerationFalling;
	
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
		GetWorld()->GetTimerManager().SetTimer(_WallRunTimerHandle, wallRunTick_TimerDelegate, CustomTickRate, true);
		GetWorld()->GetTimerManager().PauseTimer(_WallRunTimerHandle);

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


void UPS_ParkourComponent::ToggleObstacleLockConstraint(const AActor* const otherActor, UPrimitiveComponent* const otherComp,
	const bool bActivate) const
{
	if(otherActor->ActorHasTag(TAG_SLICEABLE))
	{
		UMeshComponent* objectOverlap = Cast<UMeshComponent>(otherComp);
		if(!IsValid(objectOverlap))
		{
			UE_LOG(LogTemp, Error, TEXT("%S :: objectOverlap invalid"), __FUNCTION__);
			return;
		}
		
		//Trigger or not player physic
		_PlayerCharacter->GetCapsuleComponent()->SetCollisionEnabled(bActivate ? ECollisionEnabled::QueryAndPhysics :  ECollisionEnabled::QueryOnly);
		_PlayerCharacter->GetMesh()->SetCollisionEnabled(bActivate ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::QueryOnly);

		// if(objectOverlap->IsSimulatingPhysics())
		// {
		// 	objectOverlap->BodyInstance.bLockRotation = !bActivate;
		// 	objectOverlap->BodyInstance.bLockTranslation = !bActivate;
		//
		// 	UE_LOG(LogTemp, Error, TEXT("LockConst, %s: bLockTranslation %i, bLockRotation "), *GetNameSafe(objectOverlap), objectOverlap->BodyInstance.bLockTranslation);
		// 	
		// }

		//Unhook if currently used
		if(IsValid(_PlayerCharacter->GetHookComponent()))
		{
			if(_PlayerCharacter->GetHookComponent()->IsObjectHooked() && !bActivate)
			{
				_PlayerCharacter->GetHookComponent()->DettachHook();
			}
		}
		
		//Reset overlap actor 
		if(IsValid(_ComponentOverlap))
		{
			if(!bActivate)
			{
				_ComponentOverlap->SetPhysicsLinearVelocity(FVector::ZeroVector);
			}
		}

	}
	return;
}

#pragma region WallRun
//------------------

bool UPS_ParkourComponent::FindWallOrientationFromPlayer(int32& playerToWallOrientation)
{
	FHitResult outHitRight, outHitLeft;
	const TArray<AActor*> actorsToIgnore= {_PlayerCharacter};
	const float radius = 15.0f;
	
	UKismetSystemLibrary::SphereTraceSingle(GetWorld(), _PlayerCharacter->GetActorLocation(), _PlayerCharacter->GetActorLocation() + _PlayerCharacter->GetActorRightVector() * 200, radius,  UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false, actorsToIgnore, bDebugWallRun ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outHitRight, true, FColor::Blue);

	UKismetSystemLibrary::SphereTraceSingle(GetWorld(), _PlayerCharacter->GetActorLocation(), _PlayerCharacter->GetActorLocation() - _PlayerCharacter->GetActorRightVector() * 200, radius, UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false, actorsToIgnore, bDebugWallRun ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outHitLeft, true, FColor::Green);

	playerToWallOrientation = 0;
	bool hitRight = outHitRight.bBlockingHit;
	bool hitLeft = outHitLeft.bBlockingHit;

	//If hit left && right choose the nearest
	if(hitRight && hitLeft)
	{
		if (outHitRight.Distance < outHitLeft.Distance)
		{
			playerToWallOrientation = 1;
		}else
		{
			playerToWallOrientation = -1;
		}
	}
	else if (hitRight && !hitLeft)
	{
		playerToWallOrientation = 1;
	}
	else if (hitLeft && !hitRight)
	{
		playerToWallOrientation = -1;
	}
	else if(!hitRight && !hitLeft || playerToWallOrientation == 0)
	{
		if(bDebugWallRun) UE_LOG(LogTemp, Error, TEXT("%S :: player facing the new wall, stop"), __FUNCTION__);
		OnWallRunStop();
		return false;
	}
	_WallRunSideHitResult = playerToWallOrientation < 0 ? outHitLeft : outHitRight;

	if (bDebugWallRun) UE_LOG(LogTemp, Log, TEXT("%S :: playerToWallOrientation %s"),__FUNCTION__, playerToWallOrientation > 0 ? TEXT("Right") : TEXT("Left") );

	return true;
}

void UPS_ParkourComponent::TryStartWallRun(AActor* const otherActor,const FHitResult& fwdHit)
{
	if(bIsCrouched
		|| bIsLedging
		|| bIsMantling
		|| _PlayerCharacter->GetHookComponent()->IsPlayerSwinging()) return;
	
	//Activate Only if in Air
	if(!_PlayerCharacter->GetCharacterMovement()->IsFalling() && !_PlayerCharacter->GetCharacterMovement()->IsFlying() && !_PlayerCharacter->bWasJumping) return;

	//Check distance to floor
	FFindFloorResult floorResult;
	_PlayerCharacter->GetCharacterMovement()->FindFloor(_PlayerCharacter->GetActorLocation(), floorResult , true);
	if (floorResult.bBlockingHit && floorResult.GetDistanceToFloor() < _PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 1.5f) return;
	
	//Prevent from trigger in loop by encounter new wall
	if (_PlayerCharacter->JumpCurrentCount == _JumpCountOnWallRunning && !_bIsWallRunning) return;
	
	if(bDebugWallRun) UE_LOG(LogTemp, Log, TEXT("%S"), __FUNCTION__);
	
	//Find WallOrientation from player
	int32 playerToWallOrientation;
	if (!FindWallOrientationFromPlayer(playerToWallOrientation)) return;

	//Update only if don't come from WallRun && Check if player is facing the wall
	if(!_bIsWallRunning)
	{
		_PlayerToWallOrientation = playerToWallOrientation;
	}
	else if(_PlayerToWallOrientation != playerToWallOrientation)
	{
		if(bDebugWallRun) UE_LOG(LogTemp, Error, TEXT("%S :: player facing the last wall, stop"), __FUNCTION__);
		OnWallRunStop();
		return;
	}
	_CameraTiltOrientation = _PlayerToWallOrientation;
	
	//Determine Direction
	//_WallRunDirection = DetermineWallRunDirection(otherActor, fwdHit, _WallRunSideHitResult);
	_WallRunDirection = UKismetMathLibrary::GetDirectionUnitVector(_WallRunSideHitResult.Location, fwdHit.Location);
	_WallRunDirection.Z = 0.0f;
	
	if(bDebugWallRun)
	{
		DrawDebugDirectionalArrow(GetWorld(), _PlayerCharacter->GetActorLocation() , _PlayerCharacter->GetActorLocation() + _PlayerCharacter->GetActorForwardVector() * 200, 5.0f, FColor::White, false, 2, 10, 2);
		DrawDebugDirectionalArrow(GetWorld(), _PlayerCharacter->GetActorLocation() , _PlayerCharacter->GetActorLocation() + _WallRunDirection * 200, 5.0f, FColor::Yellow, false, 2, 10, 2);
	}

	//Constraint init
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintEnabled(true);
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0,0,0));

	//Timer init
	if(!_bIsWallRunning)
	{
		_StartWallRunTimestamp = GetWorld()->GetTimeSeconds();
		_WallRunSeconds = _StartWallRunTimestamp;
		GetWorld()->GetTimerManager().UnPauseTimer(_WallRunTimerHandle);
	}

	//Camera_Tilt Setup
	_PlayerCharacter->GetFirstPersonCameraComponent()->SetupCameraTilt(false, _CameraTiltOrientation * -1);
	
	//Setup work var
	_WallRunEnterVelocity = _PlayerCharacter->GetCharacterMovement()->GetLastUpdateVelocity();	
	_Wall = otherActor;
	_bIsWallRunning = true;
	_JumpCountOnWallRunning = _PlayerCharacter->JumpCurrentCount;

	//Feedback 
	if(IsValid(_PlayerCharacter->GetProceduralAnimComponent()))
		_PlayerCharacter->GetProceduralAnimComponent()->StartWalkingAnim();

}

void UPS_ParkourComponent::OnWallRunStop()
{
	if(!_bIsWallRunning) return;

	if(bDebugWallRun) UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun stop"));

	//Reset timer
	GetWorld()->GetTimerManager().PauseTimer(_WallRunTimerHandle);
	_WallRunSeconds = 0.0f;

	//Reset Var && Disable Plane constraint
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintEnabled(false);

	//Reset Camera_Tilt
	_PlayerCharacter->GetFirstPersonCameraComponent()->SetupCameraTilt(true);
	_PlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Falling);

	//Reset obstacle lock
	ToggleObstacleLockConstraint(_ActorOverlap, _ComponentOverlap, true);

	//Reset Variables
	_VelocityWeight = 1.0f;
	_JumpCountOnWallRunning = 0;
	_bIsWallRunning = false;
}

void UPS_ParkourComponent::WallRunTick()
{
	if(!_bIsWallRunning || !IsValid(_PlayerCharacter) || !IsValid(_PlayerController)|| !IsValid(GetWorld())) return;

	//Update PlaneConstraint for follow correctly wall
	FHitResult outPlaneConstHit;
	const TArray<AActor*> actorsToIgnore = {_PlayerCharacter};
	const FVector start = _PlayerCharacter->GetActorLocation();
	const FVector end = start + (_PlayerToWallOrientation * GetRightVector() * 200);
	const float radius = 40.0f;
	
	UKismetSystemLibrary::SphereTraceSingle(GetWorld(), start, end, radius, UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false, actorsToIgnore, bDebugWallRun ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outPlaneConstHit, true);
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintNormal(outPlaneConstHit.bBlockingHit && outPlaneConstHit.GetActor() == _Wall ? outPlaneConstHit.Normal : FVector(0,0,0));
	
	//WallRun timer 
	_WallRunSeconds = _WallRunSeconds + CustomTickRate;
	const float endTimestamp = _StartWallRunTimestamp + WallRunTimeToFall;
	const float alphaWallRun = UKismetMathLibrary::MapRangeClamped(_WallRunSeconds, _StartWallRunTimestamp, endTimestamp, 0,1);

	//Velocity Weight
	//Setup alpha use for Force interp
	float curveForceAlpha = alphaWallRun;
	if(IsValid(WallRunGravityCurve))
		curveForceAlpha = WallRunForceCurve->GetFloatValue(alphaWallRun);
	
	//Clamp Max Velocity
	FVector wallRunVel = _WallRunEnterVelocity + WallRunSpeedBoost;
	wallRunVel = UPSFl::ClampVelocity(_WallRunEnterVelocity, _PlayerCharacter->GetVelocity(),wallRunVel,_PlayerCharacter->GetDefaultMaxWalkSpeed() * MaxWallRunSpeedMultiplicator);

	const float angleCamToWall = _PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector().Dot(_WallRunDirection);
	const float cameraOrientationWeight = FMath::Clamp(FMath::Abs(angleCamToWall), 0.0f, 1.0f);
	
	float targetWallRunVel = FMath::Lerp(_WallRunEnterVelocity.Length() / 2.0, wallRunVel.Length(),cameraOrientationWeight);

	//WallRunning vel impacted by cam angle
	if(WallRunMaxCamOrientationAngle < UKismetMathLibrary::DegAcos(angleCamToWall) && angleCamToWall < 0)
	{
		targetWallRunVel -= FMath::Lerp(0.0f, wallRunVel.Length(),cameraOrientationWeight);
	};
	_VelocityWeight = FMath::Lerp(_WallRunEnterVelocity.Length(), targetWallRunVel,curveForceAlpha);
	
	//Fake Gravity Velocity
	float curveGravityAlpha = alphaWallRun;
	if(IsValid(WallRunGravityCurve))
		curveGravityAlpha = WallRunGravityCurve->GetFloatValue(alphaWallRun);
	
	FVector customWallDirection = _WallRunDirection;
	customWallDirection.Z = FMath::Lerp(_WallRunDirection.Z, _WallRunDirection.Z + 0.5,curveGravityAlpha);
	customWallDirection.Normalize();

	//Velocity impact by input weight
	const float inputWeight = _PlayerController->GetMoveInput().Y > 0.0 ? _PlayerController->GetMoveInput().Y : WallRunDefaultInputWeight;
	const FVector newPlayerVelocity = customWallDirection * _VelocityWeight * inputWeight;
	_PlayerCharacter->GetCharacterMovement()->Velocity = newPlayerVelocity;

	//Determine vertical only vel
	FVector verticalVel = newPlayerVelocity;
	verticalVel.Z = 0.0f;
	
	//Stop WallRun if he was too long OR vel is under MinWallRunVelocityThreshold
	if(alphaWallRun >= 1 || verticalVel.IsNearlyZero(MinWallRunVelocityThreshold))
	{
		if(bDebugWallRun)UE_LOG(LogTemp, Log, TEXT("UTZParkourComp :: WallRun Stop by Velocity"));
		OnWallRunStop();
	}
		
	//Debug
	if(bDebugWallRun) UE_LOG(LogTemp, Log, TEXT("UTZParkourComp :: WallRun alpha %f,  _WallRunEnterVelocity %f, VelocityWeight %f"),alphaWallRun, _WallRunEnterVelocity.Length(), _VelocityWeight);
	
}

void UPS_ParkourComponent::JumpOffWallRun()
{
	if(!_bIsWallRunning) return;

	if(!IsValid(_Wall) || !IsValid(_PlayerCharacter)) return;

	if(bDebugWallRunJump)
	{
		UE_LOG(LogTemp, Warning, TEXT("%S:: WallRun jump off"), __FUNCTION__);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("WallRun Jump Off")));
	}

	OnWallRunStop();

	FHitResult outHitFwd = _PlayerCharacter->GetWeaponComponent()->GetSightHitResult();
	//const float angleSigthToWallBack = _PlayerCharacter->GetWeaponComponent()->GetSightMeshComponent()->GetForwardVector().Dot(WallRunDirection);
	bool bIsARightDirJumpOff = ((outHitFwd.bBlockingHit && IsValid(outHitFwd.GetActor()) && outHitFwd.GetActor() == _Wall) /*|| FMath::Abs(UKismetMathLibrary::DegAcos(angleSigthToWallBack)) < 6.5f*/);

	DrawDebugLine(GetWorld(), _PlayerCharacter->GetFirstPersonCameraComponent()->GetComponentLocation(), _PlayerCharacter->GetFirstPersonCameraComponent()->GetComponentLocation() + _WallRunDirection * 600.0f, FColor::Yellow, false, 2, 10, 3);
	
	//Determine Target Roll
	const float angleCamToWall = _PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector().Dot(_WallRunDirection);
	
	FVector jumpDir = bIsARightDirJumpOff ? _PlayerCharacter->GetActorRightVector() * -_PlayerToWallOrientation * FMath::Sign(angleCamToWall) : _PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector();
	jumpDir.Normalize();

	//Weight Force when sight is up
	const float angleCamToWallUp = _PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector().Dot(_WallRunDirection.UpVector);
	float jumpOffSpeed = JumpOffForceSpeed;
	if(angleCamToWallUp >= 0)
		jumpOffSpeed = UKismetMathLibrary::MapRangeClamped(angleCamToWallUp,0.0f,1.0f,JumpOffForceSpeed,0.0f);
	
	//Calc jumpForce
	const FVector jumpForce = jumpDir * (_PlayerCharacter->GetDefaultMaxWalkSpeed() + jumpOffSpeed);

	//Debug
	if(bDebugWallRunJump)
	{
		UE_LOG(LogTemp, Log, TEXT("%S :: jumpDir use %s"),__FUNCTION__, !bIsARightDirJumpOff ? TEXT("Weapon Forward") : TEXT("Wall Right"))
		DrawDebugDirectionalArrow(GetWorld(), _PlayerCharacter->GetActorLocation(),_PlayerCharacter->GetActorLocation() + jumpDir * (_PlayerCharacter->GetDefaultMaxWalkSpeed() + JumpOffForceSpeed), 10.0f, FColor::Orange, false, 2, 10, 3);
	}
	
	//Clamp Max Velocity
	//FVector jumpTargetVel = UPSFl::ClampVelocity(jumpForce,jumpForce,_PlayerCharacter->GetDefaultMaxWalkSpeed() + MaxWallRunSpeedMultiplicator);

	//Launch chara
	_PlayerCharacter->GetCharacterMovement()->AddImpulse(jumpForce,true);
	_PlayerCharacter->OnJumped();
	
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
		if(_bIsSliding) OnStopSlide();

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
	
	_bIsSliding = true;
	StartSlideTimestamp = GetWorld()->GetTimeSeconds();
	SlideSeconds = StartSlideTimestamp;
	
	//--------Configure Movement Behaviour-------
	_PlayerController->SetIgnoreMoveInput(true);
	SlideDirection = _PlayerCharacter->GetCharacterMovement()->GetLastInputVector() * _PlayerCharacter->GetCharacterMovement()->GetLastUpdateVelocity().Length(); 
	_PlayerCharacter->GetCharacterMovement()->GroundFriction = 0.0f;
	
	GetWorld()->GetTimerManager().UnPauseTimer(SlideTimerHandle);

	OnSlideEvent.Broadcast(_bIsSliding);
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
	_PlayerCharacter->GetCharacterMovement()->GroundFriction = _DefaulGroundFriction;
	_PlayerCharacter->GetCharacterMovement()->BrakingDecelerationFalling = _DefaultBrakingDecelerationFalling;

	_bIsSliding = false;
	if(bIsCrouched) _PlayerCharacter->Crouching();

	OnSlideEvent.Broadcast(_bIsSliding);
	
}

void UPS_ParkourComponent::SlideTick()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetCharacterMovement()) || !IsValid(GetWorld())) return;
	
	if(!_bIsSliding) return;

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

	if(bDebugWallRun) DrawDebugDirectionalArrow(GetWorld(), GetComponentLocation(), GetComponentLocation() + out * 500, 5.0f, FColor::Blue, false, 2, 10, 2);
	
	return out;
}



//------------------
#pragma endregion Slide

#pragma region Dash
//------------------

void UPS_ParkourComponent::OnDash()
{
	if(!IsValid(_PlayerCharacter->GetHookComponent())) return;
	
	if( bIsLedging || bIsMantling || bIsCrouched) return;

	if(GetWorld()->GetTimerManager().IsTimerActive(_DashResetTimerHandle)) return;

	if(_bIsWallRunning)
		OnWallRunStop();

	if(_bIsSliding)
		OnStopSlide();

	
	FVector dashDir = UPSFl::GetWorldInputDirection(_PlayerCharacter->GetFirstPersonCameraComponent(), _PlayerController->GetMoveInput());
	_DashFeedbackDir = _PlayerController->GetMoveInput();
	if(dashDir.IsNearlyZero())
	{
		_DashFeedbackDir = FVector2D(0.0f,1.0f);
		dashDir = _PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector();
		dashDir.Z = 0;
	};
	dashDir.Normalize();
	
	FVector dashVel = dashDir * (_PlayerCharacter->GetDefaultMaxWalkSpeed() + DashSpeed);
	// dashVel = bIsOnGround ? dashVel * 3 : (_PlayerCharacter->GetHookComponent()->IsPlayerSwinging() ? (dashVel / 2) : dashVel);

	//Change chara movement params
	switch (_PlayerCharacter->GetCharacterMovement()->MovementMode)
	{
		case MOVE_Walking:
			_PlayerCharacter->GetCharacterMovement()->GroundFriction = DashGroundFriction;
			break;
		case MOVE_Falling:
			_PlayerCharacter->GetCharacterMovement()->BrakingDecelerationFalling = _PlayerCharacter->GetCharacterMovement()->BrakingDecelerationWalking;
			break;
		default:
			break;
	}

	//Launch character
	EDashType DashType = EDashType::STANDARD;
	if(_PlayerCharacter->GetVelocity().Length() < _PlayerCharacter->GetDefaultMaxWalkSpeed() + DashSpeed)
	{
		DashType = EDashType::STANDARD;
		_PlayerCharacter->LaunchCharacter(dashVel, false, false);
	}

	if(_PlayerCharacter->GetHookComponent()->IsPlayerSwinging() && _PlayerCharacter->GetHookComponent()->GetConstraintAttachSlave()->GetComponentVelocity().Length() < _PlayerCharacter->GetDefaultMaxWalkSpeed() + DashSpeed)
	{
		DashType = EDashType::SWING;
		_PlayerCharacter->GetHookComponent()->GetConstraintAttachSlave()->AddImpulse(dashVel,NAME_None, true);
	}
	
	//Clamp Max Velocity
	_PlayerCharacter->GetCharacterMovement()->Velocity = UPSFl::ClampVelocity(_PlayerCharacter->GetVelocity(), dashDir * (_PlayerCharacter->GetDefaultMaxWalkSpeed() + DashSpeed),_PlayerCharacter->GetDefaultMaxWalkSpeed() + DashSpeed);

	//Trigger PostProcess Feedback
	_PlayerCharacter->GetFirstPersonCameraComponent()->TriggerDash(true);
	
	if(bDebugDash)UE_LOG(LogTemp, Warning, TEXT("%S :: dashType: %s, dashVel %s, dashDir %s"), __FUNCTION__, *UEnum::GetValueAsString(DashType), *dashVel.ToString(), *dashDir.ToString());
	
	FTimerDelegate dashReset_TimerDelegate;
	dashReset_TimerDelegate.BindUObject(this, &UPS_ParkourComponent::ResetDash);
	GetWorld()->GetTimerManager().SetTimer(_DashResetTimerHandle, dashReset_TimerDelegate, DashDuration, false);

	// FTimerDelegate dashCooldown_TimerDelegate;
	// dashCooldown_TimerDelegate.BindUObject(this, &UPS_ParkourComponent::ResetDash);
	// GetWorld()->GetTimerManager().SetTimer(_DashCooldownTimerHandle, dashCooldown_TimerDelegate, DashCooldown, false);

	//Stop Walk feedback on dash
	if(IsValid(_PlayerCharacter->GetProceduralAnimComponent()))
		_PlayerCharacter->GetProceduralAnimComponent()->StopWalkingAnim();

	//Activate dash and broadcast del
	_bIsDashing = true;
	OnDashEvent.Broadcast();
}

void UPS_ParkourComponent::ResetDash()
{
	_PlayerCharacter->GetCharacterMovement()->GroundFriction = _DefaulGroundFriction;

	_bIsDashing = false;

	//Restart walk 
	if(IsValid(_PlayerCharacter->GetProceduralAnimComponent()))
		_PlayerCharacter->GetProceduralAnimComponent()->StartWalkingAnimWithDelay(0.2);
	
	OnResetDashEvent.Broadcast();
}

//------------------
#pragma endregion Dash

#pragma region Mantle
//------------------

bool UPS_ParkourComponent::CanMantle(const FHitResult& inFwdHit)
{
	if(!IsValid(GetWorld())) return false;

	if(bIsCrouched) OnCrouch();
	
	FHitResult outHitHgt, outCapsHit;
	const TArray<AActor*> actorsToIgnore= {_PlayerCharacter};
	const float capsuleOffset = _PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius() / 2;

	//Height Trace
	FVector startHgt = inFwdHit.Location + inFwdHit.Normal * -1 * capsuleOffset;
	startHgt.Z = startHgt.Z + MaxMantleHeight;
	FVector endHgt = inFwdHit.Location + inFwdHit.Normal * -1 * capsuleOffset;
		
	UKismetSystemLibrary::LineTraceSingle(GetWorld(),startHgt,endHgt,UEngineTypes::ConvertToTraceType(ECC_Visibility), false,actorsToIgnore, bDebugMantle ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outHitHgt, true, FColor::Orange);
	
	if(!outHitHgt.bBlockingHit || outHitHgt.bStartPenetrating) return false;
	
	const bool bIsInAir = _PlayerCharacter->IsInAir();
	
	//If try by meet edge check if not too low
	const bool bComeFromAirAndTooLowToEdge = bIsInAir && outHitHgt.Location.Z - _PlayerCharacter->GetActorLocation().Z > _PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * OnAiMaxCapsHeightMultiplicator;
	if(bComeFromAirAndTooLowToEdge)
	{
		if(bDebugMantle) UE_LOG(LogTemp, Warning, TEXT("%S :: bComeFromAirAndTooLowToEdge true"), __FUNCTION__);
		return false;
	}
	
	//If come from Jump who can pass without Mantle OR pull up on a stair step
	const FVector footPlacement = _PlayerCharacter->GetFootPlacementLoc();
	const bool bPlayerUpperThanTarget = footPlacement.Z + (bIsInAir ? 0.0f : _PlayerCharacter->GetCharacterMovement()->MaxStepHeight) < outHitHgt.Location.Z  && outHitHgt.Location.Z  < _PlayerCharacter->GetActorLocation().Z;
	
	if(bDebugMantle) DrawDebugPoint(GetWorld(), footPlacement, 20.f, FColor::Green, true);
		
	//Force Landing by Ledge
	if(bPlayerUpperThanTarget && !_PlayerCharacter->GetCharacterMovement()->bPerformingJumpOff)
	{
		OnStartLedge(outHitHgt.Location);
		return false;
	}
	
	//Can't Mantle if not in air
	if(!_PlayerCharacter->GetCharacterMovement()->IsFalling() && !_PlayerCharacter->GetCharacterMovement()->IsFlying()) return false;
	
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

	//Callback
	OnTriggerMantleEvent.Broadcast(true);
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

	//--------Reset Collision--------
	ToggleObstacleLockConstraint(_ActorOverlap, _ComponentOverlap, true);

	//Callback
	OnTriggerMantleEvent.Broadcast(false);
	
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
		//For anim
		if(_MantlePhase != EMantlePhase::PULL_UP)
		{
			_MantlePhase = EMantlePhase::PULL_UP;
			OnPullUpMantleEvent.Broadcast();
		}

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
	_PlayerCharacter->GetCharacterMovement()->DisableMovement();
	
	_StartLedgeLoc = _PlayerCharacter->GetActorLocation();
	_TargetLedgeLoc = targetLoc;
	//FVector landLoc = _PlayerCharacter->GetActorLocation();
	_TargetLedgeLoc.Z = targetLoc.Z + _PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	
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

	//--------Reset Collision--------
	ToggleObstacleLockConstraint(_ActorOverlap, _ComponentOverlap, true);

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
	//Basic check 
	if(!IsValid(_PlayerCharacter)
		|| !IsValid(GetWorld())
		|| !IsValid(_PlayerCharacter->GetCharacterMovement())
		|| !IsValid(_PlayerCharacter->GetFirstPersonCameraComponent())
		|| !IsValid(_PlayerCharacter->GetMesh())
		|| !IsValid(otherActor)
		|| otherActor->ActorHasTag(TAG_UNPARKOURABLE)
		|| !_PlayerCharacter->GetCharacterMovement()->IsFalling()) return;

	//If already mantling or ledging exit
	if(bIsMantling || bIsLedging)  return;

	//If encounter Chaos system exit
	if (otherActor->IsA(AGeometryCollectionActor::StaticClass()) || otherActor->IsA(AFieldSystemActor::StaticClass())) return;

	//Trace for get hit infos
	FHitResult outHit;
	const TArray<AActor*> actorsToIgnore;
	
	FVector starLoc = _PlayerCharacter->GetActorLocation();
	starLoc.Z = (starLoc.Z - _PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()) + (_PlayerCharacter->IsInAir() ? 0.0f : _PlayerCharacter->GetCharacterMovement()->MaxStepHeight);

	UKismetSystemLibrary::LineTraceSingle(GetWorld(), starLoc, starLoc + GetForwardVector() * 1000, UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false, actorsToIgnore, bDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, outHit, true);

	//If don't block or block other exit
	if(!outHit.bBlockingHit
		|| outHit.GetActor() != otherActor
		|| !IsValid(outHit.GetActor())
		|| !IsValid(otherComp))
		return;

	//Setup work var
	_ActorOverlap = otherActor;
	_ComponentOverlap = otherComp;

	if(bDebug)UE_LOG(LogTemp, Log, TEXT("%S :: overlap %s"), __FUNCTION__, *_ComponentOverlap->GetReadableName());
	
	//Check angle if use LineTrace
	const float angle = UKismetMathLibrary::DegAcos(_PlayerCharacter->GetActorForwardVector().Dot(outHit.Normal * -1));

	//Block collision with GPE if try to parkour on hit 
	ToggleObstacleLockConstraint(otherActor, otherComp, false);

	//Debug
	if (bDebug)
	{
		UE_LOG(LogTemp, Log, TEXT("%S :: angle between hit and player forward %f"), __FUNCTION__, angle);
	}

	//Try start Mantle OR Ledge
	if (angle <= MaxMantleAngle && !_bIsWallRunning /*&& !GetWorld()->GetTimerManager().IsTimerActive(_PlayerCharacter->GetCoyoteTimerHandle())*/)
	{
		if (CanMantle(outHit)) OnStartMantle();
	}
	
	//Try start WallRun
	TryStartWallRun(otherActor, outHit);
	
	//If can't parkour reactivate physic
	if(!_bIsWallRunning && !bIsLedging && !bIsMantling)
	{
		ToggleObstacleLockConstraint(_ActorOverlap, _ComponentOverlap, true);
	}


}

void UPS_ParkourComponent::OnParkourDetectorEndOverlapEventReceived(UPrimitiveComponent* overlappedComponent,
                                                                    AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex)
{
	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("%S"), __FUNCTION__);
	
	if(_bIsWallRunning && OverlappingComponents.IsEmpty())
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

	if(previousCustomMode == MOVE_Falling && character->GetCharacterMovement()->MovementMode == MOVE_Walking)
	{
		if(GetWorld()->GetTimerManager().IsTimerActive(_DashResetTimerHandle))
			_PlayerCharacter->GetCharacterMovement()->GroundFriction = DashGroundFriction;
		
		_PlayerCharacter->GetCharacterMovement()->BrakingDecelerationFalling = _DefaultBrakingDecelerationFalling;
	}
	if(previousCustomMode == MOVE_Walking && character->GetCharacterMovement()->MovementMode == MOVE_Falling)
	{
		_PlayerCharacter->GetCharacterMovement()->GroundFriction = _DefaulGroundFriction;
	}


}


//------------------
#pragma endregion Event_Receiver




