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
class PROJECTSLICE_API UPS_TimerSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
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
	
	bool bIsTicking = false;

#pragma region IntrerfaceTickableGameObject
	//------------------

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bIsTicking; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UPS_TimerSubsystem, STATGROUP_Tickables); }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

	//------------------
#pragma endregion IntrerfaceTickableGameObject
};