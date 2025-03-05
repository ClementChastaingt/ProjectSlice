// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_SlicedComponent.h"

#include "Editor.h"
#include "KismetProceduralMeshLibrary.h"
#include "Components/BrushComponent.h"
#include "ProjectSlice/Data/PS_Constants.h"
#include "ProjectSlice/Data/PS_TraceChannels.h"


// Sets default values for this component's properties
UPS_SlicedComponent::UPS_SlicedComponent(const FObjectInitializer& objectInitializer) : Super(objectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	//Init ProcMeshCompo Collision
	bUseComplexAsSimpleCollision = false;
	SetCollisionProfileName(Profile_PhysicActor, false);
	SetGenerateOverlapEvents(false);
	SetNotifyRigidBodyCollision(true);
	SetSimulatePhysics(true);
		
}


// Called when the game starts
void UPS_SlicedComponent::BeginPlay()
{
	Super::BeginPlay();
	
}


// Called every frame
void UPS_SlicedComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                       FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);


	if(!IsValid(_RootMesh)) return;
	
	
}

void UPS_SlicedComponent::InitSliceObject()
{

	if(!IsValid(GetOwner())) return;
	
	// Cast<UBrushComponent>(GetOwner()->GetRootComponent());
	_RootMesh = Cast<UStaticMeshComponent>(GetOwner()->GetRootComponent());

	if(!IsValid(_RootMesh))
	{
		UE_LOG(LogTemp, Error, TEXT("%S :: %s _RootMesh invalid"), __FUNCTION__, *GetNameSafe(GetOwner()));
		return;
	}
		
	//Copy StaticMesh to ProceduralMesh
	//TODO :: See LOD index for complex mesh
	UKismetProceduralMeshLibrary::CopyProceduralMeshFromStaticMeshComponent(_RootMesh,0,this,true);

	//Set proc new Root and set Transform
	GetOwner()->SetRootComponent(this);
	SetWorldTransform(_RootMesh->GetComponentTransform());
	SetMassScale(NAME_None, _RootMesh->GetMassScale());
	this->SetAffectDistanceFieldLighting(true);
	//SetRelativeTransform(_RootMesh->GetRelativeTransform());
	
	//Destroy base StaticMesh comp
	UKismetProceduralMeshLibrary::Mesh
	this->MeshD
	NewStaticMesh->GetMeshDescription(0) = MeshDescription;
	_RootMesh->DestroyComponent(true);

	//Init Collision 
	bUseComplexAsSimpleCollision = false;
	SetGenerateOverlapEvents(true);
	SetCollisionProfileName(Profile_GPE, true);
	SetNotifyRigidBodyCollision(true);

	//Init Physic
	const bool bIsNotFixed = GetOwner()->ActorHasTag(TAG_UNFIXED);
	SetSimulatePhysics(bIsNotFixed);
	
}



