// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/GAS/Abilities/EnemyPatternAbility.h"

#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/GAS/EnemyAttributeSet.h"

UEnemyPatternAbility::UEnemyPatternAbility()
{
	// 실행 상태(요청 번호, 잠금)를 인스턴스에 보관하고 서버에서만 실행
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UEnemyPatternAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	/* 실행 컨텍스트 수신 */

	Enemy = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	PatternComponent = Enemy.IsValid() ? Enemy->GetPatternComponent() : nullptr;

	// GetExecutionForAbility가 출력할 원래 타깃
	AActor* Target = nullptr;

	// 패턴 실행에 필요한 정보가 하나라도 없으면 취소 종료
	if (!Enemy.IsValid() || !Enemy->IsAlive() || !PatternComponent.IsValid()
		|| !PatternComponent->GetExecutionForAbility(Handle, ExecutionPattern, Target, RequestId))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ExecutionTarget = Target;

	/* 설정 검사 후 실행 확정 */

	if (!CanStartPattern() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Pattern '%s' cannot start '%s'"),
			*GetNameSafe(Enemy.Get()), *ExecutionPattern.PatternId.ToString(), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	/* 파생 GA 동작 시작 */
	
	// StartPattern 안에서 즉시 끝날 수 있으믈 활성 상태일 때만 실패 처리
	if (!StartPattern() && IsActive())
	{
		CancelPattern();
	}
}

void UEnemyPatternAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo * ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 파생 GA가 만든 자원부터 정리
	StopPattern(bWasCancelled);

	// 이 GA가 획득한 잠금만 해제
	if (Enemy.IsValid())
	{
		if (bOwnsRotationLock)
		{
			Enemy->ReleaseRotationLock();
		}

		if (bOwnsMovementLock)
		{
			Enemy->ReleaseAttackMovementLock();
		}
	}

	bOwnsRotationLock = false;
	bOwnsMovementLock = false;

	ExecutionPattern = FEnemyPatternDefinition();
	ExecutionTarget = nullptr;
	PatternComponent.Reset();
	Enemy.Reset();
	RequestId = 0;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UEnemyPatternAbility::IsPatternExecutionValid() const
{
	return IsActive()
		&& Enemy.IsValid() && Enemy->IsAlive()
		&& PatternComponent.IsValid() && PatternComponent->IsRequestRunning(RequestId);
}

void UEnemyPatternAbility::FinishPattern()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UEnemyPatternAbility::CancelPattern()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UEnemyPatternAbility::LockMovement()
{
	if (!bOwnsMovementLock && Enemy.IsValid())
	{
		Enemy->AcquireAttackMovementLock();
		bOwnsMovementLock = true;
	}
}

void UEnemyPatternAbility::LockRotation()
{
	if (!bOwnsRotationLock && Enemy.IsValid())
	{
		Enemy->AcquireRotationLock();
		bOwnsRotationLock = true;
	}
}

float UEnemyPatternAbility::CalculatePatternDamage() const
{
	const UEnemyAttributeSet* Stat = Enemy.IsValid() ? Enemy->GetAttributeSet() : nullptr;

	return Stat ? Stat->GetAttackPower() * ExecutionPattern.DamageMultiplier : 0.f;
}

void UEnemyPatternAbility::RegisterLingeringHazard(AActor* SpawnedActor) const
{
	if (PatternComponent.IsValid())
	{
		PatternComponent->RegisterLingeringHazard(RequestId, SpawnedActor);
	}
}
