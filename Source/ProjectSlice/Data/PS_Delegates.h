// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PS_Delegates.generated.h"

/**
 * 
 */


/**
 * File where to put delegates
 */

class ATZNpcBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPSDelegate);

// 1 param
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Vector, const FVector, vectorValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Vector2D,const FVector2D, vectorValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Actor, const AActor*, actor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Int, const int&, intValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Float, float, newFloatValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Bool, const bool, boolValue);

// DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_TZWidget, UTZWidget*, widget);
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Ability, const EAbilityType, abilityType);
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Jump, const EJumpType, jumpType);
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_RollType, const ERollEndType, rollEndType);
//
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_HitResult, const FHitResult, hitResult);
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_Enemy, ATZEnemyBase*, enemy);
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_PlayerTrackingStage, const EPlayerTrackingStage, newStage);
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_SquadManager, ATZSquadManager*, squadManager);
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_DetectedActor, const FSDetectedActor&, detectedActor);
//
// // 2 params
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPSDelegate_Int_Actor, const int32&, intValue, const AActor*, actor);
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPSDelegate_Float_Actor, const float&, floatValue, const AActor*, actor);
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPSDelegate_KFD_Bool, const EKnotFinalDestination, kfd, const bool, boolValue);
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPSDelegate_Npc_Actor, ATZNpcBase*, npc, const AActor*, actor);
//
// // 4 params
// //DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnPSDelegate_VaultStart, float, currentSpeed, const float, outSpeed, const EVaultType, vaultType,  float, Duration);
//
// // States
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPSDelegate_EnemyState_Enemy, const EEnemyMainState, newState, ATZEnemyBase*, enemy);
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBaseStateDelegate, const uint8, oldState, const uint8, newState);
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_AlertState, const EAlertState, newState);
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_FearState, const EFearState, newState);
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPSDelegate_AttackDefensiveState, const EAttackDefensiveState, oldState, const EAttackDefensiveState, newState);
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPSDelegate_EnemyPlayerAwarenessState, const EEnemyPlayerAwarenessState, newState);
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPSDelegate_EnemyAttackerState, const EEnemyAttackerState, newState, ATZEnemyBase*, enemy);


UCLASS()
class PROJECTSLICE_API UPS_Delegates : public UObject
{
	GENERATED_BODY()
};
