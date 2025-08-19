// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_ForceComponent.h"

#include "Field/FieldSystemComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectSlice/Character/PC/PS_Character.h"
#include "ProjectSlice/Data/PS_Constants.h"
#include "ProjectSlice/Data/PS_GlobalType.h"
#include "ProjectSlice/Data/PS_TraceChannels.h"
#include "ProjectSlice/FunctionLibrary/PSFl.h"


class UPS_SlicedComponent;
// Sets default values for this component's properties
UPS_ForceComponent::UPS_ForceComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UPS_ForceComponent::BeginPlay()
{
	Super::BeginPlay();

	_PlayerCharacter = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if(!IsValid(_PlayerCharacter)) return;
	
	_PlayerController = Cast<AProjectSlicePlayerController>(_PlayerCharacter->GetController());
	if(!IsValid(_PlayerController)) return;
	
	//Screw Attach 
	AttachScrew();

	//Callback
	OnPushReleaseNotifyEvent.AddUniqueDynamic(this, &UPS_ForceComponent::OnPushReleasedEventReceived);
	if(IsValid(_PlayerCharacter->GetProceduralAnimComponent()))
	{
		_PlayerCharacter->GetProceduralAnimComponent()->OnScrewResetEnd.AddUniqueDynamic(this, &UPS_ForceComponent::OnScrewResetEndEventReceived);
	}

	if(IsValid(_PlayerCharacter->GetWeaponComponent()))
	{
		_PlayerCharacter->GetWeaponComponent()->OnFireTriggeredEvent.AddUniqueDynamic(this, &UPS_ForceComponent::OnFireTriggeredEventReceived);
		_PlayerCharacter->GetWeaponComponent()->OnFireEvent.AddUniqueDynamic(this, &UPS_ForceComponent::OnFireEventReceived);
		_PlayerCharacter->GetWeaponComponent()->OnToggleTurnRackTargetEvent.AddUniqueDynamic(this, &UPS_ForceComponent::OnToggleTurnRackTargetEventReceived);
		_PlayerCharacter->GetWeaponComponent()->OnTurnRackEvent.AddUniqueDynamic(this, &UPS_ForceComponent::OnTurnRackEventReceived);
	}
	
}

// Called every frame
void UPS_ForceComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if(IsValid(_PlayerCharacter)) _CurrentPushHitResult = _PlayerCharacter->GetWeaponComponent()->GetSightHitResult();
	
	UpdatePushTargetLoc();

	UpdateImpactField();
}

#pragma region General
//------------------

void UPS_ForceComponent::UpdatePushTargetLoc() const
{
	if(!_bIsPushLoading) return;
	
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetWeaponComponent())) return;
	
	if(!_CurrentPushHitResult.bBlockingHit
		|| !IsValid(_CurrentPushHitResult.GetActor())
		|| bIsQuickPush
		|| (!_CurrentPushHitResult.GetActor()->ActorHasTag(TAG_GPE_SLICEABLE) && !_CurrentPushHitResult.GetActor()->ActorHasTag(TAG_GPE_CHAOS)))
	{
		OnSpawnPushDistorsion.Broadcast(false);
		return;
	}
	else
		OnSpawnPushDistorsion.Broadcast(true);
	
	//Target Loc
	//_PushTargetLoc = _PlayerCharacter->GetWeaponComponent()->GetSightHitResult().Location;
	FVector dir = (_CurrentPushHitResult.Normal * - 1) + _PlayerCharacter->GetActorForwardVector();
	dir.Normalize();
	
	//FVector start = _CurrentPushHitResult.Location;
	FVector start = _PlayerCharacter->GetWeaponComponent()->GetLaserTarget();
	
	const FVector pushTargetLoc = start + dir * -10.0f;

	//Target Rot
	const FRotator pushTargetRot = UKismetMathLibrary::FindLookAtRotation(start + _CurrentPushHitResult.Normal * -500, pushTargetLoc);

	//Debug
	//if(bDebugPush) DrawDebugLine(GetWorld(), start, start + dir * -100, FColor::Yellow, false, 2, 10, 3);
	OnPushTargetUpdate.Broadcast(pushTargetLoc,pushTargetRot);
	
}

