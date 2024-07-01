// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ParkourComponent.h"

#include "InputAction.h"
#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
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

	//Link Event Receiver
	this->OnComponentBeginOverlap.AddUniqueDynamic(this,&UPS_ParkourComponent::OnParkourDetectorBeginOverlapEventReceived);
	this->OnComponentEndOverlap.AddUniqueDynamic(this,&UPS_ParkourComponent::OnParkourDetectorEndOverlapEventReceived);
	_PlayerCharacter->MovementModeChangedDelegate.AddUniqueDynamic(this,&UPS_ParkourComponent::OnMovementModeChangedEventReceived);
	
	//Custom Tick
	SetComponentTickEnabled(false);
	if (IsValid(GetWorld()))
	{
		FTimerDelegate floorTick_TimerDelegate;
		floorTick_TimerDelegate.BindUObject(this, &UPS_ParkourComponent::WallRunTick);
		GetWorld()->GetTimerManager().SetTimer(WallRunTimerHandle, floorTick_TimerDelegate, WallRunTickRate, true);
		GetWorld()->GetTimerManager().PauseTimer(WallRunTimerHandle);
	}
}


// Called every frame
void UPS_ParkourComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if(!bIsResetingCameraTilt && !bIsStooping)
		SetComponentTickEnabled(false);
		
	//-----Smooth crouching-----
	Stooping();

	//-----CameraTilt smooth reset-----
	if(bIsResetingCameraTilt)
		CameraTilt(0, GetWorld()->GetTimeSeconds(), StartCameraTiltResetTimestamp);
	
}


#pragma region WallRun
//------------------

void UPS_ParkourComponent::WallRunTick()
{
	//-----WallRunTick-----
	if(!bIsWallRunning || !IsValid(_PlayerCharacter) || !IsValid(_PlayerController)|| !IsValid(GetWorld())) return;
	
	//WallRun timer setup
	WallRunSeconds = WallRunSeconds + WallRunTickRate;
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
	const FVector newPlayerVelocity = customWallDirection * VelocityWeight * _PlayerController->GetMoveInput().Y;
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
	CameraTilt(CameraTiltOrientation, WallRunSeconds, StartWallRunTimestamp);
}

void UPS_ParkourComponent::OnWallRunStart(AActor* otherActor)
{
	UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun try start bIsWallRunning %i"), bIsWallRunning);
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

	if(bDebug)
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

	//Setup work var
	EnterVelocity = _PlayerCharacter->GetCharacterMovement()->GetLastUpdateVelocity().Length();
	StartCameraTiltRoll = _PlayerController->GetControlRotation().Roll;
	StartCameraTiltRoll = (360 + (StartCameraTiltRoll % 360)) % 360;
	Wall = otherActor;
	bForceWallRun = false;
	bIsWallRunning = true;
	
}

void UPS_ParkourComponent::OnWallRunStop()
{
	if(!bIsWallRunning) return;
	
	//-----OnWallRunStop-----
	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun stop"));
	
	GetWorld()->GetTimerManager().PauseTimer(WallRunTimerHandle);
	WallRunSeconds = 0.0f;

	//-----Reset Variables-----
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0,0,0));
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintEnabled(false);
	
	VelocityWeight = 1.0f;

	//----Camera Tilt Smooth reseting-----
	SetComponentTickEnabled(true);
	StartCameraTiltResetTimestamp = GetWorld()->GetTimeSeconds();
	StartCameraTiltRoll = _PlayerController->GetControlRotation().Roll;
	StartCameraTiltRoll = (360 + (StartCameraTiltRoll % 360)) % 360;
	bIsResetingCameraTilt = true;

	//----Stop WallRun && Movement falling-----
	_PlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	bIsWallRunning = false;
}

void UPS_ParkourComponent::CameraTilt(const int32 wallOrientationToPlayer, const float currentSeconds, const float startWallRunTimestamp)
{
	if(!IsValid(_PlayerController) || !IsValid(GetWorld())) return;
	
	//Alpha
	const float alphaTilt = UKismetMathLibrary::MapRangeClamped(currentSeconds, startWallRunTimestamp, startWallRunTimestamp + WallRunCameraTiltDuration, 0,1);
	
	//If Camera tilt already finished stop
	if(alphaTilt >= 1)
	{
		bIsResetingCameraTilt = false;
		return;
	}

	//Interp
	float curveTiltAlpha = alphaTilt;
	if(IsValid(WallRunCameraTiltCurve))
		curveTiltAlpha = WallRunCameraTiltCurve->GetFloatValue(alphaTilt);

	StartCameraTiltRoll =  StartCameraTiltRoll == 0 ? 360 : StartCameraTiltRoll;
	const int32 newRoll = FMath::Lerp(StartCameraTiltRoll, (StartCameraTiltRoll > 180 ? 360 : 0) + WallRunCameraAngle * wallOrientationToPlayer, curveTiltAlpha);
	const float newRollInRange = (360 + (newRoll % 360)) % 360;
	
	//Target Rot
	FRotator newControlRot = _PlayerController->GetControlRotation();
	newControlRot.Roll = newRollInRange;
	
	//Rotate
	_PlayerController->SetControlRotation(newControlRot);
	if(bDebugCameraTilt) UE_LOG(LogTemp, Log, TEXT("UTZParkourComp :: CurrentRoll: %f, alphaTilt %f"), _PlayerController->GetControlRotation().Roll, alphaTilt);
}

