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
	
	ParkourDetector = _PlayerCharacter->GetCapsuleComponent();
	if(IsValid(ParkourDetector))
	{
		ParkourDetector->OnComponentBeginOverlap.AddUniqueDynamic(this,&UPS_ParkourComponent::OnParkourDetectorBeginOverlapEventReceived);
		ParkourDetector->OnComponentEndOverlap.AddUniqueDynamic(this,&UPS_ParkourComponent::OnParkourDetectorEndOverlapEventReceived);
	}
	
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


void UPS_ParkourComponent::WallRunTick()
{
	if(!bIsWallRunning) return;

	//Stick to Wall Velocity
	// const float alphaForce = 
	// float curvegForceAlpha = alphaForce;
	//
	// if(IsValid(WallRunGravityCurve))
	// 	curvegForceAlpha = WallRunGravityCurve->GetFloatValue(alphaForce);

	_PlayerCharacter->GetCharacterMovement()->Velocity = WallRunDirection * WallRunForceMultiplicator;

	//Gravity interp
	WallRunTimestamp = WallRunTimestamp + WallRunTickRate;
	
	const float alphaGravity = UKismetMathLibrary::MapRangeClamped(WallRunTimestamp, StartWallRunTimestamp, StartWallRunTimestamp + WallRunTimeToMaxGravity, 0,1);
	float curvegGravityAlpha = alphaGravity;
	
	if(IsValid(WallRunGravityCurve))
		curvegGravityAlpha = WallRunGravityCurve->GetFloatValue(alphaGravity);
	_PlayerCharacter->GetCharacterMovement()->GravityScale = FMath::Lerp(_PlayerCharacter->GetCharacterMovement()->GravityScale,WallRunTargetGravity,curvegGravityAlpha);
	
}



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

	//Activate Only if in Air
	if(!_PlayerCharacter->GetCharacterMovement()->IsFalling()) return;

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("StartWallRun")));

	//WallRun Logic activation
	UCameraComponent* playerCam = _PlayerCharacter->GetFirstPersonCameraComponent();
	WallRunDirection = otherActor->GetActorForwardVector() * FMath::Sign(otherActor->GetActorForwardVector().Dot(playerCam->GetForwardVector()));
	
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintEnabled(true);
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0,0,1));

	StartWallRunTimestamp = GetWorld()->GetTimeSeconds();
	WallRunTimestamp = StartWallRunTimestamp;
	GetWorld()->GetTimerManager().UnPauseTimer(WallRunTimerHandle);
		
	bIsWallRunning = true;
		
}

void UPS_ParkourComponent::OnParkourDetectorEndOverlapEventReceived(UPrimitiveComponent* overlappedComponent,
	AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex)
{

	GetWorld()->GetTimerManager().PauseTimer(WallRunTimerHandle);
	WallRunTimestamp = 0;
	
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0,0,0));
	_PlayerCharacter->GetCharacterMovement()->SetPlaneConstraintEnabled(false);

	_PlayerCharacter->GetCharacterMovement()->GravityScale = DefaultGravity;
	
	bIsWallRunning = false;
	
}

//------------------
#pragma endregion Event_Receiver




