#pragma once
#include "PS_Constants.h"

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

UENUM(BlueprintType)
enum class ETriangularOrientation : uint8
{
	CENTER = 0 UMETA(DisplayName = "Center"),
	UP = 1 UMETA(DisplayName = "Up"),
	LEFT = 2 UMETA(DisplayName = "Left"),
	RIGHT = 3 UMETA(DisplayName = "Right"),
};

const TArray<FName> ScrewSocketNames = { SOCKET_SCREW_INDEX, SOCKET_SCREW_MIDDLE, SOCKET_SCREW_PINKY, SOCKET_SCREW_RING };


//__________________________________________________
#pragma endregion GlobalEnum

#pragma region Gameplay
//------------------

UENUM(BlueprintType)
enum class EPointedObjectType : uint8
{
	DEFAULT = 0 UMETA(DisplayName = "Default"),
	SLICEABLE = 1 UMETA(DisplayName = "Sliceable"),
	CHAOS = 2 UMETA(DisplayName = "Chaos"),
	ENEMY = 3 UMETA(DisplayName = "Enemy")
};

//------------------
#pragma endregion Gameplay
	

