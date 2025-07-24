// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_SlicedComponent.h"

#include "Editor.h"
#include "KismetProceduralMeshLibrary.h"
#include "Components/AudioComponent.h"
#include "Components/BrushComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Data/PS_Constants.h"
#include "ProjectSlice/Data/PS_TraceChannels.h"
#include "ProjectSlice/FunctionLibrary/PSFl.h"


// Sets default values for this component's properties
UPS_SlicedComponent::UPS_SlicedComponent(const FObjectInitializer& objectInitializer) : Super(objectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

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

	OnComponentHit.AddUniqueDynamic(this, &UPS_SlicedComponent::OnSlicedObjectHitEventReceived);
	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: hitComp %s"), __FUNCTION__, *this->GetName());
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
	GetBodyInstance()->WakeInstance();

	//Init Physic
	const bool bIsNotFixed = GetOwner()->ActorHasTag(TAG_UNFIXED);
	SetSimulatePhysics(bIsNotFixed);
	
}

void UPS_SlicedComponent::InitComponent()
{
	//Update
	UpdateBounds();

	//Init Collision 
	bUseComplexAsSimpleCollision = false;
	SetGenerateOverlapEvents(true);
	SetCollisionProfileName(Profile_GPE, true);
	SetNotifyRigidBodyCollision(true);
	GetBodyInstance()->WakeInstance();
}


void UPS_SlicedComponent::OnSlicedObjectHitEventReceived(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if(OtherActor == GetOwner() || OtherComp == this) return;

	//Bounds
	 FVector origin; 
     FVector boxExtent;
	 float radius;
	 UKismetSystemLibrary::GetComponentBounds(this, origin, boxExtent, radius);
	const float extent = UKismetMathLibrary::Vector_Distance(origin,boxExtent);

	//Mass scale factoring
	float objectMassScaled =UPSFl::GetObjectUnifiedMass(this);
	if(FMath::IsNearlyZero(objectMassScaled)) objectMassScaled = 1.0f;
	const float impactStrength = UKismetMathLibrary::SafeDivide((GetComponentVelocity() * objectMassScaled).Length(), extent);
	if(impactStrength < MinVelocityZForFeedback) return;
	
	//Cooldown
	float currentTime = GetWorld()->GetTimeSeconds();
	if (currentTime - _LastImpactSoundTime < _ImpactSoundCooldown) return;
	_LastImpactSoundTime = currentTime;
	
	// Try and play the sound if specified
	const float alpha = UKismetMathLibrary::MapRangeClamped(impactStrength,VelocityRangeSound.Min, VelocityRangeSound.Max, 0.0f, 1.0f);
	_LastImpactFeedbackForce = FMath::InterpStep(0,4,alpha,5);
	if (IsValid(CrashSound))
	{
		FVector loc = GetComponentLocation();
		if (Hit.bBlockingHit) loc = Hit.ImpactPoint;

		//VolumeMultiplier calculation
		float volumeMultiplier = FMath::Lerp(VolumeRange.Min, VolumeRange.Max,alpha);

		//PitchMultiplier calculation
		float pitchMultiplier = FMath::Lerp(PitchRange.Min, PitchRange.Max,alpha);
		
		//Play sound
		_CollideAudio = UGameplayStatics::SpawnSoundAtLocation(this, CrashSound,loc,FRotator::ZeroRotator,volumeMultiplier, pitchMultiplier);
		if(IsValid(_CollideAudio))
		{
			_CollideAudio->SetIntParameter(FName("Intensity"), _LastImpactFeedbackForce);
			_CollideAudio->Play();
		}

	}
	
	if(bDebug) UE_LOG(LogTemp, Log, TEXT("%S :: OtherActor %s, comp %s, impactStrength %f,alpha %f, _LastImpactFeedbackForce %i, extent %f, objectMassScaled %f"),__FUNCTION__, *GetNameSafe(OtherActor), *GetNameSafe(OtherComp), impactStrength,alpha, _LastImpactFeedbackForce, extent, objectMassScaled);

	//Callback
	OnSlicedObjectHitEvent.Broadcast();
}




