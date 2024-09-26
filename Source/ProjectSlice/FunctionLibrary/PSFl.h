#pragma once

#include "ProjectSlice/PC/PS_Character.h"
#include "PSFl.generated.h"

UCLASS(ClassGroup="FunctionLibrary", Category = "Misc", meta = (ToolTip="General project function library."))
class PROJECTSLICE_API UPSFl : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UPSFl() {}


#pragma region Utilities
	//------------------

public:

	/*
	 * @brief Check and Clamp a Velocity Target vector to MaxVelocity
	 * @param currentVelocity: movement current velocitu to check
	 * @param targetVelocity: movement target max velocity 
	 * @param maxVelocity: maximum velocity threshold
	 * @param bDebug: will log if true.
	 * @return the Vector velocity Clamped
	 */
	static FVector ClampVelocity(FVector currentVelocity, const FVector& targetVelocity, const float maxVelocity, const bool bDebug = false);

	/*
	 * @brief Check and Clamp a Velocity Target and Velocity Start vector to MaxVelocity
	 * @param startVelocity: movement start velocitu
	 * @param currentVelocity: movement current velocitu to check
	 * @param targetVelocity: movement target max velocity 
	 * @param maxVelocity: maximum velocity threshold
	 * @param bDebug: will log if true.
	 * @return the Vector velocity Clamped
	 */
	static FVector ClampVelocity(FVector& startVelocity, FVector currentVelocity, const FVector& targetVelocity, const float maxVelocity, const bool bDebug = false);
		
	static FVector GetWorldInputDirection(const UPS_PlayerCameraComponent* cameraInstance, FVector2D moveInput);

#pragma endregion Utilities


};
