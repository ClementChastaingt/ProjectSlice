#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"

struct FRealTimeTimer
{
    TSharedPtr<FTimerDelegate> Delegate;
    double EndTime = 0.0;
    double StartTime = 0.0;
    float Dilation = 1.0f;
    bool bLoop = false;
    bool bActive = true;
    float Rate = 0.0f;
};

class UPS_RealTimeTimerManager
{
public:
    static UPS_RealTimeTimerManager& Get();

    FGuid SetTimer(FTimerHandle& OutHandle, const FTimerDelegate& InDelegate, float InRate, bool bInLoop, float InFirstDelay = -1.0f, float CustomDilation = 1.0f);
    void ClearTimer(FTimerHandle& InHandle);
    bool IsTimerActive(const FTimerHandle& InHandle) const;

    TMap<FGuid, TSharedPtr<FRealTimeTimer>> Timers;
    TMap<FTimerHandle, FGuid> HandleToGuidMap;

    void TickTimers(UWorld* World);

private:
    UPS_RealTimeTimerManager() = default;

    void StartTicking(UWorld* World);


    FTimerHandle TickHandle;
    bool bIsTicking = false;
};
