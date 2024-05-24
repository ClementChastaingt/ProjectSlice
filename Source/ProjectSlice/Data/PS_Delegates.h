#pragma once

#include "PS_Delegates.generated.h"

/**
 * File where to put delegates
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPSDelegate);

// 1 param
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Vector, const FVector, vectorValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Vector2D,const FVector2D, vectorValue);

// 2 params
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPSDelegate_Int_Actor, const int32&, intValue, const AActor*, actor);


UCLASS()
class PROJECTSLICE_API UPS_Delegates : public UObject
{
	GENERATED_BODY()
};