void UPS_ForceComponent::OnFireEventReceived()
{
	if (_bIsPushing || _bIsPushLoading) StopPush();
}

void UPS_ForceComponent::OnFireTriggeredEventReceived()
{
	if (_bIsPushing || _bIsPushLoading) StopPush();
}

void UPS_ForceComponent::OnToggleTurnRackTargetEventReceived(const bool bActive)
{
	if (_bIsPushing || _bIsPushLoading) StopPush();
}

void UPS_ForceComponent::OnTurnRackEventReceived()
{
	if (_bIsPushing || _bIsPushLoading) StopPush();
}

//------------------
#pragma endregion General

#pragma region Push
//------------------

void UPS_ForceComponent::UnloadPush()
{
	if(bDebugPush) UE_LOG(LogTemp, Log, TEXT("%S"),__FUNCTION__);
	
	_ReleasePushTimestamp = GetWorld()->GetAudioTimeSeconds();
	_bIsPushLoading = false;
	DeterminePushType();
	OnPushEvent.Broadcast(_bIsPushLoading);
}

void UPS_ForceComponent::ReleasePush()
{
	if(GetWorld()->GetTimerManager().IsTimerActive(_CoolDownTimerHandle)) return;
	
	if(!IsValid(_PlayerCharacter) || !IsValid(_PlayerCharacter->GetWeaponComponent()) || !IsValid(GetWorld()))
	{
		StopPush();
		return;
	}
	
	//Setup force var
	_AlphaInput = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetAudioTimeSeconds(), _StartForcePushTimestamp,_StartForcePushTimestamp + InputMaxPushForceDuration,0.5f, 1.0f);
	const float initialForce = PushForce * _AlphaInput;
	float mass = UPSFl::GetObjectUnifiedMass(_CurrentPushHitResult.GetComponent());
		
	//Determine dir
	if(bIsQuickPush)
	{
		_DirForcePush = _PlayerCharacter->GetFirstPersonCameraComponent()->GetForwardVector();
		_DirForcePush.Normalize();
		_StartForceHandOffset =  UKismetMathLibrary::Vector_Distance(_PlayerCharacter->GetMesh()->GetSocketLocation(SOCKET_HAND_LEFT), _PlayerCharacter->GetActorLocation());
		_StartForcePushLoc = _PlayerCharacter->GetMesh()->GetSocketLocation(SOCKET_HAND_LEFT) + _DirForcePush * QuickPushStartLocOffset;
	}
	else
	{
		_DirForcePush = (_CurrentPushHitResult.Normal * - 1) + _PlayerCharacter->GetActorForwardVector();
		_DirForcePush.Normalize();
		_StartForcePushLoc = _CurrentPushHitResult.Location - _DirForcePush * (ConeLength/3);
	}
	
	//Cone raycast
	TArray<FHitResult> outHits;
	TArray<AActor*> actorsToIgnore;
	actorsToIgnore.AddUnique(_PlayerCharacter);

	UPSFl::SweepConeMultiByChannel(GetWorld(),_StartForcePushLoc, _DirForcePush,ConeAngleDegrees, ConeLength, StepInterval,outHits, ECC_GPE, actorsToIgnore, bDebugPush);

	//Sort Hits result
	TArray<FHitResult> filteredHitResult;
	SortPushTargets(outHits, filteredHitResult);
	
	//Impulse
	int32 iteration = 0;
	for (FHitResult outHitResult : filteredHitResult)
	{
		//Check comp type
		UMeshComponent* outMeshComp = Cast<UMeshComponent>(outHitResult.GetComponent());
		UGeometryCollectionComponent* outGeometryComp = Cast<UGeometryCollectionComponent>(outHitResult.GetComponent());

		if((!IsValid(outMeshComp) && !IsValid(outGeometryComp)) || outMeshComp->Mobility != EComponentMobility::Movable )
		{
			iteration++;
			continue;
		}
		
		//Testing id object is blocking view line
		FHitResult lineViewHit;
		UKismetSystemLibrary::LineTraceSingle(GetWorld(), _StartForcePushLoc, outHitResult.ImpactPoint, UEngineTypes::ConvertToTraceType(ECC_Visibility),
			false, actorsToIgnore, bDebugPush ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, lineViewHit, true);

		if (lineViewHit.bBlockingHit && lineViewHit.GetActor() != outHitResult.GetActor())
		{
			UE_LOG(LogTemp, Warning, TEXT("%S :: %s is not in Push line view "),__FUNCTION__, *outHitResult.GetActor()->GetActorNameOrLabel());
			iteration++;
			continue;
		}

		//Determine response latency
		const float currentDistSquared = UKismetMathLibrary::Vector_DistanceSquared(_StartForcePushLoc, outHitResult.Location);
		const float maxDistSquared = FMath::Square(ConeLength * 1.5);
		
		const float alpha = FMath::Clamp(currentDistSquared / maxDistSquared, 0.f, 1.f);		
		const float duration = MaxPushForceTime * alpha;
		const float force = initialForce * (1 - duration/MaxPushForceTime);

		if(bDebugPush) UE_LOG(LogTemp, Log, TEXT("%S :: bIsQuickPush %i"), __FUNCTION__, bIsQuickPush);
		
		//Chaos
		//Field is moving so we don't need to create multiple of them
		if(IsValid(outGeometryComp) && !IsValid(_ImpactField) && !_bCanMoveField)
		{
			const float radius = FMath::Sqrt(currentDistSquared) * FMath::DegreesToRadians(ConeAngleDegrees);

			//Destruct with delay for match with VFX
			FTimerHandle timerChaosHandle;
			FTimerDelegate timerChaosDelegate;
			
			timerChaosDelegate.BindUFunction(this, FName("GenerateImpactField"), outHitResult, FVector::One() * radius); 
			GetWorld()->GetTimerManager().SetTimer(timerChaosHandle, timerChaosDelegate, duration, false);

			if(bDebugPush) UE_LOG(LogTemp, Log, TEXT("%S :: Chaos "), __FUNCTION__);

			//Increment iteration var 
			iteration++;
			continue;
		}

		//Impulse
		if(IsValid(outMeshComp))
		{
			//If not mobile break
			if (!outMeshComp->GetComponentVelocity().IsNearlyZero())
			{
				iteration++;
				continue;
			}
						
			//Calculate mass for weight force 
			mass = UPSFl::GetObjectUnifiedMass(outMeshComp);

			//Impulse with delay for match with VFX
			FTimerHandle timerImpulseHandle;
			FTimerDelegate timerImpulseDelegate;
				
			timerImpulseDelegate.BindUObject(this, &UPS_ForceComponent::Impulse,outMeshComp, _StartForcePushLoc + _DirForcePush * (force * mass), lineViewHit); 
			GetWorld()->GetTimerManager().SetTimer(timerImpulseHandle, timerImpulseDelegate, duration, false);
			
			if(bDebugPush) UE_LOG(LogTemp, Log, TEXT("%S :: Impulse actor %s, compHit %s,  force %f, mass %f, pushForce %f, alphainput %f, duration %f"),__FUNCTION__,*outMeshComp->GetOwner()->GetActorNameOrLabel(), *outMeshComp->GetName(), force, mass, PushForce, _AlphaInput, duration);

			//Increment iteration var 
			iteration++;
		}
	}

	//---Feedbacks----
	//Push cone burst feedback
	_PushForceRotation = UKismetMathLibrary::FindLookAtRotation(_StartForcePushLoc, _StartForcePushLoc + _DirForcePush * ConeLength).Clamp();
	OnSpawnPushBurst.Broadcast(_StartForcePushLoc, _DirForcePush * ConeLength);
		
	//Play sounds
	//Released
	FTimerHandle timerHandle;
	FTimerDelegate timerDelegate;

	timerDelegate.BindUObject(this, &UPS_ForceComponent::PlaySound, EPushSFXType::RELEASED); 
	GetWorld()->GetTimerManager().SetTimer(timerHandle, timerDelegate, PushSoundStartDelay, false);

	//CameraShake
	const float alphaShake = (_AlphaInput < 0.25f) ? 0.25f : _AlphaInput;
	_PlayerCharacter->GetFirstPersonCameraComponent()->ShakeCamera(EScreenShakeType::FORCE, alphaShake);

	//Stop push
	StopPush();

}

