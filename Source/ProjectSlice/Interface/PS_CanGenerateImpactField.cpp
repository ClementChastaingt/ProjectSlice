// Fill out your copyright notice in the Description page of Project Settings.


#include "PS_CanGenerateImpactField.h"
#include "Components/ShapeComponent.h"
#include "ProjectSlice/Components/GPE/PS_FieldSystemActor.h"

void IPS_CanGenerateImpactField::ResetImpactField(const bool bForce)
{
	if (!IsValid(GetImpactField())) return;

	//Check if overlap already
	if (!bForce && IsValid(GetImpactField()->GetCollider()))
	{
		TArray<AActor*> overlappingActors;
		GetImpactField()->GetCollider()->GetOverlappingActors(overlappingActors);
		if(!overlappingActors.IsEmpty()) return;
	}

	GetImpactField()->Destroy();
}
