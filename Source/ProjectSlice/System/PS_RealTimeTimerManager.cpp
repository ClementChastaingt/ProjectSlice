// Fill out your copyright notice in the Description page of Project Settings.
#include "PS_RealTimeTimerManager.h"

UPS_RealTimeTimerManager::UPS_RealTimeTimerManager()
{
	// On hook le ticker global (tick indépendant du world/game time)
	 TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &UPS_RealTimeTimerManager::HandleTicker)
	);
}

UPS_RealTimeTimerManager::~UPS_RealTimeTimerManager()
{
	// Nettoyer le ticker
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
}


UPS_RealTimeTimerManager& UPS_RealTimeTimerManager::Get()
{
	static UPS_RealTimeTimerManager Instance;
	return Instance;
}


bool UPS_RealTimeTimerManager::HandleTicker(float DeltaTime)
{
	TickTimers(DeltaTime);
	return true; // keep ticking
}

/** Ajoute un timer, retourne un ID unique */
FGuid UPS_RealTimeTimerManager::AddTimer(UWorld* World, const FTimerDelegate& InDelegate, float InRate, bool bLoop,
	float InFirstDelay,
	float CustomDilation)
{
	if (!World) 
	{
		UE_LOG(LogTemp, Error, TEXT("AddTimer: World is null"));
		return FGuid();
	}

	if (CustomDilation <= 0.f)
	{
		CustomDilation = 1.f;
	}

	const double Now = FPlatformTime::Seconds();
	const float EffectiveDelay = (InFirstDelay >= 0.f) ? InFirstDelay : InRate;
	const double TargetTime = Now + (EffectiveDelay / FMath::Max(CustomDilation, KINDA_SMALL_NUMBER));

	// ID unique
	FGuid ID = FGuid::NewGuid();

	_Timers.Add(ID, MakeShared<FRealTimeTimer>(InDelegate, TargetTime, InRate, bLoop, CustomDilation, World));

	UE_LOG(LogTemp, Log, TEXT("%S :: Timer added: %s, Rate: %f, Loop: %d, Delay: %f"), 
	  __FUNCTION__, *ID.ToString(), InRate, bLoop, EffectiveDelay);

	return ID;
}

void UPS_RealTimeTimerManager::ClearTimer(const FGuid& ID)
{
	if (TSharedPtr<FRealTimeTimer>* Found = _Timers.Find(ID))
	{
		(*Found)->bActive = false;
		_Timers.Remove(ID);
	}
}

bool UPS_RealTimeTimerManager::IsTimerActive(const FGuid& ID) const
{
	if (const TSharedPtr<FRealTimeTimer>* Found = _Timers.Find(ID))
	{
		return (*Found).IsValid() && (*Found)->bActive;
	}
	return false;
}

void UPS_RealTimeTimerManager::TickTimers(float /*UnusedDelta*/)
{
	const double Now = FPlatformTime::Seconds();
	TArray<FGuid> ToRemove;

	for (auto& Elem : _Timers)
	{
		FGuid ID = Elem.Key;
		TSharedPtr<FRealTimeTimer> Timer = Elem.Value;

		if (!Timer.IsValid() || !Timer->bActive)
		{
			ToRemove.Add(ID);
			continue;
		}

		UWorld* World = Timer->WeakWorld.Get();
		if (!World)
		{
			ToRemove.Add(ID);
			continue;
		}

		if (Now >= Timer->TargetTime)
		{
			Timer->Delegate.ExecuteIfBound();

			if (Timer->bLoop)
			{
				if (Timer->Rate > 0.f)
				{
					Timer->TargetTime = Now + (Timer->Rate / FMath::Max(Timer->CustomDilation, KINDA_SMALL_NUMBER));
				}
				else
				{
					Timer->TargetTime = Now; // every-frame loop
				}
			}
			else
			{
				ToRemove.Add(ID);
			}
		}
	}

	for (const FGuid& ID : ToRemove)
	{
		_Timers.Remove(ID);
	}
}

void UPS_RealTimeTimerManager::SetDilatedRealTimeTimer(
	UWorld* World,
	FTimerHandle& InOutHandle,
	const FTimerDelegate& InDelegate,
	float InRate,
	bool bLoop,
	float InFirstDelay,
	float CustomDilation
)
{
	if (!IsValid(World)) return;

	// Supprimer un éventuel ancien mapping
	if (HandleToGuidMap.Contains(InOutHandle))
	{
		ClearDilatedRealTimeTimer(InOutHandle);
	}

	// Ajout dans le manager interne (créé et retourne un FGuid)
	FGuid NewGuid = AddTimer(World, InDelegate, InRate, bLoop, InFirstDelay, CustomDilation);

	// Associer ce Guid au FTimerHandle utilisateur
	HandleToGuidMap.Add(InOutHandle, NewGuid);
}

void UPS_RealTimeTimerManager::ClearDilatedRealTimeTimer(FTimerHandle& InOutHandle)
{
	if (FGuid* FoundGuid = HandleToGuidMap.Find(InOutHandle))
	{
		ClearTimer(*FoundGuid);
		HandleToGuidMap.Remove(InOutHandle);
	}
}

bool UPS_RealTimeTimerManager::IsDilatedRealTimeTimerActive(const FTimerHandle& InOutHandle) const
{
	if (const FGuid* FoundGuid = HandleToGuidMap.Find(InOutHandle))
	{
		return IsTimerActive(*FoundGuid);
	}
	return false;
}



