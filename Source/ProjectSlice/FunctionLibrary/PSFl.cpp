#include "PSFl.h"

#include "ProjectSlice/Components/GPE/PS_SlicedComponent.h"
#include "ProjectSlice/Components/PC/PS_PlayerCameraComponent.h"


class AProjectSliceCharacter;

FVector UPSFl::ClampVelocity(FVector currentVelocity, const FVector& targetVelocity, const float maxVelocity, const bool bDebug )
{
	FVector clampedVel = currentVelocity;
	if(targetVelocity.Length() > maxVelocity)
	{
		currentVelocity.Normalize();
		clampedVel = currentVelocity * maxVelocity;
		
		if(bDebug)
			UE_LOG(LogTemp, Warning, TEXT("%S :: Clamp Max Velocity"), __FUNCTION__);
	}
	return clampedVel;
}


FVector UPSFl::ClampVelocity(FVector& startVelocity, FVector currentVelocity, const FVector& targetVelocity, const float maxVelocity, const bool bDebug )
{
	FVector clampedVel = targetVelocity;
	if(targetVelocity.Length() > maxVelocity)
	{
		currentVelocity.Normalize();
		clampedVel = currentVelocity * maxVelocity;

		//Clamp base velocity too
		if(startVelocity.Length() > maxVelocity)
			startVelocity = clampedVel;

		if(bDebug)
			UE_LOG(LogTemp, Warning, TEXT("%S :: Clamp Max Velocity"), __FUNCTION__);
	}
	return clampedVel;
}


FVector UPSFl::GetWorldInputDirection(const UPS_PlayerCameraComponent* cameraInstance, const FVector2D moveInput)
{
	FVector worldInputDirection = cameraInstance->GetRightVector() * moveInput.X + cameraInstance->GetForwardVector() * moveInput.Y;
	worldInputDirection.Z = 0;
	worldInputDirection.Normalize();
	return worldInputDirection;
}

float UPSFl::GetSlicedObjectUnifiedMass(UPrimitiveComponent* comp)
{
	UPS_SlicedComponent* currentSlicedComponent = Cast<UPS_SlicedComponent>(comp);

	if(!IsValid(currentSlicedComponent)) return 0.0f;
	
	UPhysicalMaterial* physMat = currentSlicedComponent->BodyInstance.GetSimplePhysicalMaterial();

	UE_LOG(LogTemp, Warning, TEXT("GetComponentScale %f"), currentSlicedComponent->GetComponentScale().Length());
	
	return ((currentSlicedComponent->GetMass() * currentSlicedComponent->GetMassScale() * (IsValid(physMat) ? physMat->Density : 1.0f)) /currentSlicedComponent->GetComponentScale().Length());

}
