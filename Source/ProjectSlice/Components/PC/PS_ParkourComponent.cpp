// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ParkourComponent.h"

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
	
	_PlayerController = Cast<APlayerController>(_PlayerCharacter->GetController());
	if(!IsValid(_PlayerController)) return;

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
	if(!bIsWallRunning || !IsValid(_PlayerCharacter) || !IsValid(GetWorld())) return;
	
	WallRunTimestamp = WallRunTimestamp + WallRunTickRate;
	const float alpha = UKismetMathLibrary::MapRangeClamped(WallRunTimestamp, StartWallRunTimestamp, StartWallRunTimestamp + WallRunTimeToMaxGravity, 0,1);

	//Stick to Wall Velocity
	float curveForceAlpha = alpha;
	if(IsValid(WallRunGravityCurve))
		curveForceAlpha = WallRunForceCurve->GetFloatValue(alpha);
	
	if(WallRunTimestamp > StartWallRunTimestamp + WallRunTimeToFall)
		VelocityWeight = UKismetMathLibrary::FInterpTo(VelocityWeight, EnterVelocity - WallRunForceFallingDebuff,GetWorld()->GetDeltaSeconds(), FallingInterpSpeed);
	else
		VelocityWeight = FMath::Lerp(EnterVelocity,EnterVelocity + WallRunForceBoost,curveForceAlpha);
	
	const FVector newPlayerVelocity = WallRunDirection * VelocityWeight;
	_PlayerCharacter->GetCharacterMovement()->Velocity = newPlayerVelocity;

	if(VelocityWeight <= EnterVelocity - WallRunForceFallingDebuff || FMath::IsNearlyEqual(VelocityWeight, EnterVelocity - WallRunForceFallingDebuff, 1))
	{
		UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun Stop By Velocity near EnterVelocity"));
		OnWallRunStop();
	}

	
	if(bDebugTick) UE_LOG(LogTemp, Log, TEXT("UTZParkourComp :: WallRun EnterVelocity %f, VelocityWeight %f"),EnterVelocity, VelocityWeight);
	

	//Gravity interp
	float curveGravityAlpha = alpha;
	if(IsValid(WallRunGravityCurve))
		curveGravityAlpha = WallRunGravityCurve->GetFloatValue(alpha);
	
	_PlayerCharacter->GetCharacterMovement()->GravityScale = FMath::Lerp(DefaultGravity,WallRunTargetGravity,curveGravityAlpha);

	if(bDebugTick) UE_LOG(LogTemp, Log, TEXT("UTZParkourComp :: WallRun alpha %f, GravityScale %f, "), curveGravityAlpha, _PlayerCharacter->GetCharacterMovement()->GravityScale);
	
}

void UPS_ParkourComponent::OnWallRunStart(const AActor* otherActor)
{
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

	//Constraint init
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintEnabled(true);
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0,0,0));

	//Timer init
	StartWallRunTimestamp = GetWorld()->GetTimeSeconds();
	WallRunTimestamp = StartWallRunTimestamp;
	GetWorld()->GetTimerManager().UnPauseTimer(WallRunTimerHandle);

	//Setup work var
	EnterVelocity = _PlayerCharacter->GetCharacterMovement()->GetLastUpdateVelocity().Length();
	bIsWallRunning = true;
	
}

void UPS_ParkourComponent::OnWallRunStop()
{
	if(bDebug) UE_LOG(LogTemp, Warning, TEXT("UTZParkourComp :: WallRun stop"));
	
	GetWorld()->GetTimerManager().PauseTimer(WallRunTimerHandle);
	WallRunTimestamp = 0.0f;
	
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0,0,0));
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintEnabled(false);

	_PlayerCharacter->GetCharacterMovement()->GravityScale = DefaultGravity;
	VelocityWeight = 1.0f;

	_PlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	
	bIsWallRunning = false;
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
	OnWallRunStop();
}

//------------------
#pragma endregion Event_Receiver




