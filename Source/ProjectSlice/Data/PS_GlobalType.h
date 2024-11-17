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


USTRUCT(BlueprintType)
struct FSRangeFloat
{
	GENERATED_BODY()

	FSRangeFloat(){};
	FSRangeFloat(const float max, const float min){ Max = max; Min = min;}

	UPROPERTY()
	float Max = 0.0f;
	
	UPROPERTY()
	float Min = 0.0f;
};


//__________________________________________________
#pragma endregion GlobalEnum

