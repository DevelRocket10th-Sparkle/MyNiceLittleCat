#include "Enemy/GAS/Abilities/GA_Enemy_MeleeAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

#include "Enemy/Combat/EnemyTargetUtils.h"
#include "Enemy/Components/EnemyMeleeTraceComponent.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/GAS/EnemyCombatTags.h"

UGA_Enemy_MeleeAttack::UGA_Enemy_MeleeAttack()
{
	// 몽타주와 회전을 쓰므로 몸 점유: 돌진 등 다른 몸 사용 패턴과 동시에 실행되지 않음 (돌진 GA는 상속으로 함께 적용)
	DefaultPatternResources.AddTag(EnemyCombatTags::PatternResourceBody);
}

/* UEnemyPatternAbility */

bool UGA_Enemy_MeleeAttack::CanStartPattern() const
{
	const UEnemyMeleePatternSettings* Settings = GetPatternSettings<UEnemyMeleePatternSettings>();
	return Settings && Settings->Montage;
}

bool UGA_Enemy_MeleeAttack::StartPattern()
{
	MeleeSettings = GetPatternSettings<UEnemyMeleePatternSettings>();
	MeleeTraceComponent = Enemy->GetMeleeTraceComponent();

	/* 소켓 판정 준비 */

	if (!MeleeTraceComponent.IsValid() || !MeleeTraceComponent->PrepareAttack(MeleeSettings->MeleeTraceSettings, RequestId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Pattern %s has no valid melee traces"),
			*Enemy->GetName(), *ExecutionPattern.PatternId.ToString());
		return false;
	}

	TargetHitHandle = MeleeTraceComponent->OnTargetHit.AddUObject(this, &UGA_Enemy_MeleeAttack::HandleTargetHit);

	/* 이동 정지 및 잠금 */

	StopOwnerMovement();
	LockMovement();
	if (ExecutionPattern.bLockRotation)
	{
		LockRotation();
	}

	/* 판정 구간 이벤트: 몽타주 시작 전에 구독해야 첫 프레임 Notify도 받음 */

	UAbilityTask_WaitGameplayEvent* BeginTask =
		UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, EnemyCombatTags::MeleeWindowBegin, nullptr, false, true
		);
	BeginTask->EventReceived.AddDynamic(this, &UGA_Enemy_MeleeAttack::HandleWindowBegin);
	BeginTask->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* EndTask =
		UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, EnemyCombatTags::MeleeWindowEnd, nullptr, false, true
		);
	EndTask->EventReceived.AddDynamic(this, &UGA_Enemy_MeleeAttack::HandleWindowEnd);
	EndTask->ReadyForActivation();

	/* 4) 몽타주 재생: Ability가 취소되면 몽타주도 정지 */

	// 근접은 몽타주가 끝나야 완료되므로 몽타주 없이는 시작하지 않음 (몸을 쓰는 GA라 보조·상시로 실행되지 않음)
	UAbilityTask_PlayMontageAndWait* MontageTask =
		CreatePatternMontageTask(MeleeSettings->Montage, 1.f, MeleeSettings->MontageSection);
	if (!MontageTask)
	{
		return false;
	}
	MontageTask->OnCompleted.AddDynamic(this, &UGA_Enemy_MeleeAttack::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_Enemy_MeleeAttack::HandleMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_Enemy_MeleeAttack::HandleMontageCancelled);
	MontageTask->ReadyForActivation();

	if (IsActive())
	{
		OnMontageStarted();
	}

	return true;

}

void UGA_Enemy_MeleeAttack::StopPattern(bool bWasCancelled)
{
	if (MeleeTraceComponent.IsValid())
	{
		MeleeTraceComponent->OnTargetHit.Remove(TargetHitHandle);
		MeleeTraceComponent->StopAttack(RequestId);
	}

	TargetHitHandle.Reset();
	MeleeTraceComponent.Reset();
	MeleeSettings = nullptr;
	DamagedActors.Reset();
}

/* 판정 */

void UGA_Enemy_MeleeAttack::HandleTargetHit(const FHitResult& Hit)
{
	if (!IsPatternExecutionValid())
		return;

	AActor* Target = Hit.GetActor();
	if (!EnemyTargetUtils::IsAttackableTarget(Target) || DamagedActors.Contains(Target))
		return;

	// 벽 너머 타격 방지: 시야가 필요한 패턴은 맞은 순간에도 시야 확인
	if (ExecutionPattern.bRequiresLineOfSight)
	{
		const AAIController* AI = Cast<AAIController>(Enemy->GetController());
		if (!AI || !AI->LineOfSightTo(Target))
		{
			return;
		}
	}

	// 피격 방향: 공격자 -> 대상 수평 방향 (겹쳐 있으면 공격자 정면)
	FVector Direction = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Enemy->GetActorForwardVector().GetSafeNormal2D();
	}

	// 피해 콜백의 재진입(사망 -> GA 취소)에 대비해 피해 적용 전에 기록
	DamagedActors.Add(Target);
	EnemyTargetUtils::ApplyAttackHit(Target, CalculatePatternDamage(), Direction, Enemy.Get());
}

/* 몽타주 및 판정 이벤트 */

void UGA_Enemy_MeleeAttack::HandleMontageCompleted()
{
	FinishPattern();
}

void UGA_Enemy_MeleeAttack::HandleMontageCancelled()
{
	CancelPattern();
}

void UGA_Enemy_MeleeAttack::HandleWindowBegin(FGameplayEventData Payload)
{
	// 이 Enemy의 본체 메시에서 온 이벤트만 처리
	if (!IsPatternExecutionValid() || !MeleeTraceComponent.IsValid()
		|| Payload.Instigator != Enemy.Get() || Payload.OptionalObject2 != Enemy->GetMesh())
		return;

	// 다른 몽타주(피격 등)의 Notify가 섞이지 않도록 현재 공격 몽타주인지 확인
	const UAnimInstance* AnimInstance = Enemy->GetMesh()->GetAnimInstance();
	if (!MeleeSettings || !AnimInstance || AnimInstance->GetCurrentActiveMontage() != MeleeSettings->Montage)
		return;

	MeleeTraceComponent->BeginWindow(Payload.OptionalObject.Get());
}

void UGA_Enemy_MeleeAttack::HandleWindowEnd(FGameplayEventData Payload)
{
	if (MeleeTraceComponent.IsValid() && Enemy.IsValid()
		&& Payload.Instigator == Enemy.Get()
		&& Payload.OptionalObject2 == Enemy->GetMesh())
	{
		MeleeTraceComponent->EndWindow(Payload.OptionalObject.Get());
	}
}