void UPS_ForceComponent::OnPushReleasedEventReceived()
{
	//Release force when screw reseting finished
	ReleasePush();
}

void UPS_ForceComponent::DeterminePushType()
{
	//Quick pushing check && if player target is valid, if don't do quickPush logic
	bIsQuickPush = GetWorld()->GetAudioTimeSeconds() - _StartForcePushTimestamp <= QuickPushTimeThreshold;
	UE_LOG(LogTemp, Error, TEXT("bIsQuickPush %i "), bIsQuickPush);
	if(!bIsQuickPush)
	{
		bIsQuickPush =
			!_CurrentPushHitResult.bBlockingHit
			|| !IsValid(_CurrentPushHitResult.GetActor())
			|| !IsValid(_CurrentPushHitResult.GetComponent())
			|| !_CurrentPushHitResult.GetComponent()->IsSimulatingPhysics();

		//UE_LOG(LogTemp, Error, TEXT("%i 01, %i 02"), !_CurrentPushHitResult.bBlockingHit, _CurrentPushHitResult.GetComponent()->IsSimulatingPhysics());
		//DrawDebugPoint(GetWorld(), _CurrentPushHitResult.Location, 20.f, FColor::Orange, false, 2.0f);
	}
	UE_LOG(LogTemp, Error, TEXT("bIsQuickPush02 %i "), bIsQuickPush);
}

