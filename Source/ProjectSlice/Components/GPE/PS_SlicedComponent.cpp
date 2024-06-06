// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_SlicedComponent.h"
#include "KismetProceduralMeshLibrary.h"
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
	_RootMesh = Cast<UStaticMeshComponent>(GetOwner()->GetRootComponent());

	if(!IsValid(_RootMesh)) return;

	//Set Transform
	SetRelativeLocation(_RootMesh->GetRelativeLocation());
	SetRelativeRotation(_RootMesh->GetRelativeRotation());
	
	//Init StaticMeshCompo Collision and Physic
	//TODO:: Need Destroy base mesh
	_RootMesh->SetSimulatePhysics(false);
	_RootMesh->SetGenerateOverlapEvents(false);
	_RootMesh->SetCollisionProfileName(Profile_GPE, true);
	_RootMesh->SetVisibility(false);
	
	//Copy StaticMesh to ProceduralMesh
	//TODO :: See LOD index for complex mesh
	UKismetProceduralMeshLibrary::CopyProceduralMeshFromStaticMeshComponent(_RootMesh,0,this,true);
	
	
}



