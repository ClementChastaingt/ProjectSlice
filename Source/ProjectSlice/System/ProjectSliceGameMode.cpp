// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProjectSliceGameMode.h"

#include "../PC/PS_Character.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectSlice/Components/PS_SlicedComponent.h"
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
		//newComp->AttachToComponent(outActor->GetRootComponent(),FAttachmentTransformRules::KeepRelativeTransform);
		outActor->RegisterAllComponents();
		if(IsValid(newComp))
		{
			outActor->AddInstanceComponent(newComp);
			newComp->InitSliceObject();
			newComp->SetupAttachment(outActor->GetRootComponent());
			if(bDebugMode) UE_LOG(LogTemp, Log, TEXT("PS_GameMode :: Sliceable Actor[%i] %s add %s"), i++, *outActor->GetName(), *newComp->GetName());
		}
		else
			UE_LOG(LogTemp, Error, TEXT("PS_GameMode :: Sliceable Actor[%i] invalid UPS_SlicedComponent for %s"), i++, *outActor->GetName());
	}
}