void UPS_ForceComponent::SortPushTargets(const TArray<FHitResult>& hitsToSort, UPARAM(Ref) TArray<FHitResult>& outFilteredHitResult)
{
	// Remove duplicate components
	TSet<UPrimitiveComponent*> uniqueComp;
	for (FHitResult outHit : hitsToSort)
	{
		if(!IsValid(outHit.GetComponent())) continue;
		
		if (outHit.GetComponent() && !uniqueComp.Contains(outHit.GetComponent()))
		{
			uniqueComp.Add(outHit.GetComponent());
			outFilteredHitResult.Add(outHit);
		}
	}

	// Sort by distance (nearest to farthest)
	Algo::Sort(outFilteredHitResult, [](const FHitResult& A, const FHitResult& B)
	{
		return A.Distance < B.Distance;
	});

}

void UPS_ForceComponent::Impulse(UMeshComponent* inComp, const FVector impulse, const FHitResult impactPoint)
{
	//impulse
	inComp->AddImpulse(impulse, NAME_None, false);

	//Play Impact sound
	_ImpactForcePushLoc = impactPoint.ImpactPoint;
	PlaySound(EPushSFXType::IMPACT);
}

void UPS_ForceComponent::SetupPush()
{
	if(!IsValid(GetWorld())) return;

	if(_bIsPushLoading || GetWorld()->GetTimerManager().IsTimerActive(_CoolDownTimerHandle)) return;

	if(bDebugPush) UE_LOG(LogTemp, Log, TEXT("%S"),__FUNCTION__);
	
	_StartForcePushTimestamp = GetWorld()->GetAudioTimeSeconds();

	_bIsPushing = true;
	_bIsPushLoading = true;
	_bIsPushReleased = false;
	OnPushEvent.Broadcast(_bIsPushing);
}

void UPS_ForceComponent::StopPush()
{
	if(bDebugPush) UE_LOG(LogTemp, Log, TEXT("%S"),__FUNCTION__);

	_bIsPushing = false;
	_bIsPushLoading = false;
	_bIsPushReleased = true;
	
	const float duration = _AlphaInput < CoolDownDuration ? CoolDownDuration : _AlphaInput;
	UPSFl::StartCooldown(GetWorld(),duration,_CoolDownTimerHandle);

	OnPushEvent.Broadcast(false);
}

