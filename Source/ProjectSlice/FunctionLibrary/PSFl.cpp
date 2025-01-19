#include "PSFl.h"

#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ProjectSlice/Components/GPE/PS_SlicedComponent.h"
#include "ProjectSlice/Components/PC/PS_PlayerCameraComponent.h"


class AProjectSliceCharacter;

#pragma region Utilities

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


#pragma endregion Utilities

float UPSFl::GetObjectUnifiedMass(UPrimitiveComponent* const comp, const bool bDebug)
{
	if(!IsValid(comp)) return 0.0f;
	
	UPhysicalMaterial* physMat = comp->BodyInstance.GetSimplePhysicalMaterial();

	const float output = ((comp->GetMass() * comp->GetMassScale() * (IsValid(physMat) ? physMat->Density : 1.0f)) / comp->GetComponentScale().Length());
	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: %s output %f"),__FUNCTION__, *comp->GetName(),output);
	
	return output ;

}

void UPSFl::SweepConeMultiByChannel(
	UWorld* World,
	FVector ConeApex,
	FVector ConeDirection,
	float ConeAngleDegrees,
	float ConeLength,
	float StepInterval,
	float SphereRadius,
	TArray<FHitResult>& OutHits,
	ECollisionChannel TraceChannel,
	FCollisionQueryParams QueryParams
)
{
	if (!World) return;

	// Normalize the cone direction
	ConeDirection.Normalize();

	// Convert angle to radians
	float ConeAngleRadians = FMath::DegreesToRadians(ConeAngleDegrees);

	// Sweep multiple spheres along the cone's length
	for (float Step = 0.0f; Step <= ConeLength; Step += StepInterval)
	{
		// Calculate the current position along the cone's axis
		FVector SweepCenter = ConeApex + ConeDirection * Step;

		// Perform a sphere sweep
		TArray<FHitResult> SphereHits;
		FCollisionShape Sphere = FCollisionShape::MakeSphere(SphereRadius);

		DrawDebugSphere(World,ConeApex,SphereRadius,10,FColor::Yellow,false,4.0f, 10.0f, 3.0f);

		if (World->SweepMultiByChannel(SphereHits, ConeApex, SweepCenter, FQuat::Identity, TraceChannel, Sphere, QueryParams))
		{
			for (const FHitResult& Hit : SphereHits)
			{
				// Calculate the vector from the cone apex to the hit location
				FVector HitDirection = (Hit.ImpactPoint - ConeApex).GetSafeNormal();

				// Check if the hit is within the cone angle
				float DotProduct = FVector::DotProduct(ConeDirection, HitDirection);
				float Angle = FMath::Acos(DotProduct); // Angle in radians

				if (Angle <= ConeAngleRadians)
				{
					DrawDebugPoint(World,Hit.ImpactPoint,10.0f,FColor::Purple,false,4.0f, 10.0f);
					OutHits.Add(Hit); // Add valid hit to the result array
				}
			}
		}
	}

	// Debug: Draw the cone (optional)
	DrawDebugCone(World, ConeApex, ConeDirection, ConeLength, ConeAngleRadians, ConeAngleRadians, 12, FColor::Green, false, 5.0f);
}

void UPSFl::SweepConeMultiByChannel(UWorld* World, FVector ConeApex, FVector ConeDirection, float ConeAngleDegrees,
	float ConeLength, float StepInterval, float SphereRadius, TArray<UPrimitiveComponent*>& OutHitComponents,
	ECollisionChannel TraceChannel, FCollisionQueryParams QueryParams)
{
	if (!World) return;

	// Normalize the cone direction
	ConeDirection.Normalize();

	// Convert angle to radians
	float ConeAngleRadians = FMath::DegreesToRadians(ConeAngleDegrees);

	
	// Sweep multiple spheres along the cone's length
	int i=0;
	int j =0;
	for (float Step = 0.0f; Step <= ConeLength; Step += StepInterval)
	{
		// Calculate the current position along the cone's axis
		FVector SweepCenter = ConeApex + ConeDirection * Step;

		// Perform a sphere sweep
		TArray<FHitResult> SphereHits;
		FCollisionShape Sphere = FCollisionShape::MakeSphere(SphereRadius);
		DrawDebugSphere(World,ConeApex,(ConeApex - SweepCenter).Length(),10,FColor::Yellow,false,4.0f, 10.0f, 3.0f);
		
		if (World->SweepMultiByChannel(SphereHits, ConeApex, SweepCenter, FQuat::Identity, ECC_WorldDynamic, Sphere, QueryParams))
		{
			for (const FHitResult& Hit : SphereHits)
			{
				UE_LOG(LogTemp, Error, TEXT("TEXT i:%i J:%i"), i,j);
				i++;
				// Calculate the vector from the cone apex to the hit location
				FVector HitDirection = (Hit.ImpactPoint - ConeApex).GetSafeNormal();

				// Check if the hit is within the cone angle
				float DotProduct = FVector::DotProduct(ConeDirection, HitDirection);
				float Angle = FMath::Acos(DotProduct); // Angle in radians
		
				DrawDebugPoint(World,Hit.ImpactPoint,10.0f,FColor::Red,false,4.0f, 55.0f);
				
				if (Angle <= ConeAngleRadians && IsValid(Hit.GetComponent()))
				{
					DrawDebugPoint(World,Hit.ImpactPoint,15.0f,FColor::Purple,false,4.0f, 10.0f);
					OutHitComponents.AddUnique(Hit.GetComponent()); // Add valid hit to the result array
				}
			}
		}
		j++;
	}

	// Debug: Draw the cone (optional)
	DrawDebugCone(World, ConeApex, ConeDirection, ConeLength, ConeAngleRadians, ConeAngleRadians, 12, FColor::Green, false, 5.0f);
}



