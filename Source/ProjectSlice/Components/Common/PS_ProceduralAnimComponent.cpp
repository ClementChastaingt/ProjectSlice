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
	const float alpha = FMath::Clamp((GetWorld()->GetTimeSeconds() / (DipStartTimestamp + DipDuration)) * DipSpeed,0,1.0f);
	
	float curveForceAlpha = alpha;
	if(IsValid(DipCurve))
		curveForceAlpha = DipCurve->GetFloatValue(alpha);

	const float dipAlpha = curveForceAlpha * DipStrenght;
	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: dipAlpha %f"), __FUNCTION__, alpha);

	//Move player loc
	//TODO :: Surly need to use other comp
	UCameraComponent* playerCam = _PlayerCharacter->GetFirstPersonCameraComponent();

	FVector newCamLoc = playerCam->GetRelativeLocation();
	newCamLoc.Z = FMath::Lerp(0,-10, dipAlpha);
	playerCam->SetRelativeLocation(newCamLoc);

	//Stop dip
	if(dipAlpha >= 1.0f)
	{
		bIsDipping = false;
		if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: stop dip"), __FUNCTION__);
	}		
}

void UPS_ProceduralAnimComponent::SetVelocityLagPosition()
{
	if(!IsValid(_PlayerCharacter) || !IsValid(GetWorld()))
		return;

	FVector LagVector;
	LagVector.X = UKismetMathLibrary::SafeDivide(_PlayerCharacter->GetVelocity().Dot(_PlayerCharacter->GetActorRightVector()),  _PlayerCharacter->GetCharacterMovement()->GetMaxSpeed());
	LagVector.Y =  UKismetMathLibrary::SafeDivide(_PlayerCharacter->GetVelocity().Dot(_PlayerCharacter->GetActorForwardVector()) , _PlayerCharacter->GetCharacterMovement()->GetMaxSpeed() * -1);
	LagVector.Z =  UKismetMathLibrary::SafeDivide(_PlayerCharacter->GetVelocity().Dot(_PlayerCharacter->GetActorUpVector()) , _PlayerCharacter->GetCharacterMovement()->JumpZVelocity * -1);

	LocationLagPosition = UKismetMathLibrary::VInterpTo(LocationLagPosition, UKismetMathLibrary::ClampVectorSize(LagVector * 2, 0.0f, 4.0f), GetWorld()->GetDeltaSeconds(), (1 / GetWorld()->GetDeltaSeconds()) / 6.0f);
}

void UPS_ProceduralAnimComponent::Walking()
{
	if(!IsValid(_PlayerCharacter)) return;

	//Calculate dip alpha
	const float alpha = FMath::Clamp((GetWorld()->GetTimeSeconds() / (DipStartTimestamp + DipDuration)) * DipSpeed,0,1.0f);

	
	//R: LeftRightAlpha, G: UpDown_Alpha, B: Roll_Alpha, A: Footstep 
	FLinearColor curveWalkingAlpha = FLinearColor(alpha,alpha,alpha,alpha);	
	if(IsValid(WalkingProcAnimCurve))
		curveWalkingAlpha = WalkingProcAnimCurve->GetLinearColorValue(alpha);

	//Left/Right && Up/down 
	WalkAnimPos.X = FMath::Lerp(WalkingLeftRightOffest * -1,WalkingLeftRightOffest, curveWalkingAlpha.R);
	WalkAnimPos.Z = FMath::Lerp(WalkingDownOffest,WalkingUpOffest, curveWalkingAlpha.G);

	//Roll rot
	WalkAnimRot.Pitch = FMath::Lerp(1,-1, curveWalkingAlpha.B);

	//Find WalkAnim Alpha
	const UCharacterMovementComponent* playerMovementComp = _PlayerCharacter->GetCharacterMovement();
	const float speed = FMath::Lerp(0.0f,WalkingMaxSpeed, WalkAnimAlpha);
	
	WalkAnimAlpha = (playerMovementComp->MovementMode == MOVE_Falling ? 0.0f : UKismetMathLibrary::NormalizeToRange(_PlayerCharacter->GetVelocity().Length(), 0.0f, playerMovementComp->GetMaxSpeed())) * speed;

	SetVelocityLagPosition();
}

//------------------
#pragma endregion Dip

#pragma region Walking
//------------------



//------------------
#pragma endregion Walking
	

