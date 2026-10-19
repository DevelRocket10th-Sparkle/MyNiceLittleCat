// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify/AnimNotify_SendGamePlayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Actor.h"

void UAnimNotify_SendGamePlayEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp || !MeshComp->GetOwner() || !EventTag.IsValid()) return;

    AActor* Owner = MeshComp->GetOwner();

    UAbilitySystemComponent* ASC =
        UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);

    if (!ASC) return;

    FGameplayEventData Payload;
    Payload.Instigator = Owner;
    Payload.Target = Owner;

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
        Owner,
        EventTag,
        Payload
    );
}