void UPS_ParkourComponent::JumpOffWallRun()
{
	if(!bIsWallRunning) return;

	if(!IsValid(Wall) || !IsValid(_PlayerCharacter)) return;

	if(bDebug)
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

#pragma region Crouch
//------------------


void UPS_ParkourComponent::OnCrouch()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetCapsuleComponent()) || !IsValid(_PlayerCharacter->GetCharacterMovement()) || !IsValid(GetWorld())) return;

	const ACharacter* DefaultCharacter = _PlayerCharacter->GetClass()->GetDefaultObject<ACharacter>();

	StartCrouchHeight = _PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	StartStoopTimestamp = GetWorld()->GetTimeSeconds();
		
	//Crouch
	if(!bIsCrouched && _PlayerCharacter->GetCharacterMovement() && _PlayerCharacter->GetCharacterMovement()->CanEverCrouch() && _PlayerCharacter->GetRootComponent() && !_PlayerCharacter->GetRootComponent()->IsSimulatingPhysics())
	{
		//Begin Slide if Velocity is enough high
		if(_PlayerCharacter->GetVelocity().Length() > CrouchingEnterMaxVelocity)
			OnStartSlide();
		
		// See if collision is already at desired size.
		if (_PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == _PlayerCharacter->GetCharacterMovement()->CrouchedHalfHeight) return;
				
		//Activate interp
		bIsCrouched = true;		
	}
	//Uncrouch
	//TODO :: Need to test if player have enough place for uncrouch 
	else
	{
		// See if collision is already at desired size.
		if(_PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight())return;

		//Activate interp
		bIsCrouched = false;
	}
	
	bIsStooping = true;
	SetComponentTickEnabled(bIsStooping);
	
	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("PS_Character :: %s"), bIsCrouched ? TEXT("Crouched") : TEXT("Uncrouched"));

}

void UPS_ParkourComponent::Stooping()
{
	if(!IsValid(_PlayerCharacter)) return;
	
	if(!bIsStooping) return;
	
	const float targetHeight = bIsCrouched ? _PlayerCharacter->GetCharacterMovement()->CrouchedHalfHeight : _PlayerCharacter->GetClass()->GetDefaultObject<ACharacter>()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float alpha = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetTimeSeconds(), StartStoopTimestamp, StartStoopTimestamp + SmoothCrouchDuration, 0,1);
	
	float curveAlpha = alpha;
	if(IsValid(CrouchCurve))
		curveAlpha = CrouchCurve->GetFloatValue(alpha);

	const float currentHeight = FMath::Lerp(StartCrouchHeight,targetHeight, curveAlpha);
	_PlayerCharacter->GetCapsuleComponent()->SetCapsuleHalfHeight(currentHeight, true);
	
	if(curveAlpha >= 1)
	{
		if(!bIsSliding) bIsCrouched ? _PlayerCharacter->Crouch() : _PlayerCharacter->UnCrouch();
		bIsStooping = false;
	}
}

//------------------
#pragma endregion Crouch

#pragma region Slide
//------------------


void UPS_ParkourComponent::OnStartSlide()
{
	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("PS_Character :: Slide"));
}


void UPS_ParkourComponent::OnStopSlide()
{
}

void UPS_ParkourComponent::OnSlide()
{
	
}

void UPS_ParkourComponent::CalculateFloorInflucence()
{
	
}



//------------------
#pragma endregion Slide


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
	
	OnWallRunStart(otherActor);
		
}

void UPS_ParkourComponent::OnParkourDetectorEndOverlapEventReceived(UPrimitiveComponent* overlappedComponent,
                                                                    AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex)
{
	if(bDebug && bIsWallRunning)UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun Stop by Wall end"));
	OnWallRunStop();
}

void UPS_ParkourComponent::OnMovementModeChangedEventReceived(ACharacter* character, EMovementMode prevMovementMode,
	uint8 previousCustomMode)
{
    bComeFromAir = prevMovementMode == MOVE_Falling || prevMovementMode == MOVE_Flying;
}

//------------------
#pragma endregion Event_Receiver




