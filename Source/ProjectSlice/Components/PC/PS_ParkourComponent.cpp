// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ParkourComponent.h"

#include "InputAction.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/PC/PS_Character.h"


// Sets default values for this component's properties
UPS_ParkourComponent::UPS_ParkourComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
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

	//Init Default var	
	DefaultGravity = _PlayerCharacter->GetCharacterMovement()->GravityScale;

	//Custom Tick
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
	
}


#pragma region WallRun
//------------------

void UPS_ParkourComponent::WallRunTick()
{
	//-----WallRunTick-----
	if(!bIsWallRunning || !IsValid(_PlayerCharacter) || !IsValid(_PlayerController)|| !IsValid(GetWorld())) return;

	//WallRun timer setup
	WallRunSeconds = WallRunSeconds + WallRunTickRate;
	const float endTimestamp = StartWallRunTimestamp + WallRunTimeToMaxGravity + WallRunTimeToFall;
	const float alphaMaxGravity = UKismetMathLibrary::MapRangeClamped(WallRunSeconds, StartWallRunTimestamp, StartWallRunTimestamp + WallRunTimeToMaxGravity, 0,1);
	const float alphaWallRun = UKismetMathLibrary::MapRangeClamped(WallRunSeconds, StartWallRunTimestamp, endTimestamp, 0,1);

	//-----Stuck to Wall Velocity-----
	//Setup alpha use for Force interp
	float curveForceAlpha = alphaWallRun;
	if(IsValid(WallRunGravityCurve))
		curveForceAlpha = WallRunForceCurve->GetFloatValue(alphaWallRun);
	
	VelocityWeight = FMath::Lerp(EnterVelocity, EnterVelocity + WallRunForceBoost,curveForceAlpha);
	UE_LOG(LogTemp, Error, TEXT("_PlayerController->GetMoveInput().X %f"), _PlayerController->GetMoveInput().Y);
	const FVector newPlayerVelocity = WallRunDirection * VelocityWeight * _PlayerController->GetMoveInput().Y;
	_PlayerCharacter->GetCharacterMovement()->Velocity = newPlayerVelocity;

	
	//Stop WallRun if he was too long
	if(alphaWallRun > 1 || newPlayerVelocity.IsNearlyZero(0.01))
	{
		if(bDebug)UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun Stop by Velocity"));
		OnWallRunStop();
	}
	
	//Debug
	if(bDebugTick) UE_LOG(LogTemp, Log, TEXT("UTZParkourComp :: WallRun EnterVelocity %f, VelocityWeight %f"),EnterVelocity, VelocityWeight);

	//TODO:: Change by Wallrun Z movement
	//-----Gravity interp-----
	float curveGravityAlpha = alphaMaxGravity;
	if(IsValid(WallRunGravityCurve))
		curveGravityAlpha = WallRunGravityCurve->GetFloatValue(alphaMaxGravity);
	
	_PlayerCharacter->GetCharacterMovement()->GravityScale = FMath::Lerp(DefaultGravity,WallRunTargetGravity,curveGravityAlpha);

	//Debug
	if(bDebugTick) UE_LOG(LogTemp, Log, TEXT("UTZParkourComp :: WallRun alpha %f, GravityScale %f, "), curveGravityAlpha, _PlayerCharacter->GetCharacterMovement()->GravityScale);

	//-----Gravity interp-----
	CameraTilt(WallToPlayerDirection);
}

void UPS_ParkourComponent::OnWallRunStart(const AActor* otherActor)
{
	//-----OnWallRunStart-----
	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun start"));

	//Activate Only if in Air
	if(!_PlayerCharacter->GetCharacterMovement()->IsFalling()) return;
	
	if(bDebug)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun start"));
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("StartWallRun")));
	}
	
	//WallRun Logic activation
	UCameraComponent* playerCam = _PlayerCharacter->GetFirstPersonCameraComponent();
	WallRunDirection = otherActor->GetActorForwardVector() * FMath::Sign(otherActor->GetActorForwardVector().Dot(playerCam->GetForwardVector()));
	WallToPlayerDirection = FMath::Sign((otherActor->GetActorLocation() + otherActor->GetActorRightVector()).Dot(_PlayerCharacter->GetActorRightVector()) * -1);
	
	DrawDebugLine(GetWorld(), otherActor->GetActorLocation(), otherActor->GetActorLocation() + otherActor->GetActorRightVector() * 200, FColor::Yellow, false, 2, 10, 3);

	//Constraint init
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintEnabled(true);
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0,0,0));

	//Timer init
	StartWallRunTimestamp = GetWorld()->GetTimeSeconds();
	WallRunSeconds = StartWallRunTimestamp;
	GetWorld()->GetTimerManager().UnPauseTimer(WallRunTimerHandle);

	//Setup work var
	EnterVelocity = _PlayerCharacter->GetCharacterMovement()->GetLastUpdateVelocity().Length();
	bIsWallRunning = true;
	
}

void UPS_ParkourComponent::OnWallRunStop()
{
	//-----OnWallRunStop-----
	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun stop"));
	
	GetWorld()->GetTimerManager().PauseTimer(WallRunTimerHandle);
	WallRunSeconds = 0.0f;
	
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0,0,0));
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintEnabled(false);

	_PlayerCharacter->GetCharacterMovement()->GravityScale = DefaultGravity;
	VelocityWeight = 1.0f;

	CameraTilt(0);

	_PlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	
	bIsWallRunning = false;
}

void UPS_ParkourComponent::CameraTilt(const int32 wallOrientationToPlayer)
{
	if(!IsValid(_PlayerController) || !IsValid(GetWorld())) return;

	UE_LOG(LogTemp, Error, TEXT("ActorRoll %f, WallRunCameraAngle %f, wallOrientationToPlayer %i"), _PlayerCharacter->GetActorRotation().Roll, WallRunCameraAngle, wallOrientationToPlayer);

	//If Camera tilt already finished stop
	//if(FMath::IsNearlyEqual(_PlayerController->GetControlRotation().Roll, WallRunCameraAngle * wallOrientationToPlayer),0.01) return;
	

	//Alpha
	const float alphaFalling = UKismetMathLibrary::MapRangeClamped(WallRunSeconds, StartWallRunTimestamp, StartWallRunTimestamp + WallRunCameraTiltDuration, 0,1);

	float curveTiltAlpha = alphaFalling;
	if(IsValid(WallRunGravityCurve))
		curveTiltAlpha = WallRunGravityCurve->GetFloatValue(alphaFalling);
	
	float newRoll = FMath::Lerp(_PlayerCharacter->GetActorRotation().Roll, WallRunCameraAngle * wallOrientationToPlayer, curveTiltAlpha);

	//Target Rot
	FRotator newControlRot = _PlayerController->GetControlRotation();
	newControlRot.Roll = newRoll;
	
	//Rotate
	_PlayerController->SetControlRotation(newControlRot);

	if(bDebugTick) UE_LOG(LogTemp, Log, TEXT("UTZParkourComp :: WallRun Camera Roll: %f"), _PlayerController->GetControlRotation().Roll);
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
	if(bDebug)UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun Stop by Wall end"));
	OnWallRunStop();
}

//------------------
#pragma endregion Event_Receiver




