// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_SlicedComponent.h"
#include "KismetProceduralMeshLibrary.h"



// Sets default values for this component's properties
UPS_SlicedComponent::UPS_SlicedComponent(const FObjectInitializer& objectInitializer) : Super(objectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
		
		
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

	// ...
}

void UPS_SlicedComponent::InitSliceObject()
{
	UStaticMeshComponent* rootMesh = Cast<UStaticMeshComponent>(GetOwner()->GetRootComponent());

	if(!IsValid(rootMesh)) return;

	//Init StaticMeshCompo
	//TODO:: Destroy base mesh
	rootMesh->SetSimulatePhysics(false);
	rootMesh->SetGenerateOverlapEvents(false);
	rootMesh->SetCollisionProfileName(TEXT("NoCollision"), true);
	rootMesh->SetVisibility(false);
	
	//Copy StaticMesh to ProceduralMesh
	//TODO :: See LOD index for complex mesh
	UKismetProceduralMeshLibrary::CopyProceduralMeshFromStaticMeshComponent(rootMesh,0,this,true);

	//Init ProcMeshCompo Collision
	bUseComplexAsSimpleCollision = false;
	SetGenerateOverlapEvents(false);
	SetSimulatePhysics(true);
	SetCollisionProfileName(TEXT("PhysicsActor"), true);
	
	
}


