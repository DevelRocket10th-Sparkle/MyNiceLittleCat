// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify/AnimNotifyState_MeleeHitWindow.h"
#include "Characters/PlayerCharacter.h"

void UAnimNotifyState_MeleeHitWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyBegin(
        MeshComp, Animation, TotalDuration, EventReference);

    if (!MeshComp) return;

    if (APlayerCharacter* Character =
        Cast<APlayerCharacter>(MeshComp->GetOwner()))
    {
        //Character->BeginMeleeHitWindow();
    }
}

void UAnimNotifyState_MeleeHitWindow::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyTick(
        MeshComp, Animation, FrameDeltaTime, EventReference);

    if (!MeshComp) return;

    if (APlayerCharacter* Character =
        Cast<APlayerCharacter>(MeshComp->GetOwner()))
    {
        //Character->TickMeleeHitWindow();
    }
}


void UAnimNotifyState_MeleeHitWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyEnd(
        MeshComp, Animation, EventReference);

    if (!MeshComp) return;

    if (APlayerCharacter* Character =
        Cast<APlayerCharacter>(MeshComp->GetOwner()))
    {
        //Character->EndMeleeHitWindow();
    }
}

FString UAnimNotifyState_MeleeHitWindow::GetNotifyName_Implementation() const
{
    return TEXT("Melee Hit Window");
}
