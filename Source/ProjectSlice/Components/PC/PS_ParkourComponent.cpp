// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ParkourComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectSlice/PC/PS_Character.h"


// Sets default values for this component's properties
UPS_ParkourComponent::UPS_ParkourComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UPS_ParkourComponent::BeginPlay()
{
	Super::BeginPlay();
	
	_PlayerCharacter = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if(!IsValid(_PlayerCharacter)) return;
	
	_PlayerController = Cast<APlayerController>(_PlayerCharacter->GetController());
	if(!IsValid(_PlayerController)) return;
	
	ParkourDetector = _PlayerCharacter->GetCapsuleComponent();
	if(IsValid(ParkourDetector))
		ParkourDetector->OnComponentBeginOverlap.AddUniqueDynamic(this,&UPS_ParkourComponent::OnParkourDetectorBeginOverlapEventReceived);
}


// Called every frame
void UPS_ParkourComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

#pragma region Event_Receiver
//------------------

void UPS_ParkourComponent::OnParkourDetectorBeginOverlapEventReceived(UPrimitiveComponent onComponentBeginOverlap, UPrimitiveComponent* overlappedComponent,
	AActor* otherActor, UPrimitiveComponent* otherComp, int otherBodyIndex, bool bFromSweep, const FHitResult& sweepResult)
{
	if(!IsValid(_PlayerCharacter) || _PlayerCharacter->GetFirstPersonCameraComponent()->IsValidLowLevel() || !IsValid(otherActor)) return;

	//Activate Only if in Air
	if(!_PlayerCharacter->GetCharacterMovement()->IsFalling()) return;

	PlayerCamera = _PlayerCharacter->GetFirstPersonCameraComponent();

	FVector wallRunDir = otherActor->GetActorForwardVector() * otherActor->GetActorForwardVector().Dot(PlayerCamera->GetForwardVector());
	
	bIsWallRunning = true;
	
	
	
}

//------------------
#pragma endregion Event_Receiver




