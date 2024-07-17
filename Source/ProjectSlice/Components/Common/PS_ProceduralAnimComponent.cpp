// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ProceduralAnimComponent.h"

#include "Curves/CurveLinearColor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/PC/PS_Character.h"
#include "ProjectSlice/PC/PS_PlayerController.h"


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

void UPS_ProceduralAnimComponent::Dip()
{
	if(!bIsDipping || !IsValid(_PlayerCharacter)) return;

	//Calculate dip alpha
	const float alpha = FMath::Clamp( UKismetMathLibrary::SafeDivide(GetWorld()->GetTimeSeconds() , (DipStartTimestamp + DipDuration)) * DipSpeed,0,1.0f);
	
	float curveForceAlpha = alpha;
	if(IsValid(DipCurve))
		curveForceAlpha = DipCurve->GetFloatValue(alpha);

	DipAlpha = curveForceAlpha * DipStrenght;
	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: dipAlpha %f"), __FUNCTION__, DipAlpha);

	//Move player loc
	//TODO :: Surly need to use other comp
	USceneComponent* playerRoot = _PlayerCharacter->GetFirstPersonRoot();

	FVector newCamLoc = playerRoot->GetRelativeLocation();
	newCamLoc.Z = FMath::Lerp(0,-10, DipAlpha);
	playerRoot->SetRelativeLocation(newCamLoc);

	//Stop dip
	if(alpha >= 1.0f)
	{
		bIsDipping = false;
		DipAlpha = 0.0f;
		if(bDebug) UE_LOG(LogTemp, Error, TEXT("%S :: stop dip"), __FUNCTION__);
	}		
}

//------------------
#pragma endregion Dip

#pragma region Walking
//------------------

void UPS_ProceduralAnimComponent::GetVelocityLagPosition()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(GetWorld()))
		return;

	FVector LagVector;
	LagVector.X = UKismetMathLibrary::SafeDivide(_PlayerCharacter->GetVelocity().Dot(_PlayerCharacter->GetActorRightVector()),  _PlayerCharacter->GetCharacterMovement()->GetMaxSpeed());
	LagVector.Y =  UKismetMathLibrary::SafeDivide(_PlayerCharacter->GetVelocity().Dot(_PlayerCharacter->GetActorForwardVector()) , _PlayerCharacter->GetCharacterMovement()->GetMaxSpeed() * -1);
	LagVector.Z =  UKismetMathLibrary::SafeDivide(_PlayerCharacter->GetVelocity().Dot(_PlayerCharacter->GetActorUpVector()) , _PlayerCharacter->GetCharacterMovement()->JumpZVelocity * -1);

	LocationLagPosition = UKismetMathLibrary::VInterpTo(LocationLagPosition, UKismetMathLibrary::ClampVectorSize(LagVector * 2, 0.0f, 4.0f), GetWorld()->GetDeltaSeconds(), (1 / GetWorld()->GetDeltaSeconds()) / 6.0f);
}

void UPS_ProceduralAnimComponent::Walking(const float leftRightAlpha, const float upDownAlpha,  const float rollAlpha)
{
	if(!IsValid(_PlayerCharacter)) return;
	
	//Left/Right && Up/down
	WalkAnimPos.X = FMath::Lerp(WalkingLeftRightOffest * -1,WalkingLeftRightOffest, leftRightAlpha);
	WalkAnimPos.Z = FMath::Lerp(WalkingDownOffest,WalkingUpOffest, upDownAlpha);

	//Roll rot
	WalkAnimRot.Pitch = FMath::Lerp(1,-1, rollAlpha);

	//Find WalkAnim Alpha
	const UCharacterMovementComponent* playerMovementComp = _PlayerCharacter->GetCharacterMovement();
	WalkAnimAlpha = (playerMovementComp->MovementMode == MOVE_Falling ? 0.0f : UKismetMathLibrary::NormalizeToRange(_PlayerCharacter->GetVelocity().Length(), 0.0f, playerMovementComp->GetMaxSpeed()));
	WalkingSpeed = FMath::Lerp(0.0f,WalkingMaxSpeed, WalkAnimAlpha);
	
}

//------------------
#pragma endregion Walking
	

