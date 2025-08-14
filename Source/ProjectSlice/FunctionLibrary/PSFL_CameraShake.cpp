// Fill out your copyright notice in the Description page of Project Settings.


#include "PSFL_CameraShake.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectSlice/Character/PC/PS_Character.h"
#include "ProjectSlice/Components/PC/PS_PlayerCameraComponent.h"

#pragma region CameraShake
//------------------

class AProjectSliceCharacter;
class AProjectSlicePlayerController;

void UPSFL_CameraShake::ShakeCamera(const UObject* const context, const EScreenShakeType& shakeType, const float& scale, const FVector& epicenter)
{
	AProjectSliceCharacter* player = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(context, 0));
	
	if(!IsValid(player)) return;

	player->GetFirstPersonCameraComponent()->ShakeCamera(shakeType, scale, epicenter);
}

void UPSFL_CameraShake::StopCameraShake(const UObject* const context, const EScreenShakeType& shakeType,const bool& bImmediately)
{
	AProjectSliceCharacter* player = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(context, 0));
	
	if(!IsValid(player)) return;

	player->GetFirstPersonCameraComponent()->StopCameraShake(shakeType, bImmediately);
}

void UPSFL_CameraShake::UpdateCameraShake(const UObject* const context, const EScreenShakeType& shakeType, const float& scale)
{
	AProjectSliceCharacter* player = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(context, 0));
	
	if(!IsValid(player)) return;
	
	player->GetFirstPersonCameraComponent()->UpdateCameraShakeScale(shakeType, scale);
}

void UPSFL_CameraShake::WorldShakeCamera(const UObject* context, const EScreenShakeType& shakeType,
	const FVector& epicenter, const FSWorldShakeParams& worldShakeParams, const float& scale)
{
	AProjectSliceCharacter* player = Cast<AProjectSliceCharacter>(UGameplayStatics::GetPlayerCharacter(context, 0));
	
	if(!IsValid(player)) return;

	player->GetFirstPersonCameraComponent()->WorldShakeCamera(shakeType, epicenter, worldShakeParams, scale);
}


//------------------
#pragma endregion CameraShake