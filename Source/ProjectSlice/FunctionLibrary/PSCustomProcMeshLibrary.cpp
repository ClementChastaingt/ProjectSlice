// Fill out your copyright notice in the Description page of Project Settings.


#include "PSCustomProcMeshLibrary.h"

#include "GeomTools.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"


struct FUtilEdge3D;


//////////////////////////////////////////////////////////////////////////

/** Util that returns 1 ir on positive side of plane, -1 if negative, or 0 if split by plane */
int32 BoxPlaneCompare(FBox InBox, const FPlane& InPlane)
{
	FVector BoxCenter, BoxExtents;
	InBox.GetCenterAndExtents(BoxCenter, BoxExtents);

	// Find distance of box center from plane
	FVector::FReal BoxCenterDist = InPlane.PlaneDot(BoxCenter);

	// See size of box in plane normal direction
	FVector::FReal BoxSize = FVector::BoxPushOut(InPlane, BoxExtents);

	if (BoxCenterDist > BoxSize)
	{
		return 1;
	}
	else if (BoxCenterDist < -BoxSize)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

/** Take two static mesh verts and interpolate all values between them */
FProcMeshVertex InterpolateVert(const FProcMeshVertex& V0, const FProcMeshVertex& V1, float Alpha)
{
	FProcMeshVertex Result;

	// Handle dodgy alpha
	if (FMath::IsNaN(Alpha) || !FMath::IsFinite(Alpha))
	{
		Result = V1;
		return Result;
	}

	Result.Position = FMath::Lerp(V0.Position, V1.Position, Alpha);

	Result.Normal = FMath::Lerp(V0.Normal, V1.Normal, Alpha);

	Result.Tangent.TangentX = FMath::Lerp(V0.Tangent.TangentX, V1.Tangent.TangentX, Alpha);
	Result.Tangent.bFlipTangentY = V0.Tangent.bFlipTangentY; // Assume flipping doesn't change along edge...

	Result.UV0 = FMath::Lerp(V0.UV0, V1.UV0, Alpha);

	Result.Color.R = FMath::Clamp(FMath::TruncToInt(FMath::Lerp(float(V0.Color.R), float(V1.Color.R), Alpha)), 0, 255);
	Result.Color.G = FMath::Clamp(FMath::TruncToInt(FMath::Lerp(float(V0.Color.G), float(V1.Color.G), Alpha)), 0, 255);
	Result.Color.B = FMath::Clamp(FMath::TruncToInt(FMath::Lerp(float(V0.Color.B), float(V1.Color.B), Alpha)), 0, 255);
	Result.Color.A = FMath::Clamp(FMath::TruncToInt(FMath::Lerp(float(V0.Color.A), float(V1.Color.A), Alpha)), 0, 255);

	return Result;
}


/** Transform triangle from 2D to 3D static-mesh triangle. */
void Transform2DPolygonTo3D(const FUtilPoly2D& InPoly, const FMatrix& InMatrix, TArray<FProcMeshVertex>& OutVerts, FBox& OutBox)
{
	FVector3f PolyNormal = (FVector3f)-InMatrix.GetUnitAxis(EAxis::Z);
	FProcMeshTangent PolyTangent(InMatrix.GetUnitAxis(EAxis::X), false);

	for (int32 VertexIndex = 0; VertexIndex < InPoly.Verts.Num(); VertexIndex++)
	{
		const FUtilVertex2D& InVertex = InPoly.Verts[VertexIndex];

		FProcMeshVertex NewVert;

		NewVert.Position = InMatrix.TransformPosition(FVector(InVertex.Pos.X, InVertex.Pos.Y, 0.f));
		NewVert.Normal = (FVector)PolyNormal;
		NewVert.Tangent = PolyTangent;
		NewVert.Color = InVertex.Color;
		NewVert.UV0 = InVertex.UV;

		OutVerts.Add(NewVert);

		// Update bounding box
		OutBox += NewVert.Position;
	}
}

/** Given a polygon, decompose into triangles. */
bool TriangulatePoly(TArray<uint32>& OutTris, const TArray<FProcMeshVertex>& PolyVerts, int32 VertBase, const FVector3f& PolyNormal)
{
	// Can't work if not enough verts for 1 triangle
	int32 NumVerts = PolyVerts.Num() - VertBase;
	if (NumVerts < 3)
	{
		OutTris.Add(0);
		OutTris.Add(2);
		OutTris.Add(1);

		// Return true because poly is already a tri
		return true;
	}

	// Remember initial size of OutTris, in case we need to give up and return to this size
	const int32 TriBase = OutTris.Num();

	// Init array of vert indices, in order. We'll modify this
	TArray<int32> VertIndices;
	VertIndices.AddUninitialized(NumVerts);
	for (int VertIndex = 0; VertIndex < NumVerts; VertIndex++)
	{
		VertIndices[VertIndex] = VertBase + VertIndex;
	}

	// Keep iterating while there are still vertices
	while (VertIndices.Num() >= 3)
	{
		// Look for an 'ear' triangle
		bool bFoundEar = false;
		for (int32 EarVertexIndex = 0; EarVertexIndex < VertIndices.Num(); EarVertexIndex++)
		{
			// Triangle is 'this' vert plus the one before and after it
			const int32 AIndex = (EarVertexIndex == 0) ? VertIndices.Num() - 1 : EarVertexIndex - 1;
			const int32 BIndex = EarVertexIndex;
			const int32 CIndex = (EarVertexIndex + 1) % VertIndices.Num();

			const FProcMeshVertex& AVert = PolyVerts[VertIndices[AIndex]];
			const FProcMeshVertex& BVert = PolyVerts[VertIndices[BIndex]];
			const FProcMeshVertex& CVert = PolyVerts[VertIndices[CIndex]];

			// Check that this vertex is convex (cross product must be positive)
			const FVector3f ABEdge = FVector3f(BVert.Position - AVert.Position);
			const FVector3f ACEdge = FVector3f(CVert.Position - AVert.Position);
			const float TriangleDeterminant = (ABEdge ^ ACEdge) | PolyNormal;
			if (TriangleDeterminant > 0.f)
			{
				continue;
			}

			bool bFoundVertInside = false;
			// Look through all verts before this in array to see if any are inside triangle
			for (int32 VertexIndex = 0; VertexIndex < VertIndices.Num(); VertexIndex++)
			{
				const FProcMeshVertex& TestVert = PolyVerts[VertIndices[VertexIndex]];

				if(	VertexIndex != AIndex && 
					VertexIndex != BIndex && 
					VertexIndex != CIndex &&
					FGeomTools::PointInTriangle((FVector3f)AVert.Position, (FVector3f)BVert.Position, (FVector3f)CVert.Position, (FVector3f)TestVert.Position) )
				{
					bFoundVertInside = true;
					break;
				}
			}

			// Triangle with no verts inside - its an 'ear'! 
			if (!bFoundVertInside)
			{
				OutTris.Add(VertIndices[AIndex]);
				OutTris.Add(VertIndices[CIndex]);
				OutTris.Add(VertIndices[BIndex]);

				// And remove vertex from polygon
				VertIndices.RemoveAt(EarVertexIndex);

				bFoundEar = true;
				break;
			}
		}

		// If we couldn't find an 'ear' it indicates something is bad with this polygon - discard triangles and return.
		if (!bFoundEar)
		{
			OutTris.SetNum(TriBase, true);
			return false;
		}
	}

	return true;
}


/** Util to slice a convex hull with a plane */
void SliceConvexElem(const FKConvexElem& InConvex, const FPlane& SlicePlane, TArray<FVector>& OutConvexVerts)
{
	// Get set of planes that make up hull
	TArray<FPlane> ConvexPlanes;
	InConvex.GetPlanes(ConvexPlanes);

	if (ConvexPlanes.Num() >= 4)
	{
		// Add on the slicing plane (need to flip as it culls geom in the opposite sense to our geom culling code)
		ConvexPlanes.Add(SlicePlane.Flip());

		// Create output convex based on new set of planes
		FKConvexElem SlicedElem;
		bool bSuccess = SlicedElem.HullFromPlanes(ConvexPlanes, InConvex.VertexData);
		if (bSuccess)
		{
			OutConvexVerts = SlicedElem.VertexData;
		}
	}
}


void UPSCustomProcMeshLibrary::SliceProcMesh(UProceduralMeshComponent* InProcMesh, FVector PlanePosition,
	FVector PlaneNormal, bool bCreateOtherHalf,
	UProceduralMeshComponent*& OutOtherHalfProcMesh, FSCustomSliceOutput& outSlicingData,
	EProcMeshSliceCapOption CapOption, UMaterialInterface* CapMaterial,  UMaterialInterface* OutLineMaterial)
{
	if (InProcMesh != nullptr)
	{
		// Transform plane from world to local space
		FTransform ProcCompToWorld = InProcMesh->GetComponentToWorld();
		FVector LocalPlanePos = ProcCompToWorld.InverseTransformPosition(PlanePosition);
		FVector LocalPlaneNormal = ProcCompToWorld.InverseTransformVectorNoScale(PlaneNormal);
		LocalPlaneNormal = LocalPlaneNormal.GetSafeNormal(); // Ensure normalized

		FPlane SlicePlane(LocalPlanePos, LocalPlaneNormal);

		// Set of sections to add to the 'other half' component
		TArray<FProcMeshSection> OtherSections;
		// Material for each section of other half
		TArray<UMaterialInterface*> OtherMaterials;

		// Set of new edges created by clipping polys by plane
		TArray<FUtilEdge3D> ClipEdges;

		for (int32 SectionIndex = 0; SectionIndex < InProcMesh->GetNumSections(); SectionIndex++)
		{
			FProcMeshSection* BaseSection = InProcMesh->GetProcMeshSection(SectionIndex);
			// If we have a section, and it has some valid geom
			if (BaseSection != nullptr && BaseSection->ProcIndexBuffer.Num() > 0 && BaseSection->ProcVertexBuffer.Num() > 0)
			{
				// Compare bounding box of section with slicing plane
				int32 BoxCompare = BoxPlaneCompare(BaseSection->SectionLocalBox, SlicePlane);

				// Box totally clipped, clear section
				if (BoxCompare == -1)
				{
					// Add entire section to other half
					if (bCreateOtherHalf)
					{
						OtherSections.Add(*BaseSection);
						OtherMaterials.Add(InProcMesh->GetMaterial(SectionIndex));
					}

					InProcMesh->ClearMeshSection(SectionIndex);
				}
				// Box totally on one side of plane, leave it alone, do nothing
				else if (BoxCompare == 1)
				{
					// ...
				}
				// Box intersects plane, need to clip some polys!
				else
				{
					// New section for geometry
					FProcMeshSection NewSection;

					// New section for 'other half' geometry (if desired)
					FProcMeshSection* NewOtherSection = nullptr;
					if (bCreateOtherHalf)
					{
						int32 OtherSectionIndex = OtherSections.Add(FProcMeshSection());
						NewOtherSection = &OtherSections[OtherSectionIndex];

						OtherMaterials.Add(InProcMesh->GetMaterial(SectionIndex)); // Remember material for this section
					}

					// Map of base vert index to sliced vert index
					TMap<int32, int32> BaseToSlicedVertIndex;
					TMap<int32, int32> BaseToOtherSlicedVertIndex;

					const int32 NumBaseVerts = BaseSection->ProcVertexBuffer.Num();

					// Distance of each base vert from slice plane
					TArray<float> VertDistance;
					VertDistance.AddUninitialized(NumBaseVerts);

					// Build vertex buffer 
					for (int32 BaseVertIndex = 0; BaseVertIndex < NumBaseVerts; BaseVertIndex++)
					{
						FProcMeshVertex& BaseVert = BaseSection->ProcVertexBuffer[BaseVertIndex];

						// Calc distance from plane
						VertDistance[BaseVertIndex] = SlicePlane.PlaneDot(BaseVert.Position);

						// See if vert is being kept in this section
						if (VertDistance[BaseVertIndex] > 0.f)
						{
							// Copy to sliced v buffer
							int32 SlicedVertIndex = NewSection.ProcVertexBuffer.Add(BaseVert);
							// Update section bounds
							NewSection.SectionLocalBox += BaseVert.Position;
							// Add to map
							BaseToSlicedVertIndex.Add(BaseVertIndex, SlicedVertIndex);
						}
						// Or add to other half if desired
						else if(NewOtherSection != nullptr)
						{
							int32 SlicedVertIndex = NewOtherSection->ProcVertexBuffer.Add(BaseVert);
							NewOtherSection->SectionLocalBox += BaseVert.Position;
							BaseToOtherSlicedVertIndex.Add(BaseVertIndex, SlicedVertIndex);
						}
					}


					// Iterate over base triangles (ie 3 indices at a time)
					for (int32 BaseIndex = 0; BaseIndex < BaseSection->ProcIndexBuffer.Num(); BaseIndex += 3)
					{
						int32 BaseV[3]; // Triangle vert indices in original mesh
						int32* SlicedV[3]; // Pointers to vert indices in new v buffer
						int32* SlicedOtherV[3]; // Pointers to vert indices in new 'other half' v buffer

						// For each vertex..
						for (int32 i = 0; i < 3; i++)
						{
							// Get triangle vert index
							BaseV[i] = BaseSection->ProcIndexBuffer[BaseIndex + i];
							// Look up in sliced v buffer
							SlicedV[i] = BaseToSlicedVertIndex.Find(BaseV[i]);
							// Look up in 'other half' v buffer (if desired)
							if (bCreateOtherHalf)
							{
								SlicedOtherV[i] = BaseToOtherSlicedVertIndex.Find(BaseV[i]);
								// Each base vert _must_ exist in either BaseToSlicedVertIndex or BaseToOtherSlicedVertIndex 
								check((SlicedV[i] != nullptr) != (SlicedOtherV[i] != nullptr));
							}
						}

						// If all verts survived plane cull, keep the triangle
						if (SlicedV[0] != nullptr && SlicedV[1] != nullptr && SlicedV[2] != nullptr)
						{
							NewSection.ProcIndexBuffer.Add(*SlicedV[0]);
							NewSection.ProcIndexBuffer.Add(*SlicedV[1]);
							NewSection.ProcIndexBuffer.Add(*SlicedV[2]);
						}
						// If all verts were removed by plane cull
						else if (SlicedV[0] == nullptr && SlicedV[1] == nullptr && SlicedV[2] == nullptr)
						{
							// If creating other half, add all verts to that
							if (NewOtherSection != nullptr)
							{
								NewOtherSection->ProcIndexBuffer.Add(*SlicedOtherV[0]);
								NewOtherSection->ProcIndexBuffer.Add(*SlicedOtherV[1]);
								NewOtherSection->ProcIndexBuffer.Add(*SlicedOtherV[2]);
							}
						}
						// If partially culled, clip to create 1 or 2 new triangles
						else
						{
							int32 FinalVerts[4];
							int32 NumFinalVerts = 0;

							int32 OtherFinalVerts[4];
							int32 NumOtherFinalVerts = 0;

							FUtilEdge3D NewClipEdge;
							int32 ClippedEdges = 0;

							float PlaneDist[3];
							PlaneDist[0] = VertDistance[BaseV[0]];
							PlaneDist[1] = VertDistance[BaseV[1]];
							PlaneDist[2] = VertDistance[BaseV[2]];

							for (int32 EdgeIdx = 0; EdgeIdx < 3; EdgeIdx++)
							{
								int32 ThisVert = EdgeIdx;

								// If start vert is inside, add it.
								if (SlicedV[ThisVert] != nullptr)
								{
									check(NumFinalVerts < 4);
									FinalVerts[NumFinalVerts++] = *SlicedV[ThisVert];
								}
								// If not, add to other side
								else if(bCreateOtherHalf)
								{
									check(NumOtherFinalVerts < 4);
									OtherFinalVerts[NumOtherFinalVerts++] = *SlicedOtherV[ThisVert];
								}

								// If start and next vert are on opposite sides, add intersection
								int32 NextVert = (EdgeIdx + 1) % 3;

								if ((SlicedV[EdgeIdx] == nullptr) != (SlicedV[NextVert] == nullptr))
								{
									// Find distance along edge that plane is
									float Alpha = -PlaneDist[ThisVert] / (PlaneDist[NextVert] - PlaneDist[ThisVert]);
									// Interpolate vertex params to that point
									FProcMeshVertex InterpVert = InterpolateVert(BaseSection->ProcVertexBuffer[BaseV[ThisVert]], BaseSection->ProcVertexBuffer[BaseV[NextVert]], FMath::Clamp(Alpha, 0.0f, 1.0f));

									// Add to vertex buffer
									int32 InterpVertIndex = NewSection.ProcVertexBuffer.Add(InterpVert);
									// Update bounds
									NewSection.SectionLocalBox += InterpVert.Position;

									// Save vert index for this poly
									check(NumFinalVerts < 4);
									FinalVerts[NumFinalVerts++] = InterpVertIndex;

									// If desired, add to the poly for the other half as well
									if (NewOtherSection != nullptr)
									{
										int32 OtherInterpVertIndex = NewOtherSection->ProcVertexBuffer.Add(InterpVert);
										NewOtherSection->SectionLocalBox += InterpVert.Position;
										check(NumOtherFinalVerts < 4);
										OtherFinalVerts[NumOtherFinalVerts++] = OtherInterpVertIndex;
									}

									// When we make a new edge on the surface of the clip plane, save it off.
									check(ClippedEdges < 2);
									if (ClippedEdges == 0)
									{
										NewClipEdge.V0 = (FVector3f)InterpVert.Position;
									}
									else
									{
										NewClipEdge.V1 = (FVector3f)InterpVert.Position;
									}

									ClippedEdges++;
								}
							}

							// Triangulate the clipped polygon.
							for (int32 VertexIndex = 2; VertexIndex < NumFinalVerts; VertexIndex++)
							{
								NewSection.ProcIndexBuffer.Add(FinalVerts[0]);
								NewSection.ProcIndexBuffer.Add(FinalVerts[VertexIndex - 1]);
								NewSection.ProcIndexBuffer.Add(FinalVerts[VertexIndex]);
							}

							// If we are making the other half, triangulate that as well
							if (NewOtherSection != nullptr)
							{
								for (int32 VertexIndex = 2; VertexIndex < NumOtherFinalVerts; VertexIndex++)
								{
									NewOtherSection->ProcIndexBuffer.Add(OtherFinalVerts[0]);
									NewOtherSection->ProcIndexBuffer.Add(OtherFinalVerts[VertexIndex - 1]);
									NewOtherSection->ProcIndexBuffer.Add(OtherFinalVerts[VertexIndex]);
								}
							}

							check(ClippedEdges != 1); // Should never clip just one edge of the triangle

							// If we created a new edge, save that off here as well
							if (ClippedEdges == 2)
							{
								ClipEdges.Add(NewClipEdge);
							}
						}
					}

					// Remove 'other' section from array if no valid geometry for it
					if (NewOtherSection != nullptr && (NewOtherSection->ProcIndexBuffer.Num() == 0 || NewOtherSection->ProcVertexBuffer.Num() == 0))
					{
						OtherSections.RemoveAt(OtherSections.Num() - 1);
					}

					// If we have some valid geometry, update section
					if (NewSection.ProcIndexBuffer.Num() > 0 && NewSection.ProcVertexBuffer.Num() > 0)
					{
						// Assign new geom to this section
						InProcMesh->SetProcMeshSection(SectionIndex, NewSection);
					}
					// If we don't, remove this section
					else
					{
						InProcMesh->ClearMeshSection(SectionIndex);
					}
				}
			}
		}

		// Create cap geometry (if some edges to create it from)
		if (CapOption != EProcMeshSliceCapOption::NoCap && ClipEdges.Num() > 0)
		{
			FProcMeshSection CapSection;
			int32 CapSectionIndex = INDEX_NONE;

			// If using an existing section, copy that info first
			if (CapOption == EProcMeshSliceCapOption::UseLastSectionForCap)
			{
				CapSectionIndex = InProcMesh->GetNumSections() - 1;
				CapSection = *InProcMesh->GetProcMeshSection(CapSectionIndex);
			}
			// Adding new section for cap
			else
			{
				CapSectionIndex = InProcMesh->GetNumSections();
			}

			// Project 3D edges onto slice plane to form 2D edges
			TArray<FUtilEdge2D> Edges2D;
			FUtilPoly2DSet PolySet;
			FGeomTools::ProjectEdges(Edges2D, PolySet.PolyToWorld, ClipEdges, SlicePlane);

			// Find 2D closed polygons from this edge soup
			FGeomTools::Buid2DPolysFromEdges(PolySet.Polys, Edges2D, FColor(255, 255, 255, 255));

			// Remember start point for vert and index buffer before adding and cap geom
			int32 CapVertBase = CapSection.ProcVertexBuffer.Num();
			int32 CapIndexBase = CapSection.ProcIndexBuffer.Num();

			// Triangulate each poly
			for (int32 PolyIdx = 0; PolyIdx < PolySet.Polys.Num(); PolyIdx++)
			{
				// Generate UVs for the 2D polygon.
				FGeomTools::GeneratePlanarTilingPolyUVs(PolySet.Polys[PolyIdx], 64.f);

				// Remember start of vert buffer before adding triangles for this poly
				int32 PolyVertBase = CapSection.ProcVertexBuffer.Num();

				// Transform from 2D poly verts to 3D
				Transform2DPolygonTo3D(PolySet.Polys[PolyIdx], PolySet.PolyToWorld, CapSection.ProcVertexBuffer, CapSection.SectionLocalBox);

				// Triangulate this polygon
				TriangulatePoly(CapSection.ProcIndexBuffer, CapSection.ProcVertexBuffer, PolyVertBase, (FVector3f)LocalPlaneNormal);
			}

			// Set geom for cap section
			InProcMesh->SetProcMeshSection(CapSectionIndex, CapSection);

			// If creating new section for cap, assign cap material to it
			if (CapOption == EProcMeshSliceCapOption::CreateNewSectionForCap)
			{
				InProcMesh->SetMaterial(CapSectionIndex, CapMaterial);
				if(IsValid(OutLineMaterial))
					InProcMesh->SetMaterial(CapSectionIndex, OutLineMaterial);
				
				//Storage InProc modified material for differed usage
				outSlicingData.InProcMeshCapIndex = CapSectionIndex;
				outSlicingData.InProcMeshDefaultMat = InProcMesh->OverrideMaterials[CapSectionIndex];

				//UE_LOG(LogTemp, Error, TEXT("index %i ,mat %s"), CapSectionIndex, *GetNameSafe(outSlicingData.InProcMeshDefaultMat));
			}

			// If creating the other half, copy cap geom into other half sections
			if (bCreateOtherHalf)
			{
				// Find section we want to use for the cap on the 'other half'
				FProcMeshSection* OtherCapSection;
				if (CapOption == EProcMeshSliceCapOption::CreateNewSectionForCap)
				{
					OtherSections.Add(FProcMeshSection());
					OtherMaterials.Add(CapMaterial);
					if(IsValid(OutLineMaterial))
						OtherMaterials.Add(OutLineMaterial);
					
					//Storage OutProc modified material for differed usage
					outSlicingData.OutProcMeshCapIndex = OtherSections.Num() - 1;
				}
				OtherCapSection = &OtherSections.Last();

				// Remember current base index for verts in 'other cap section'
				int32 OtherCapVertBase = OtherCapSection->ProcVertexBuffer.Num();

				// Copy verts from cap section into other cap section
				for (int32 VertIdx = CapVertBase; VertIdx < CapSection.ProcVertexBuffer.Num(); VertIdx++)
				{
					FProcMeshVertex OtherCapVert = CapSection.ProcVertexBuffer[VertIdx];

					// Flip normal and tangent TODO: FlipY?
					OtherCapVert.Normal *= -1.f;
					OtherCapVert.Tangent.TangentX *= -1.f;

					// Add to other cap v buffer
					OtherCapSection->ProcVertexBuffer.Add(OtherCapVert);
					// And update bounding box
					OtherCapSection->SectionLocalBox += OtherCapVert.Position;
				}

				// Find offset between main cap verts and other cap verts
				int32 VertOffset = OtherCapVertBase - CapVertBase;

				// Copy indices over as well
				for (int32 IndexIdx = CapIndexBase; IndexIdx < CapSection.ProcIndexBuffer.Num(); IndexIdx += 3)
				{
					// Need to offset and change winding
					OtherCapSection->ProcIndexBuffer.Add(CapSection.ProcIndexBuffer[IndexIdx + 0] + VertOffset);
					OtherCapSection->ProcIndexBuffer.Add(CapSection.ProcIndexBuffer[IndexIdx + 2] + VertOffset);
					OtherCapSection->ProcIndexBuffer.Add(CapSection.ProcIndexBuffer[IndexIdx + 1] + VertOffset);
				}
			}
		}

		// Array of sliced collision shapes
		TArray< TArray<FVector> > SlicedCollision;
		TArray< TArray<FVector> > OtherSlicedCollision;

		UBodySetup* ProcMeshBodySetup = InProcMesh->GetBodySetup();

		for (int32 ConvexIndex = 0; ConvexIndex < ProcMeshBodySetup->AggGeom.ConvexElems.Num(); ConvexIndex++)
		{
			FKConvexElem& BaseConvex = ProcMeshBodySetup->AggGeom.ConvexElems[ConvexIndex];

			int32 BoxCompare = BoxPlaneCompare(BaseConvex.ElemBox, SlicePlane);

			// If box totally clipped, add to other half (if desired)
			if (BoxCompare == -1)
			{
				if (bCreateOtherHalf)
				{
					OtherSlicedCollision.Add(BaseConvex.VertexData);
				}
			}
			// If box totally valid, just keep mesh as is
			else if (BoxCompare == 1)
			{
				SlicedCollision.Add(BaseConvex.VertexData);				// LWC_TODO: Perf pessimization
			}
			// Need to actually slice the convex shape
			else
			{
				TArray<FVector> SlicedConvexVerts;
				SliceConvexElem(BaseConvex, SlicePlane, SlicedConvexVerts);
				// If we got something valid, add it
				if (SlicedConvexVerts.Num() >= 4)
				{
					SlicedCollision.Add(SlicedConvexVerts);
				}

				// Slice again to get the other half of the collision, if desired
				if (bCreateOtherHalf)
				{
					TArray<FVector> OtherSlicedConvexVerts;
					SliceConvexElem(BaseConvex, SlicePlane.Flip(), OtherSlicedConvexVerts);
					if (OtherSlicedConvexVerts.Num() >= 4)
					{
						OtherSlicedCollision.Add(OtherSlicedConvexVerts);
					}
				}
			}
		}

		// Update collision of proc mesh
		InProcMesh->SetCollisionConvexMeshes(SlicedCollision);

		// If creating other half, create component now
		if (bCreateOtherHalf)
		{
			// Create new component with the same outer as the proc mesh passed in
			OutOtherHalfProcMesh = NewObject<UProceduralMeshComponent>(InProcMesh->GetOuter());

			// Set transform to match source component
			OutOtherHalfProcMesh->SetWorldTransform(InProcMesh->GetComponentTransform());

			// Add each section of geometry
			for (int32 SectionIndex = 0; SectionIndex < OtherSections.Num(); SectionIndex++)
			{
				OutOtherHalfProcMesh->SetProcMeshSection(SectionIndex, OtherSections[SectionIndex]);
				OutOtherHalfProcMesh->SetMaterial(SectionIndex, OtherMaterials[SectionIndex]);
			}

			// Copy collision settings from input mesh
			OutOtherHalfProcMesh->SetCollisionProfileName(InProcMesh->GetCollisionProfileName());
			OutOtherHalfProcMesh->SetCollisionEnabled(InProcMesh->GetCollisionEnabled());
			OutOtherHalfProcMesh->bUseComplexAsSimpleCollision = InProcMesh->bUseComplexAsSimpleCollision;

			// Assign sliced collision
			OutOtherHalfProcMesh->SetCollisionConvexMeshes(OtherSlicedCollision);

			// Finally register
			OutOtherHalfProcMesh->RegisterComponent();
		}
	}
}
