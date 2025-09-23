// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
struct FRealTimeTimer
{
    FTimerDelegate Delegate;
    double TargetTime = 0.0;
    float Rate = 0.f;
    bool bLoop = false;
    float CustomDilation = 1.f;
    bool bActive = true;
    TWeakObjectPtr<UWorld> WeakWorld;

    FRealTimeTimer(const FTimerDelegate& InDelegate, double InTargetTime, float InRate, bool InLoop, float InCustomDilation, UWorld* InWorld)
        : Delegate(InDelegate)
        , TargetTime(InTargetTime)
        , Rate(InRate)
        , bLoop(InLoop)
        , CustomDilation(InCustomDilation)
        , WeakWorld(InWorld)
    {}
};

class PROJECTSLICE_API UPS_RealTimeTimerManager
{
 
public:
    UPS_RealTimeTimerManager();
    ~UPS_RealTimeTimerManager();

public:
    static UPS_RealTimeTimerManager& Get();

    FGuid AddTimer(UWorld* World, const FTimerDelegate& InDelegate, float InRate, bool bLoop, float InFirstDelay,
        float CustomDilation);
    
    void ClearTimer(const FGuid& ID);
    
    bool IsTimerActive(const FGuid& ID) const;
    
    void TickTimers(float);
    
    void SetDilatedRealTimeTimer(UWorld* World, FTimerHandle& InOutHandle, const FTimerDelegate& InDelegate,
        float InRate,
        bool bLoop, float InFirstDelay, float CustomDilation);
    
    void ClearDilatedRealTimeTimer(FTimerHandle& InOutHandle);
    
    bool IsDilatedRealTimeTimerActive(const FTimerHandle& InOutHandle) const;

    TMap<FTimerHandle, FGuid> HandleToGuidMap;

private:    
    bool HandleTicker(float DeltaTime);
    
    TMap<FGuid, TSharedPtr<FRealTimeTimer>> _Timers;
    
    FThreadSafeCounter64 _NextID;

    // Pour le ticker
    FTSTicker::FDelegateHandle TickerHandle;

};






