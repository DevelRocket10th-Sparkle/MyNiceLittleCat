// Fill out your copyright notice in the Description page of Project Settings.

#include "Systems/HitStopSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

void UHitStopSubsystem::ApplyHitStop(AActor* Actor, float Duration)
{
	UWorld* World = GetWorld();
	if (!IsValid(Actor) || !World || Duration <= 0.f) return;

	FTimerManager& TimerManager = World->GetTimerManager();
	const FTimerDelegate EndDelegate = FTimerDelegate::CreateUObject(
		this, &UHitStopSubsystem::EndHitStop, TWeakObjectPtr<AActor>(Actor));

	// 이미 히트스톱 중이면 다시 멈추지 않고 시간만 연장
	if (FHitStopState* Existing = ActiveHitStops.Find(Actor))
	{
		if (TimerManager.GetTimerRemaining(Existing->TimerHandle) < Duration)
		{
			TimerManager.SetTimer(Existing->TimerHandle, EndDelegate, Duration, false);
		}
		return;
	}

	ACharacter* Character = Cast<ACharacter>(Actor);
	UAnimInstance* AnimInstance =
		(Character && Character->GetMesh()) ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance) return;

	UAnimMontage* Montage = AnimInstance->GetCurrentActiveMontage();
	if (!Montage) return;   // 재생 중인 몽타주가 없으면 멈출 것이 없음

	// 몽타주만 정지 → AnimBP(꼬리 물리 등)는 정상 시간으로 계속 동작
	AnimInstance->Montage_Pause(Montage);

	// 정지 중엔 루트모션이 끊겨 일반 이동으로 처리되므로 남은 속도 제거
	if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}

	FHitStopState& State = ActiveHitStops.Add(Actor);
	State.AnimInstance = AnimInstance;
	State.PausedMontage = Montage;

	TimerManager.SetTimer(State.TimerHandle, EndDelegate, Duration, false);
}

void UHitStopSubsystem::EndHitStop(TWeakObjectPtr<AActor> WeakActor)
{
	if (const FHitStopState* State = ActiveHitStops.Find(WeakActor))
	{
		ResumeMontage(*State);
	}

	ActiveHitStops.Remove(WeakActor);
}

void UHitStopSubsystem::ResumeMontage(const FHitStopState& State)
{
	UAnimInstance* AnimInstance = State.AnimInstance.Get();
	UAnimMontage* Montage = State.PausedMontage.Get();

	// 정지 중에 다른 몽타주로 바뀌었으면(피격 리액션 등) 건드리지 않음
	if (AnimInstance && Montage && AnimInstance->Montage_IsActive(Montage))
	{
		AnimInstance->Montage_Resume(Montage);
	}
}

void UHitStopSubsystem::Deinitialize()
{
	for (const TPair<TWeakObjectPtr<AActor>, FHitStopState>& Pair : ActiveHitStops)
	{
		ResumeMontage(Pair.Value);
	}
	ActiveHitStops.Empty();

	Super::Deinitialize();
}