// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProjectSliceGameMode.h"

#include "../PC/PS_Character.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectSlice/Components/PS_SliceComponent.h"
#include "UObject/ConstructorHelpers.h"

AProjectSliceGameMode::AProjectSliceGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}

void AProjectSliceGameMode::BeginPlay()
{
	Super::BeginPlay();

	//Add SLiceComponent to sliceable actors
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("Sliceable")), SliceableActors);
	const FTransform relativeTransform = FTransform();
	int i = 0;
	if(bDebugMode) UE_LOG(LogTemp, Warning, TEXT("----- PS_GameMode :: Add SliceComponent to Sliceable Actors -----"));
	for (const auto outActor : SliceableActors)
	{
		UPS_SliceComponent* newComp = Cast<UPS_SliceComponent>(outActor->AddComponentByClass(SliceComponent, false, relativeTransform, false));
		outActor->RegisterAllComponents();
		outActor->AddInstanceComponent(newComp);
		newComp->InitSliceObject();
		if(bDebugMode) UE_LOG(LogTemp, Log, TEXT("PS_GameMode :: Sliceable Actor[%i] %s"), i++, *newComp->GetName());
	}
	

}
