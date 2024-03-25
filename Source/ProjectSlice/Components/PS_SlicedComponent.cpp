// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_SlicedComponent.h"
#include "KismetProceduralMeshLibrary.h"



// Sets default values for this component's properties
UPS_SlicedComponent::UPS_SlicedComponent(const FObjectInitializer& objectInitializer) : Super(objectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	//Set component collision
	SetCollisionProfileName(TEXT("NoCollision"), true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);

	//Create ProceduralMeshComponent
	ProcMeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMeshComponent"));
	

}


// Called when the game starts
void UPS_SlicedComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
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
	RootMesh = Cast<UStaticMeshComponent>(GetOwner()->GetRootComponent());

	if(!IsValid(RootMesh) || !IsValid(ProcMeshComp)) return;

	//Init StaticMeshCompo
	RootMesh->SetGenerateOverlapEvents(true);
	//RootMesh->SetCollisionProfileName(TEXT("NoCollision"), true);
	//RootMesh->SetVisibility(false);

	//Init ProcMeshCompo
	ProcMeshComp->bUseComplexAsSimpleCollision = false;
	ProcMeshComp->SetGenerateOverlapEvents(true);
	ProcMeshComp->SetCollisionProfileName(TEXT("BlockAll"), true);
	ProcMeshComp->SetSimulatePhysics(true);

	//Copy StaticMesh to ProceduralMesh
	//TODO :: See LOD index for complex mesh
	UKismetProceduralMeshLibrary::CopyProceduralMeshFromStaticMeshComponent(RootMesh,0,ProcMeshComp,true);
	
}


