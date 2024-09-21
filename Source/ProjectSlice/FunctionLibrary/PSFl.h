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
	
	UFUNCTION()
	static FVector ClampVelocity(FVector currentVelocity, const FVector& targetVelocity, const float maxVelocity, FVector startVelocity =FVector(0, 0, 0), const bool bDebug = false);

#pragma endregion Utilities


};
