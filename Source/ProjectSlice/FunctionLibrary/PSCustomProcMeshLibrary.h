// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "KismetProceduralMeshLibrary.h"
#include "PSCustomProcMeshLibrary.generated.h"

/**
 * 
 */


USTRUCT(BlueprintType, Category = "Struct")
struct FSCustomSliceOutput
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	bool bDebug = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	UMaterialInterface* InProcMeshDefaultMat = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	int32 InProcMeshCapIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	int32 OutProcMeshCapIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClipCap")
	TArray<FVector> ClipCapPointLoc;
};


UCLASS()
class PROJECTSLICE_API UPSCustomProcMeshLibrary : public UKismetProceduralMeshLibrary
{
	GENERATED_BODY()

public:

	/** 
	 *	Slice the ProceduralMeshComponent (including simple convex collision) using a plane. Optionally create 'cap' geometry. 
	 *	@param	InProcMesh				ProceduralMeshComponent to slice
	 *	@param	PlanePosition			Point on the plane to use for slicing, in world space
	 *	@param	PlaneNormal				Normal of plane used for slicing. Geometry on the positive side of the plane will be kept.
	 *	@param	bCreateOtherHalf		If true, an additional ProceduralMeshComponent (OutOtherHalfProcMesh) will be created using the other half of the sliced geometry
	 *	@param	OutOtherHalfProcMesh	If bCreateOtherHalf is set, this is the new component created. Its owner will be the same as the supplied InProcMesh.
	 *	@param	CapOption				If and how to create 'cap' geometry on the slicing plane
	 *	@param	CapMaterial				If creating a new section for the cap, assign this material to that section
	 */
	
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh")
	static void SliceProcMesh(UProceduralMeshComponent* InProcMesh, FVector PlanePosition, FVector PlaneNormal, bool bCreateOtherHalf, UProceduralMeshComponent
		*& OutOtherHalfProcMesh, FSCustomSliceOutput& outSlicingData, EProcMeshSliceCapOption CapOption, UMaterialInterface*
		CapMaterial);
};
