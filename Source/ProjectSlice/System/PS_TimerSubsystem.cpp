#include "PS_TimerSubsystem.h"
#include "Engine/World.h"

void UPS_TimerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    if (bDebug) UE_LOG(LogTemp, Error, TEXT("========== PS_TimerSubsystem INITIALIZED =========="));
    
    bIsTicking = true;
    
    FTimerDelegate TickDelegate;
    TickDelegate.BindUObject(this, &UPS_TimerSubsystem::TickTimers);
    
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(TickHandle, TickDelegate, 0.01f, true);
       if (bDebug) UE_LOG(LogTemp, Error, TEXT("PS_TimerSubsystem: Tick timer started"));
    }
}

void UPS_TimerSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TickHandle);
    }
    
    Timers.Empty();
    HandleToGuidMap.Empty();
    bIsTicking = false;
    
    if (bDebug) UE_LOG(LogTemp, Warning, TEXT("PS_TimerSubsystem: Deinitialized"));
    
    Super::Deinitialize();
}

FTimerHandle UPS_TimerSubsystem::SetTimerWithHandle(const FTimerDelegate& InDelegate, float InRate, bool bInLoop, float InFirstDelay, float CustomDilation)
{
    UWorld* World = GetWorld();
    if (!IsValid(World)) 
    {
        if (bDebug) UE_LOG(LogTemp, Error, TEXT("SetTimerWithHandle: World invalid"));
        return FTimerHandle();
    }
    
    if (!InDelegate.IsBound()) 
    {
        if (bDebug) UE_LOG(LogTemp, Error, TEXT("SetTimerWithHandle: Delegate not bound"));
        return FTimerHandle();
    }

    // Créer un handle valide
    FTimerHandle NewHandle;
    FTimerDelegate DummyDelegate;
    DummyDelegate.BindLambda([]() {});
    World->GetTimerManager().SetTimer(NewHandle, DummyDelegate, 999999.f, false);
    
    if (!NewHandle.IsValid())
    {
        if (bDebug) UE_LOG(LogTemp, Error, TEXT("SetTimerWithHandle: Failed to create valid handle"));
        return FTimerHandle();
    }
    
    // Créer le timer
    FGuid NewGuid = FGuid::NewGuid();
    FRealTimeTimer NewTimer;
    NewTimer.Delegate = MakeShared<FTimerDelegate>(InDelegate);
    NewTimer.StartTime = World->GetRealTimeSeconds();
    NewTimer.EndTime = NewTimer.StartTime + ((InFirstDelay > 0.f ? InFirstDelay : InRate) / CustomDilation);
    NewTimer.Dilation = CustomDilation;
    NewTimer.Rate = InRate;
    NewTimer.bLoop = bInLoop;
    NewTimer.bActive = true;

    Timers.Add(NewGuid, NewTimer);
    HandleToGuidMap.Add(NewHandle, NewGuid);
    
    if (bDebug) UE_LOG(LogTemp, Error, TEXT("SetTimerWithHandle: Created timer GUID=%s, EndTime=%f, TimeRemaining=%.2f"), 
        *NewGuid.ToString(), NewTimer.EndTime, NewTimer.EndTime - World->GetRealTimeSeconds());
    
    return NewHandle;
}

void UPS_TimerSubsystem::ClearTimer(FTimerHandle& InHandle)
{
    if (!InHandle.IsValid()) return;

    UWorld* World = GetWorld();
    
    if (const FGuid* FoundGuid = HandleToGuidMap.Find(InHandle))
    {
        if (FRealTimeTimer* FoundTimer = Timers.Find(*FoundGuid))
        {
            FoundTimer->bActive = false;
        }
        Timers.Remove(*FoundGuid);
        HandleToGuidMap.Remove(InHandle);
        
        if (bDebug) UE_LOG(LogTemp, Warning, TEXT("ClearTimer: Cleared timer"));
    }

    if (IsValid(World))
    {
        World->GetTimerManager().ClearTimer(InHandle);
    }

    InHandle.Invalidate();
}

bool UPS_TimerSubsystem::IsTimerActive(const FTimerHandle& InHandle) const
{
    if (!InHandle.IsValid()) return false;
    
    if (const FGuid* FoundGuid = HandleToGuidMap.Find(InHandle))
    {
        if (const FRealTimeTimer* FoundTimer = Timers.Find(*FoundGuid))
            return FoundTimer->bActive;
    }
    return false;
}

void UPS_TimerSubsystem::TickTimers()
{
    UWorld* World = GetWorld();
    if (!IsValid(World)) return;

    const double CurrentRealTime = World->GetRealTimeSeconds();

    // Log debug périodique
    static double LastDebugLog = 0;
    if (CurrentRealTime - LastDebugLog > 0.5)
    {
        if (bDebug) UE_LOG(LogTemp, Log, TEXT("TickTimers: %d active timers at time %.2f"), Timers.Num(), CurrentRealTime);
        LastDebugLog = CurrentRealTime;
    }

    TArray<FGuid> ToRemove;
    TMap<FGuid, FRealTimeTimer> ToParkour = Timers;
    for (auto& Pair : ToParkour)
    {
        FRealTimeTimer& Timer = Pair.Value;
        
        if (!Timer.bActive) continue;

        const float TimeRemaining = Timer.EndTime - CurrentRealTime;
        
        if (CurrentRealTime >= Timer.EndTime)
        {
            if (bDebug) UE_LOG(LogTemp, Error, TEXT("========== TIMER FIRED! GUID=%s =========="), *Pair.Key.ToString());
            
            if (Timer.Delegate.IsValid() && Timer.Delegate->IsBound())
            {
                if (bDebug) UE_LOG(LogTemp, Error, TEXT("Executing delegate..."));
                Timer.Delegate->ExecuteIfBound();
                if (bDebug) UE_LOG(LogTemp, Error, TEXT("Delegate executed successfully!"));
            }
            else
            {
                if (bDebug) UE_LOG(LogTemp, Error, TEXT("ERROR: Delegate is invalid or not bound!"));
            }

            if (Timer.bLoop)
            {
                Timer.StartTime = CurrentRealTime;
                Timer.EndTime = CurrentRealTime + (Timer.Rate / Timer.Dilation);
            }
            else
            {
                Timer.bActive = false;
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