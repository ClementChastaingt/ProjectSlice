#pragma once

#include "PS_GlobalType.generated.h"

#pragma region GlobalEnum
//__________________________________________________

UENUM(BlueprintType)
enum class EComparison : uint8
{
	EQUAL = 0 UMETA(DisplayName = "Equal"),
	INFERIOR = 1 UMETA(DisplayName = "Inferior"),
	SUPERIOR = 2 UMETA(DisplayName = "Superior")
};

UENUM(BlueprintType)
enum class ETransitPhase : uint8
{
	TRANSIT_IN = 0 UMETA(DisplayName = "Transit_In"),
	TRANSIT_OUT = 1 UMETA(DisplayName = "Transit_Out"),
};


//__________________________________________________
#pragma endregion GlobalEnum

