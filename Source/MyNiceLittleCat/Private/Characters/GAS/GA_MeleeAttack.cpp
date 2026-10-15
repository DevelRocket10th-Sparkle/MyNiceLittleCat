// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/GAS/GA_MeleeAttack.h"
#include "GameFramework/Character.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "AbilitySystemComponent.h"

UGA_MeleeAttack::UGA_MeleeAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGA_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

    ACharacter* Character =
        Cast<ACharacter>(ActorInfo->AvatarActor.Get());

    if (!Character || !Character->GetMesh())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UAnimInstance* AnimInstance =
        Character->GetMesh()->GetAnimInstance();

    if (!AnimInstance || !AttackMontage)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const float Duration =
        AnimInstance->Montage_Play(AttackMontage);

    if (Duration <= 0.0f)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
