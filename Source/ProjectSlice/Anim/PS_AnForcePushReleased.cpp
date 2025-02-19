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
	if(!IsValid(animatedPlayer) || !IsValid(animatedPlayer->GetWorld())) return;

	UPS_ForceComponent* forceComp = Cast<UPS_ForceComponent>(animatedPlayer->GetForceComponent());
	if(!IsValid(forceComp)) return;
	
	forceComp->OnPushReleaseNotifyEvent.Broadcast();
	
}