void UPS_ForceComponent::PlaySound(const EPushSFXType soundType)
{
	if(!PushSounds.Contains(soundType) || !PushSounds.Contains(EPushSFXType::WHISPER)) return;

	USoundBase* currentSound = *PushSounds.Find(soundType);

	if(!IsValid(currentSound) || !IsValid(_PlayerCharacter) || !IsValid(GetWorld())) return;

	switch (soundType)
	{
		case EPushSFXType::RELEASED:
			UGameplayStatics::SpawnSoundAtLocation(GetWorld(), currentSound, _PlayerCharacter->GetActorLocation() + _PlayerCharacter->GetActorForwardVector() * 350);

			if (bIsQuickPush && PushSounds.Contains(EPushSFXType::WHISPER) && IsValid(*PushSounds.Find(EPushSFXType::WHISPER)))
				UGameplayStatics::SpawnSoundAttached(*PushSounds.Find(soundType), _PlayerCharacter->GetMesh(), SOCKET_HAND_LEFT);
		
			break;
			
		case EPushSFXType::WHISPER:
			if (!bIsQuickPush)
				UGameplayStatics::SpawnSoundAtLocation(GetWorld(), currentSound, _StartForcePushLoc);
		
			break;
			
		case EPushSFXType::IMPACT:
			UGameplayStatics::SpawnSoundAtLocation(GetWorld(), currentSound, _ImpactForcePushLoc);
			break;
	}
	
}

//------------------
#pragma endregion Push

#pragma region Screw

//------------------

void UPS_ForceComponent::OnScrewResetEndEventReceived()
{
	if (bDebugPush) UE_LOG(LogTemp, Log, TEXT("%S"),__FUNCTION__);
}

