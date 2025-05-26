// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_SlicedComponent.h"

#include "Editor.h"
#include "KismetProceduralMeshLibrary.h"
#include "Components/AudioComponent.h"
#include "Components/BrushComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectSlice/Data/PS_Constants.h"
#include "ProjectSlice/Data/PS_TraceChannels.h"
#include "ProjectSlice/FunctionLibrary/PSFl.h"


// Sets default values for this component's properties
UPS_SlicedComponent::UPS_SlicedComponent(const FObjectInitializer& objectInitializer) : Super(objectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	//Init ProcMeshCompo Collision
	bUseComplexAsSimpleCollision = false;
	UPrimitiveComponent::SetCollisionProfileName(Profile_PhysicActor, false);
	SetGenerateOverlapEvents(true);
	UPrimitiveComponent::SetNotifyRigidBodyCollision(true);
	UPrimitiveComponent::SetSimulatePhysics(true);
}


// Called when the game starts
void UPS_SlicedComponent::BeginPlay()
{
	Super::BeginPlay();

	InitComponent();
	
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
	
	Cast<UBrushComponent>(GetOwner()->GetRootComponent());
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
	//UPSFl::CreateMeshFromProcMesh(this);
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

void UPS_SlicedComponent::InitComponent()
{
	OnComponentHit.AddUniqueDynamic(this, &UPS_SlicedComponent::OnSlicedObjectHitEventReceived);
	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: hitComp %s"), __FUNCTION__, *this->GetName());	
}


void UPS_SlicedComponent::OnSlicedObjectHitEventReceived(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if(OtherComp == this) return;
	
	//float impactStrength = NormalImpulse.Size(); 
	float impactStrength = HitComponent->GetComponentVelocity().Size();
	if(GetComponentVelocity().Z < MinVelocityZForFeedback || impactStrength < VelocityRangeSound.Min) return;

	//Cooldown
	float currentTime = GetWorld()->GetTimeSeconds();
	if (currentTime - _LastImpactSoundTime < _ImpactSoundCooldown) return;
	_LastImpactSoundTime = currentTime;

	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: OtherActor %s, comp %s, impactStrength %f"),__FUNCTION__, *GetNameSafe(OtherActor), *GetNameSafe(OtherComp),impactStrength);
	
	// Try and play the sound if specified
	if (IsValid(CrashSound))
	{
		FVector loc = GetComponentLocation();
		if (Hit.bBlockingHit) loc = Hit.ImpactPoint;

		//volumeMultiplier
		float volumeMultiplier = FMath::Clamp(
			FMath::GetMappedRangeValueClamped(
				FVector2D(VelocityRangeSound.Min, VelocityRangeSound.Max),
				FVector2D(VolumeRangeMin, 1.0f),
				impactStrength),
			VolumeRangeMin, 1.0f);
		
		//Sound Attenuation
		_FallingAudio = UGameplayStatics::SpawnSoundAtLocation(this, CrashSound,loc,FRotator::ZeroRotator,volumeMultiplier);
		_FallingAudio->Play();

	}
}




