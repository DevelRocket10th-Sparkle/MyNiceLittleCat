// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/GAS/GA_MeleeAttack.h"
#include "GameFramework/Character.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

#include "Characters/PlayerCharacter.h"


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

    const APlayerCharacter* Player = Cast<APlayerCharacter>(Character);
    bIsSprintAttack = SprintAttackMontage && Player && Player->GetbIsRunning();
    CurrentAttackMontage = bIsSprintAttack ? SprintAttackMontage : AttackMontage;

    UAbilityTask_PlayMontageAndWait* MontageTask =
        UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this,
            NAME_None,
            CurrentAttackMontage
        );

    if (!MontageTask)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    MontageTask->OnCompleted.AddDynamic(
        this, &UGA_MeleeAttack::OnMontageCompleted);

    MontageTask->OnInterrupted.AddDynamic(
        this, &UGA_MeleeAttack::OnMontageInterrupted);

    MontageTask->OnCancelled.AddDynamic(
        this, &UGA_MeleeAttack::OnMontageInterrupted);

    MontageTask->OnBlendOut.AddDynamic(
        this, &UGA_MeleeAttack::OnMontageCompleted);

    // 콤보 상태 초기화
    ComboIndex = 0;
    bCanQueueCombo = false;
    bComboInputQueued = false;

    // 콤보 시작 가능 구간
    UAbilityTask_WaitGameplayEvent* ComboOpenTask =
        UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
            this,
            FGameplayTag::RequestGameplayTag(
                FName("Event.Attack.ComboOpen")),
            nullptr,
            false,
            true);

    ComboOpenTask->EventReceived.AddDynamic(
        this, &UGA_MeleeAttack::HandleComboOpen);

    ComboOpenTask->ReadyForActivation();

    // 콤보 입력 구간 종료
    UAbilityTask_WaitGameplayEvent* ComboCloseTask =
        UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
            this,
            FGameplayTag::RequestGameplayTag(
                FName("Event.Attack.ComboClose")),
            nullptr,
            false,
            true);

    ComboCloseTask->EventReceived.AddDynamic(
        this, &UGA_MeleeAttack::HandleComboClose);

    ComboCloseTask->ReadyForActivation();

    // 다음 공격 입력
    UAbilityTask_WaitGameplayEvent* ComboInputTask =
        UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
            this,
            FGameplayTag::RequestGameplayTag(
                FName("Event.Attack.Input")),
            nullptr,
            false,
            true);

    ComboInputTask->EventReceived.AddDynamic(
        this, &UGA_MeleeAttack::HandleComboInput);

    ComboInputTask->ReadyForActivation();
    StartAttackForwardMove(500.0f, 0.5f);
    MontageTask->ReadyForActivation();

}

void UGA_MeleeAttack::HandleComboOpen(FGameplayEventData Payload)
{
    bCanQueueCombo = true;
}

void UGA_MeleeAttack::HandleComboClose(FGameplayEventData Payload)
{
    bCanQueueCombo = false;

    if (bComboInputQueued && ComboIndex < 2)
    {
        AdvanceCombo();
        return;
    }

    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

    UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();

    if (AnimInstance && CurrentAttackMontage)
    {
        AnimInstance->Montage_Stop(0.3f, CurrentAttackMontage);
    }
}

void UGA_MeleeAttack::HandleComboInput(FGameplayEventData Payload)
{
    if (bIsSprintAttack) return;

    if (bCanQueueCombo && ComboIndex < 2)
    {
        bComboInputQueued = true;
    }
}

void UGA_MeleeAttack::AdvanceCombo()
{
    bComboInputQueued = false;
    ++ComboIndex;

    static const FName Sections[] =
    {
        TEXT("Attack1"),
        TEXT("Attack2"),
        TEXT("Attack3")
    };

    bCanQueueCombo = false;
    bComboInputQueued = false;
    ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());

    if (Character)
    {
        StartAttackForwardMove(500.0f, 0.5f);
    }

    if (UAnimInstance* AnimInstance =
        GetCurrentActorInfo()->GetAnimInstance())
    {
        AnimInstance->Montage_JumpToSection(
            Sections[ComboIndex],
            AttackMontage
        );
    }
}

void UGA_MeleeAttack::StartAttackForwardMove(float Distance, float Duration)
{
    //ACharacter* Character =
    //    Cast<ACharacter>(GetAvatarActorFromActorInfo());

    //if (!Character || Duration <= 0.0f)
    //{
    //    return;
    //}

    //UCharacterMovementComponent* MoveComp =
    //    Character->GetCharacterMovement();

    //if (!MoveComp)
    //{
    //    return;
    //}

    //AttackMoveDirection = Character->GetActorForwardVector();
    //AttackMoveDirection.Z = 0.0f;
    //AttackMoveDirection.Normalize();

    //AttackMoveRemaining = Distance;
    //AttackMoveSpeed = Distance / Duration;
    //bIsAttackMoving = true;

    //FVector Velocity = MoveComp->Velocity;
    //Velocity.X = AttackMoveDirection.X * AttackMoveSpeed;
    //Velocity.Y = AttackMoveDirection.Y * AttackMoveSpeed;

    //MoveComp->Velocity = Velocity;

    //UE_LOG(LogTemp, Warning,
    //    TEXT("[MeleeAttack] 전진 시작 | Speed=%.1f | Duration=%.2f"),
    //    AttackMoveSpeed, Duration);
}

void UGA_MeleeAttack::OnMontageCompleted()
{
    EndAbility(
        CurrentSpecHandle,
        CurrentActorInfo,
        CurrentActivationInfo,
        true,
        false
    );
}

void UGA_MeleeAttack::OnMontageInterrupted()
{
    UE_LOG(LogTemp, Warning, TEXT("[MeleeAttack] 몽타주 중단"));
    EndAbility(
        CurrentSpecHandle,
        CurrentActorInfo,
        CurrentActivationInfo,
        true,
        true
    );
}
