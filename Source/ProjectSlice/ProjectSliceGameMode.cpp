// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProjectSliceGameMode.h"
#include "ProjectSliceCharacter.h"
#include "UObject/ConstructorHelpers.h"

AProjectSliceGameMode::AProjectSliceGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
