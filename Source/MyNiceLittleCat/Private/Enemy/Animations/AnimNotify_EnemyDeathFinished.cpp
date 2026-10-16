// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Animations/AnimNotify_EnemyDeathFinished.h"

#include "Components/SkeletalMeshComponent.h"
#include "Enemy/EnemyCharacter.h"

void UAnimNotify_EnemyDeathFinished::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp))
		return;

	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(MeshComp->GetOwner()))
	{
		Enemy->FinishDeath();
	}
}
