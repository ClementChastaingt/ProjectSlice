// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ParkourComponent.h"

#include "InputAction.h"
#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
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

	//-----CameraTilt smooth reset-----
	if(bIsResetingCameraTilt)
		CameraTilt(0, GetWorld()->GetTimeSeconds(), StartCameraTiltResetTimestamp);
	else
		SetComponentTickEnabled(false);

	
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
	CameraTilt(WallToPlayerDirection, WallRunSeconds, StartWallRunTimestamp);
}

void UPS_ParkourComponent::OnWallRunStart(AActor* otherActor)
{
	if(bIsWallRunning) return;
	
	//-----OnWallRunStart-----	
	//Activate Only if in Air
	if(!_PlayerCharacter->GetCharacterMovement()->IsFalling()) return;
	
	if(bDebug)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun start"));
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("StartWallRun")));
	}
	
	//WallRun Logic activation
	const UCameraComponent* playerCam = _PlayerCharacter->GetFirstPersonCameraComponent();
	const float angleObjectFwdToCamFwd = otherActor->GetActorForwardVector().Dot(playerCam->GetForwardVector());
	
	//TODO : Reactivate while check come from WallRun Jump off is ON
	// if(FMath::Abs(angleObjectFwdToCamFwd) < MinEnterAngle/10)
	// {
	// 	if(bDebug )UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun abort by WallDotPlayerCam angle %f "), angleObjectFwdToCamFwd * 10);
	// 	return;
	// }
	
	FRotator arrowRot = _PlayerCharacter->GetActorRotation();
	arrowRot.Yaw = (arrowRot.Yaw  + otherActor->GetActorRotation().Yaw) * FMath::Sign(angleObjectFwdToCamFwd);
	arrowRot.Vector().Normalize();
	UE_LOG(LogTemp, Error, TEXT("arrowRot.Yaw %f, angleObjectFwdToCamFwd %f, playerYaw %f, actorYaw %f"), arrowRot.Yaw, angleObjectFwdToCamFwd, _PlayerCharacter->GetActorRotation().Yaw, otherActor->GetActorRotation().Yaw);
	WallToPlayerDirection = FMath::Sign((otherActor->GetActorLocation() + otherActor->GetActorRightVector()).Dot(arrowRot.Vector().RightVector));
	WallRunDirection = otherActor->GetActorForwardVector() * FMath::Sign(angleObjectFwdToCamFwd);
	

	if(bDebugWallRun)
	{ 
		DrawDebugDirectionalArrow(GetWorld(), _PlayerCharacter->GetActorLocation() , _PlayerCharacter->GetActorLocation() + arrowRot.Vector() * 200, 5.0f, FColor::Black, false, 2, 10, 2);
		DrawDebugDirectionalArrow(GetWorld(), _PlayerCharacter->GetActorLocation() , _PlayerCharacter->GetActorLocation() + arrowRot.Vector().RightVector * 200, 5.0f, FColor::Emerald, false, 2, 10, 2);
		DrawDebugDirectionalArrow(GetWorld(), _PlayerCharacter->GetActorLocation() , _PlayerCharacter->GetActorLocation() + arrowRot.Vector().ForwardVector * 200, 5.0f, FColor::Orange, false, 2, 10, 2);
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
	const FVector jumpForce =  Wall->GetActorRightVector() * WallToPlayerDirection * JumpOffForceMultiplicator;
	if(bDebugWallRunJump) DrawDebugDirectionalArrow(GetWorld(), Wall->GetActorLocation(), Wall->GetActorLocation() + Wall->GetActorRightVector() * 200, 10.0f, FColor::Orange, false, 2, 10, 3);

	_PlayerCharacter->LaunchCharacter(jumpForce,false,false);	
}


//------------------
#pragma endregion WallRun


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

//------------------
#pragma endregion Event_Receiver




