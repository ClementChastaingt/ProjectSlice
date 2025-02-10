// OSome

#include "PS_AnForcePushReleased.h"

#include "ProjectSlice/Character/PC/PS_Character.h"

UPSAnForcePushReleased::UPSAnForcePushReleased()
{
}

void UPSAnForcePushReleased::Notify(USkeletalMeshComponent* meshComp, UAnimSequenceBase* animation,
	const FAnimNotifyEventReference& eventReference)
{
	Super::Notify(meshComp, animation, eventReference);

	const AProjectSliceCharacter* animatedPlayer = Cast<AProjectSliceCharacter>(meshComp->GetOwner());
	if(!IsValid(animatedPlayer)) return;
	
	Cast<UPS_ForceComponent>(animatedPlayer->GetForceComponent())->OnPushReleasedEvent.Broadcast();
	
}
