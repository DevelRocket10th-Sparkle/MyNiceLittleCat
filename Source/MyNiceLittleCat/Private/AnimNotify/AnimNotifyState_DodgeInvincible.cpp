// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify/AnimNotifyState_DodgeInvincible.h"
#include "Characters/PlayerCharacter.h"

void UAnimNotifyState_DodgeInvincible::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (APlayerCharacter* Player = MeshComp ? Cast<APlayerCharacter>(MeshComp->GetOwner()) : nullptr)
	{
		Player->SetDodgeInvincible(true);
	}
}

void UAnimNotifyState_DodgeInvincible::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (APlayerCharacter* Player = MeshComp ? Cast<APlayerCharacter>(MeshComp->GetOwner()) : nullptr)
	{
		Player->SetDodgeInvincible(false);
	}
}
