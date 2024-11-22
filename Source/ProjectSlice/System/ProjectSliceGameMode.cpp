// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProjectSliceGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectSlice/Components/GPE/PS_SlicedComponent.h"
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

	if(bEnableInitSliceableContent)
		InitSliceableContent();
	
}

void AProjectSliceGameMode::InitSliceableContent()
{
	//Add SliceComponent to sliceable actors
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("Sliceable")), SliceableActors);
	const FTransform relativeTransform = FTransform();

	int i = 0;
	if(bDebugMode) UE_LOG(LogTemp, Warning, TEXT("----- PS_GameMode :: Add SliceComponent to Sliceable Actors -----"));

	
	for (const auto outActor : SliceableActors)
	{
		//Test actor validity
		if(!IsValid(outActor) || !IsValid(outActor->GetRootComponent()))
		{
			UE_LOG(LogTemp, Error, TEXT("PS_GameMode :: Sliceable Actor:  %s invalid "), *outActor->GetActorNameOrLabel());
			break;
		}

		//Create and Add SlicedComponent to actors
		UPS_SlicedComponent* newComp = Cast<UPS_SlicedComponent>(outActor->AddComponentByClass(SliceComponent, true, relativeTransform, false));
		outActor->RegisterAllComponents();

		if(IsValid(newComp))
		{
			//For display the componenet on actor 
			outActor->AddInstanceComponent(newComp);
			
			newComp->InitSliceObject();
			
			if(bDebugMode) UE_LOG(LogTemp, Log, TEXT("PS_GameMode :: Sliceable Actor[%i] %s add %s"), i++, *outActor->GetActorNameOrLabel(), *newComp->GetName());
		}
		else
			UE_LOG(LogTemp, Error, TEXT("PS_GameMode :: Sliceable Actor[%i] invalid UPS_SlicedComponent for %s"), i++, *outActor->GetName());
	}
}

