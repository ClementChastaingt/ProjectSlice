#pragma once

#include "ProceduralMeshComponent.h"
#include "ProjectSlice/Components/PC/PS_PlayerCameraComponent.h"
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

	/*
	 * @brief Return Input direction by Camera orientation
	 * @param cameraInstance: camera instance used
	 * @param moveInput: input of the current movement
	 * @return the world input direction
	*/
	static FVector GetWorldInputDirection(const UPS_PlayerCameraComponent* cameraInstance, FVector2D moveInput);

	/*
	 * @brief Return screen center point world location 
	 * @param APlayerController: current player controller reference
	 * @return the screen center point world location 
	*/
	UFUNCTION(BlueprintCallable)
	static FVector GetScreenCenterWorldLocation(const APlayerController* const PlayerController);

	/*
	 * @brief Return screen center directed point on an input distance 
	 * @param APlayerController: current player controller reference
	 * @param Distance: distance of the point
	 * @return the screen center point world location 
	*/
	static FVector GetWorldPointInFrontOfCamera(const APlayerController* PlayerController, float Distance);

	/*
	 * @brief Get an Object unified mass. Return Mass scaled by Mass Scale, Object Scale and by Physical Material density
	 * @param comp UPrimitiveComponent to check mass
	 * @param bDebug: display returned mass
	 * @return the Vector velocity Clamped
	 */
	static float GetObjectUnifiedMass(UPrimitiveComponent* const comp, const bool bDebug = false);

	UFUNCTION()
	UStaticMesh* CreateMeshFromProcMesh(UProceduralMeshComponent* procMesh);

#pragma endregion Utilities

#pragma region Trace
	//------------------

public:
	
	/*
	 * @brief Generate a custom Collision Query Params 
	 */
	
	static FCollisionQueryParams CustomConfigureCollisionParams(FName TraceTag, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, bool bIgnoreSelf, const UObject* WorldContextObject);
	
	/*
	 * @brief Made a cone Sweep trace test and return HitResult
	 * @param World: World reference
	 * @param ConeApex: Cone base start loc
	 * @param ConeDirection: Conde direction
	 * @param ConeAngleDegrees: maximum velocity threshold
	 * @param ConeLength: Cone length in cm
	 * @param StepInterval: Sweep multiple spheres along the cone's length refresh rate
	 * @param SphereRadius: Sweep sphere radius.
	 * @param OutHits: Array of FHitResult founded in cone radius
	 * @param TraceChannel: Trace channel to test
	 * @param QueryParams: Collsion query params (ignored actor ect...).
	 * @return the void
	 */
	static void SweepConeMultiByChannel(
		UWorld* World,
		FVector ConeApex,
		FVector ConeDirection,
		float ConeAngleDegrees,
		float ConeLength,
		float StepInterval,
		TArray<FHitResult>& OutHits,
		ECollisionChannel TraceChannel,
		const TArray<AActor*>& ActorsToIgnore,
		bool bDebug);

	/*
	 * @brief Made a cone Sweep trace test and return array of component use no duplicates
	 * @param World: World reference
	 * @param ConeApex: Cone base start loc
	 * @param ConeDirection: Conde direction
	 * @param ConeAngleDegrees: maximum velocity threshold
	 * @param ConeLength: Cone length in cm
	 * @param StepInterval: Sweep multiple spheres along the cone's length refresh rate
	 * @param SphereRadius: Sweep sphere radius.
	 * @param OutHitComponents: Array of components founded in cone radius
	 * @param TraceChannel: Trace channel to test
	 * @param QueryParams: Collsion query params (ignored actor ect...).
	 * @return the void
	 */
	static void SweepConeMultiByChannel(
		UWorld* World,
		FVector ConeApex,
		FVector ConeDirection,
		float ConeAngleDegrees,
		float ConeLength,
		float StepInterval,
		TArray<UPrimitiveComponent*>& OutHitComponents,
		ECollisionChannel TraceChannel,
		const TArray<AActor*>& ActorsToIgnore,
		bool bDebug);

#pragma endregion Trace

#pragma region Cooldown
	//------------------

public:
	
	/*
	 * @brief Start cooldown logic
	 * @param World: World reference
	 * @param Duration: durartion of cooldown
	 * @param FTimerHandle: timerHandler output
	 * @return the cooldown timerhandler
	 */
	UFUNCTION(BlueprintCallable)
	static void StartCooldown(UWorld* World, float coolDownDuration,UPARAM(ref) FTimerHandle& timerHandler);
	
	//------------------
#pragma endregion Cooldown
};
