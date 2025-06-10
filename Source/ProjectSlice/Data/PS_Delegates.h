#pragma once

#include "PS_Delegates.generated.h"

/**
 * File where to put delegates
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPSDelegate);

// 1 param
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Bool, const bool, boolValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Int, const int32, intValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Float, const float, floatValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Vector, const FVector&, vectorValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Vector2D,const FVector2D&, vectorValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Transform,const FTransform&, transformValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Field,const AFieldSystemActor*, fieldActor);


// 2 params
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPSDelegate_Int_Actor, const int32&, intValue, const AActor*, actor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPSDelegate_Vector_Vector, const FVector&, vectorValue1, const FVector&, vectorValue2);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPSDelegate_Vector_Rotator, const FVector&, vectorValue, const FRotator&, rotatorValue);



UCLASS()
class PROJECTSLICE_API UPS_Delegates : public UObject
{
	GENERATED_BODY()
};
