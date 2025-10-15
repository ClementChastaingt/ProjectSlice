#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PS_TimerSubsystem.generated.h"

USTRUCT()
struct FRealTimeTimer
{
	GENERATED_BODY()

	TSharedPtr<FTimerDelegate> Delegate;
	double EndTime = 0.0;
	double StartTime = 0.0;
	float Dilation = 1.0f;
	bool bLoop = false;
	bool bActive = true;
	float Rate = 0.0f;
};

UCLASS()
class PROJECTSLICE_API UPS_TimerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	FTimerHandle SetTimerWithHandle(const FTimerDelegate& InDelegate, float InRate, bool bInLoop, float InFirstDelay = -1.0f, float CustomDilation = 1.0f);
	void ClearTimer(FTimerHandle& InHandle);

	bool IsTimerActive(const FTimerHandle& InHandle) const;

	UPROPERTY()
	TMap<FGuid, FRealTimeTimer> Timers;
    
	TMap<FTimerHandle, FGuid> HandleToGuidMap;

protected:
	bool bDebug = false;

private:
	void TickTimers();

	FTimerHandle TickHandle;
	bool bIsTicking = false;
};