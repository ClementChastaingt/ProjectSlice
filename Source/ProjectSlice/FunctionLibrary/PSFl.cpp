#include "PSFl.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "ProjectSlice/Components/PC/PS_PlayerCameraComponent.h"
#include "ProjectSlice/PC/PS_Character.h"


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

//TODO :: Move this func in ACharcter when NME was create
FVector UPSFl::GetFootPlacementLoc(const ACharacter* const character)
{
	FVector footPlacement = character->GetCapsuleComponent()->GetComponentLocation();
	footPlacement.Z = footPlacement.Z - character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

	return footPlacement;
}



bool UPSFl::IsInAir(const ACharacter* const character)
{
	const bool bIsInAir = character->GetCharacterMovement()->IsFalling() || character->GetCharacterMovement()->IsFlying();
	return bIsInAir;
}