void UPS_ForceComponent::AttachScrew()
{
	USkeletalMeshComponent* playerSkel = _PlayerCharacter->GetMesh();
	if(!IsValid(playerSkel))
	{
		UE_LOG(LogTemp, Warning, TEXT("%S :: Skeletal Mesh not found !"),__FUNCTION__);
		return;
	}

	UStaticMesh* LoadedScrewMesh =  ScrewMesh.LoadSynchronous();
	if(!IsValid(LoadedScrewMesh))
	{
		UE_LOG(LogTemp, Warning, TEXT("%S :: ScrewMesh not found or invalid !"),__FUNCTION__);
		return;
	}

	// Parkour list of socket names
	for (FName SocketName : ScrewSocketNames)
	{
		// Check if socket exists
		if (_PlayerCharacter->GetMesh()->DoesSocketExist(SocketName))
		{
			// Create a new StaticMeshComponent
			UStaticMeshComponent* NewMeshComponent = NewObject<UStaticMeshComponent>(this);
			NewMeshComponent->SetStaticMesh(Cast<UStaticMesh>(LoadedScrewMesh));

			//Collision
			NewMeshComponent->SetCollisionProfileName(Profile_NoCollision);
			NewMeshComponent->SetGenerateOverlapEvents(false);

			//Opti
			NewMeshComponent->PrimaryComponentTick.bCanEverTick = false;
			NewMeshComponent->PrimaryComponentTick.bStartWithTickEnabled = false;
			NewMeshComponent->bApplyImpulseOnDamage = false;
			NewMeshComponent->bReplicatePhysicsToAutonomousProxy = false;
			//NewMeshComponent->SetCastShadow(false);
			NewMeshComponent->SetCanEverAffectNavigation(false);

			//Finish spawn
			NewMeshComponent->RegisterComponent();
			
			//Setup mesh transform
			NewMeshComponent->SetRelativeRotation(FRotator(90.0f,0.0f,0.0f));
			NewMeshComponent->SetRelativeScale3D(FVector(0.25f,0.25f,0.25f));
            
			// Attach the mesh to the socket			
			NewMeshComponent->AttachToComponent(playerSkel, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%S :: Socket %s does not exist!"),__FUNCTION__, *SocketName.ToString());
		}
	}

	
}

//------------------
#pragma endregion Screw

#pragma region Destruction
//------------------

#pragma region CanGenerateImpactField
//------------------

void UPS_ForceComponent::GenerateImpactField(const FHitResult& targetHit, const FVector extent)
{
	if (!IsValid(_PlayerCharacter) || !IsValid(_PlayerController) || !IsValid(GetWorld()) || !targetHit.bBlockingHit) return;

	if(!IsValid(FieldSystemActor.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("%S :: SpawnActor failed because no class was specified"),__FUNCTION__);
		return;
	}
	
	//Spawn param
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = _PlayerCharacter;
	SpawnInfo.Instigator = _PlayerCharacter;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	//Spawn Location
	FVector loc = targetHit.Location;
		
	//Spawn Rotation
	FRotator rot = UKismetMathLibrary::FindLookAtRotation(_StartForcePushLoc, _StartForcePushLoc + _DirForcePush * ConeLength);
	rot.Pitch = rot.Pitch - 90.0f;

	//Spawn scale
	FVector scale = (extent * 2) / 100;
		
	_ImpactField = GetWorld()->SpawnActor<APS_FieldSystemActor>(FieldSystemActor.Get(), loc, rot, SpawnInfo);
	_ImpactField->SetActorScale3D(scale);

	if(!IsValid(_ImpactField)) return;
	
	//Stock move field var
	_GenerateFieldTimestamp = GetWorld()->GetTimeSeconds();
	_FieldTransformOnGeneration = FTransform(rot, loc, scale);
	_bCanMoveField = true;
	
	//Debug
	if(bDebugChaos) UE_LOG(LogTemp, Log, TEXT("%S :: success %i"), __FUNCTION__, IsValid(_ImpactField));

	//Callback
	OnForceChaosFieldGeneratedEvent.Broadcast(_ImpactField);
}

void UPS_ForceComponent::UpdateImpactField()
{
	if(!IsValid(_ImpactField) || !IsValid(GetWorld())) return;

	if(!_bCanMoveField) return;

	//Init work var
	const float moveDist = ConeLength + FieldSystemMoveTargetLocFwdOffset;
	const FVector targetLoc = _StartForcePushLoc + _DirForcePush * moveDist;

	const float distFromStart = UKismetMathLibrary::Vector_DistanceSquared(_FieldTransformOnGeneration.GetLocation(), targetLoc);
	const float distToTarget = UKismetMathLibrary::Vector_DistanceSquared(_ImpactField->GetActorLocation(),targetLoc);
	
	const float moveDuration = UKismetMathLibrary::SafeDivide(FMath::Sqrt(distFromStart), moveDist);

	//Override life span
	const float newLifeSpan = moveDuration + FieldSystemDestroyingDelay;
	if(_ImpactField->GetLifeSpan() != newLifeSpan) _ImpactField->SetLifeSpan(moveDuration + FieldSystemDestroyingDelay);
	
	//Move
	const float alpha = UKismetMathLibrary::MapRangeClamped(GetWorld()->GetTimeSeconds(), _GenerateFieldTimestamp, _GenerateFieldTimestamp + moveDuration, 0.0f, 1.0f);
	_ImpactField->SetActorLocation(FMath::Lerp(_FieldTransformOnGeneration.GetLocation(),targetLoc,alpha));

	//Scale
	const float radius = FMath::Abs((FMath::Sqrt(distToTarget) - FMath::Sqrt(distFromStart)) * FMath::DegreesToRadians(ConeAngleDegrees));
	_ImpactField->SetActorScale3D(_FieldTransformOnGeneration.GetScale3D() + ((FVector::One() * radius * 2) / 100));

	if (bDebugChaos) UE_LOG(LogTemp, Log, TEXT("%S :: duration %f, lifespan %f,  distFromStart %f, distToTarget %f, radiusOffset %f, alpha %f"),__FUNCTION__, moveDuration, _ImpactField->GetLifeSpan(),  distFromStart, distToTarget, radius, alpha);

	//Stop moving
	if (alpha >= 1.0f)
	{
		_bCanMoveField = false;
	}
}

//------------------

#pragma endregion CanGenerateImpactField

//------------------
#pragma endregion Destruction
	
	
	

