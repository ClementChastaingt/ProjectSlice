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
enum class EDirectionOrientation : uint8
{
	FORWARD     UMETA(DisplayName = "Forward"),
	UP          UMETA(DisplayName = "Up"),
	BACKWARD    UMETA(DisplayName = "Backward"),
	DOWN        UMETA(DisplayName = "Down"),
	LEFT        UMETA(DisplayName = "Left"),
	RIGHT       UMETA(DisplayName = "Right"),
};


UENUM(BlueprintType)
enum class ETriangularOrientation : uint8
{
	CENTER = 0 UMETA(DisplayName = "Center"),
	UP = 1 UMETA(DisplayName = "Up"),
	LEFT = 2 UMETA(DisplayName = "Left"),
	RIGHT = 3 UMETA(DisplayName = "Right"),
};

//__________________________________________________
#pragma endregion GlobalEnum

#pragma region Gameplay
//__________________________________________________
UENUM(BlueprintType)
enum class EPointedObjectType : uint8
{
	DEFAULT = 0 UMETA(DisplayName = "Default"),
	SLICEABLE = 1 UMETA(DisplayName = "Sliceable"),
	CHAOS = 2 UMETA(DisplayName = "Chaos"),
	ENEMY = 3 UMETA(DisplayName = "Enemy")
};

UENUM(BlueprintType)
enum class EScreenShakeType : uint8
{
	GLASSES = 0 UMETA(DisplayName = "Glasses"),
	SHOOT = 1 UMETA(DisplayName = "Shoot"),
	DASH = 2 UMETA(DisplayName = "Dash"),
	SLIDE = 4 UMETA(DisplayName = "Slide"),
	FORCE = 5 UMETA(DisplayName = "Force"),
	LANDING = 6 UMETA(DisplayName = "Landing"),
	IMPACT = 7 UMETA(DisplayName = "Impact"),
};


const TArray<FName> ScrewSocketNames = { SOCKET_SCREW_INDEX, SOCKET_SCREW_MIDDLE, SOCKET_SCREW_PINKY, SOCKET_SCREW_RING };

//__________________________________________________
#pragma endregion Gameplay