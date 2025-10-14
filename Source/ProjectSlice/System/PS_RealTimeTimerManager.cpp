#include "PS_RealTimeTimerManager.h"

struct FRealTimeTimer;

UPS_RealTimeTimerManager& UPS_RealTimeTimerManager::Get()
{
	static UPS_RealTimeTimerManager Instance;
	return Instance;
}

void UPS_RealTimeTimerManager::StartTicking(UWorld* World)
{
	if (!bIsTicking && IsValid(World))
	{
		bIsTicking = true;
		FTimerDelegate TickDelegate;
		TickDelegate.BindLambda([this, World]()
		{
			TickTimers(World);
		});

		World->GetTimerManager().SetTimer(TickHandle, TickDelegate, 0.01f, true);
	}
}

FGuid UPS_RealTimeTimerManager::SetTimer(FTimerHandle& OutHandle, const FTimerDelegate& InDelegate, float InRate, bool bInLoop, float InFirstDelay, float CustomDilation)
{
	UWorld* World = GWorld;
	if (!IsValid(World)) 
	{
		UE_LOG(LogTemp, Error, TEXT("SetTimer: GWorld is invalid"));
		return FGuid();
	}

	if (!InDelegate.IsBound())
	{
		UE_LOG(LogTemp, Error, TEXT("SetTimer: Delegate is not bound"));
		return FGuid();
	}

	StartTicking(World);

	// Si le handle existe déjà, nettoyer l'ancien mapping
	if (OutHandle.IsValid())
	{
		if (FGuid* ExistingGuid = HandleToGuidMap.Find(OutHandle))
		{
			Timers.Remove(*ExistingGuid);
			HandleToGuidMap.Remove(OutHandle);
			UE_LOG(LogTemp, Log, TEXT("SetTimer: Cleaned existing timer for handle"));
		}
	}
	
	FGuid NewGuid = FGuid::NewGuid();
	TSharedPtr<FRealTimeTimer> NewTimer = MakeShared<FRealTimeTimer>();
	NewTimer->Delegate = MakeShared<FTimerDelegate>(InDelegate);
	NewTimer->StartTime = World->GetRealTimeSeconds();
	NewTimer->EndTime = NewTimer->StartTime + ((InFirstDelay > 0.f ? InFirstDelay : InRate) / CustomDilation);
	NewTimer->Dilation = CustomDilation;
	NewTimer->Rate = InRate;
	NewTimer->bLoop = bInLoop;
	NewTimer->bActive = true;

	Timers.Add(NewGuid, NewTimer);
	HandleToGuidMap.Add(OutHandle, NewGuid);
	
	UE_LOG(LogTemp, Log, TEXT("SetTimer: Created timer with GUID %s, EndTime: %f"), *NewGuid.ToString(), NewTimer->EndTime);

	return NewGuid;
}

void UPS_RealTimeTimerManager::TickTimers(UWorld* World)
{
	if (!IsValid(World)) return;

	const double CurrentRealTime = World->GetRealTimeSeconds();

	TArray<FGuid> ToRemove;
	TMap<FGuid, TSharedPtr<FRealTimeTimer>> timersToCheck = Timers;
	
	for (auto& Pair : timersToCheck)
	{
		TSharedPtr<FRealTimeTimer> Timer = Pair.Value;
		if (!Timer.IsValid() || !Timer->bActive) continue;

		if (CurrentRealTime >= Timer->EndTime)
		{
			if (Timer->Delegate.IsValid())
				Timer->Delegate->ExecuteIfBound();

			if (Timer->bLoop)
			{
				Timer->StartTime = CurrentRealTime;
				Timer->EndTime = CurrentRealTime + (Timer->Rate / Timer->Dilation);
			}
			else
			{
				Timer->bActive = false;
				ToRemove.Add(Pair.Key);
			}
		}
	}

	for (const FGuid& Guid : ToRemove)
	{
		Timers.Remove(Guid);
		for (auto It = HandleToGuidMap.CreateIterator(); It; ++It)
		{
			if (It.Value() == Guid)
			{
				It.RemoveCurrent();
				break;
			}
		}
	}
}

void UPS_RealTimeTimerManager::ClearTimer(FTimerHandle& InHandle)
{
	if (!InHandle.IsValid()) 
	{
		UE_LOG(LogTemp, Warning, TEXT("ClearTimer: Handle is already invalid"));
		return;
	}
	
	if (const FGuid* FoundGuid = HandleToGuidMap.Find(InHandle))
	{
		if (TSharedPtr<FRealTimeTimer>* FoundTimer = Timers.Find(*FoundGuid))
		{
			if (FoundTimer && FoundTimer->IsValid())
				(*FoundTimer)->bActive = false;
			Timers.Remove(*FoundGuid);
		}
		HandleToGuidMap.Remove(InHandle);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ClearTimer: No GUID found for handle"));
	}

	InHandle.Invalidate();
}

bool UPS_RealTimeTimerManager::IsTimerActive(const FTimerHandle& InHandle) const
{
	if (!InHandle.IsValid()) return false;

	
	if (const FGuid* FoundGuid = HandleToGuidMap.Find(InHandle))
	{
		if (const TSharedPtr<FRealTimeTimer>* FoundTimer = Timers.Find(*FoundGuid))
			return FoundTimer && (*FoundTimer)->bActive;
	}
	return false;
}
