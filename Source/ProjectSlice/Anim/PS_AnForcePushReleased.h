// OSome

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "PS_AnForcePushReleased.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTSLICE_API UPSAnForcePushReleased : public UAnimNotify
{
	GENERATED_BODY()
	
	UPSAnForcePushReleased();
	
private:
	virtual void Notify(USkeletalMeshComponent* meshComp, UAnimSequenceBase* animation,
		const FAnimNotifyEventReference& eventReference) override;
};
