/// 작성자: 최요환
/// 작성일자: 2026-10-25
///
/// 보스 격자 레이저 패턴 GA 구현

#include "Gimmicks/Boss/Abilities/GA_BossLaserGrid.h"
#include "Gimmicks/Boss/Abilities/AbilityTask_BossMoveToPoint.h"
#include "Gimmicks/Boss/Abilities/BossLaserGridPatternSettings.h"
#include "Gimmicks/Boss/BossGimmickComponent.h"
#include "Gimmicks/Boss/LaserGridPattern.h"
#include "Gimmicks/Boss/LaserGridSequenceData.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/GAS/EnemyAttributeSet.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Boss/Components/BossPhaseComponent.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"

UGA_BossLaserGrid::UGA_BossLaserGrid()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

bool UGA_BossLaserGrid::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UBossGimmickComponent* Gimmick = Avatar ? Avatar->FindComponentByClass<UBossGimmickComponent>() : nullptr;
	return !Gimmick || Gimmick->IsLaserTurnAllowed(EBossLaserTurn::Grid);
}

void UGA_BossLaserGrid::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Enemy = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	PatternComponent = Enemy.IsValid() ? Enemy->GetPatternComponent() : nullptr;

	AActor* Target = nullptr;
	if (!Enemy.IsValid() || !Enemy->IsAlive() || !PatternComponent.IsValid()
		|| !PatternComponent->GetActiveExecution(ExecutionPattern, Target, RequestId))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	GridSettings = Cast<UBossLaserGridPatternSettings>(ExecutionPattern.ExecutionSettings);
	ExecutionTarget = Target;
	Grid = GridSettings ? FindGridPattern() : nullptr;
	VolleysFired = 0;

	if (!GridSettings || !Grid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossLaserGrid] %s: 패턴 %s에 BossLaserGridPatternSettings가 없거나 격자 액터를 찾지 못했습니다. 매니저 PhaseGimmickActors를 확인하세요."),
			*Enemy->GetName(), *ExecutionPattern.PatternId.ToString());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 레벨에서 자동 시작 등으로 이미 돌고 있으면 겹치지 않게 실패 처리
	if (Grid->IsPatternActive())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossLaserGrid] %s: 격자 %s가 이미 실행 중입니다."), *Enemy->GetName(), *Grid->GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[BossLaserGrid] %s: 시작 (%d회, 피해 %.1f)"), *Enemy->GetName(), GridSettings->VolleyCount, GetDamagePerHitOverride());

	// 이동 잠금이 걸리면 경로 이동이 막히므로 이동을 먼저
	if (GridSettings->bMoveToStandPoint)
	{
		const FTransform StandTransform = Grid->GetBossStandTransform();
		UAbilityTask_BossMoveToPoint* MoveTask = UAbilityTask_BossMoveToPoint::BossMoveToPoint(this, StandTransform.GetLocation(),
			StandTransform.Rotator().Yaw, GridSettings->MoveAcceptanceRadius, GridSettings->MoveTimeLimit);
		MoveTask->OnArrived.AddDynamic(this, &UGA_BossLaserGrid::HandleArrived);
		MoveTask->ReadyForActivation();
	}
	else
	{
		HandleArrived();
	}
}

void UGA_BossLaserGrid::HandleArrived()
{
	if (!IsExecutionValid())
	{
		EndWithResult(true);
		return;
	}

	if (GridSettings->bLockMovement)
	{
		if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
		{
			AI->StopMovement();
		}
		Enemy->GetCharacterMovement()->StopMovementImmediately();

		Enemy->AcquireAttackMovementLock();
		bOwnsMovementLock = true;
	}

	// 대기 지점으로 갔으면 지점 방향을 유지, 아니면 타깃을 바라봄
	if (ExecutionPattern.bFaceTargetOnStart && !GridSettings->bMoveToStandPoint)
	{
		FaceTarget();
	}

	if (ExecutionPattern.bLockRotation)
	{
		Enemy->AcquireRotationLock();
		bOwnsRotationLock = true;
	}

	if (GridSettings->StartMontage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, GridSettings->StartMontage, 1.f, NAME_None, true);
		MontageTask->ReadyForActivation();
	}

	// 시퀀스가 즉시 끝나는 경우(모든 단계가 영역 밖)도 받도록 먼저 구독
	Grid->OnSequenceFinished.AddUniqueDynamic(this, &UGA_BossLaserGrid::HandleSequenceFinished);

	StartNextVolley();
}

void UGA_BossLaserGrid::StartNextVolley()
{
	if (!IsExecutionValid())
	{
		EndWithResult(true);
		return;
	}

	// 시작 전에 켜 두어야 PlaySequenceOnce 안에서 바로 끝나도 상태가 맞음 (완료 처리에서 다시 끔)
	bStartedGrid = true;
	ULaserGridSequenceData* Sequence = PickSequence();
	if (!Grid->PlaySequenceOnce(Sequence, GetDamagePerHitOverride()))
	{
		bStartedGrid = false;
		EndWithResult(true);
		return;
	}

	if (IsActive())
	{
		UE_LOG(LogTemp, Log, TEXT("[BossLaserGrid] %s: %d/%d회 발사 시작"), *Enemy->GetName(), VolleysFired + 1, GridSettings->VolleyCount);
	}
}

void UGA_BossLaserGrid::HandleSequenceFinished(ULaserGridSequenceData* Sequence)
{
	// 격자가 스스로 끝났으므로 EndAbility에서 다시 멈출 필요 없음
	bStartedGrid = false;

	if (!IsExecutionValid())
	{
		EndWithResult(true);
		return;
	}

	++VolleysFired;
	UE_LOG(LogTemp, Log, TEXT("[BossLaserGrid] %d/%d회 끝: %s"), VolleysFired, GridSettings->VolleyCount, *GetNameSafe(Sequence));

	if (VolleysFired >= GridSettings->VolleyCount)
	{
		if (UBossGimmickComponent* Gimmick = Enemy->FindComponentByClass<UBossGimmickComponent>())
		{
			Gimmick->CompleteLaserTurn(EBossLaserTurn::Grid);
		}
		EndWithResult(false);
		return;
	}

	if (GridSettings->VolleyInterval > 0.f)
	{
		UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, GridSettings->VolleyInterval);
		WaitTask->OnFinish.AddDynamic(this, &UGA_BossLaserGrid::StartNextVolley);
		WaitTask->ReadyForActivation();
	}
	else
	{
		StartNextVolley();
	}
}

ALaserGridPattern* UGA_BossLaserGrid::FindGridPattern() const
{
	if (!Enemy.IsValid() || !GridSettings)
	{
		return nullptr;
	}

	const UBossGimmickComponent* Gimmick = Enemy->FindComponentByClass<UBossGimmickComponent>();
	if (!Gimmick)
	{
		return nullptr;
	}

	FName Key = GridSettings->GridActorKey;
	if (Key.IsNone())
	{
		const ABossCharacter* Boss = Cast<ABossCharacter>(Enemy.Get());
		const UBossPhaseComponent* Phase = Boss ? Boss->GetPhaseComponent() : nullptr;
		Key = Phase ? Phase->GetCurrentPhaseId() : NAME_None;
	}

	return Cast<ALaserGridPattern>(Gimmick->GetPhaseActor(Key));
}

ULaserGridSequenceData* UGA_BossLaserGrid::PickSequence()
{
	TArray<int32> ValidIndices;
	for (int32 Index = 0; Index < GridSettings->Sequences.Num(); ++Index)
	{
		if (GridSettings->Sequences[Index] && !GridSettings->Sequences[Index]->Steps.IsEmpty())
		{
			ValidIndices.Add(Index);
		}
	}

	if (ValidIndices.IsEmpty())
	{
		return nullptr;
	}

	if (GridSettings->bRandomSequenceOrder)
	{
		// 같은 시퀀스가 연속으로 나오지 않게 (2개 이상일 때)
		if (ValidIndices.Num() > 1)
		{
			ValidIndices.Remove(LastSequenceIndex);
		}
		LastSequenceIndex = ValidIndices[FMath::RandRange(0, ValidIndices.Num() - 1)];
	}
	else
	{
		const int32 Position = ValidIndices.IndexOfByKey(LastSequenceIndex);
		LastSequenceIndex = ValidIndices[(Position + 1) % ValidIndices.Num()];
	}

	return GridSettings->Sequences[LastSequenceIndex];
}

float UGA_BossLaserGrid::GetDamagePerHitOverride() const
{
	if (!GridSettings || !GridSettings->bUseBossAttackPower || !Enemy.IsValid() || !Enemy->GetAttributeSet())
	{
		return -1.f;
	}

	// 근접 패턴과 같은 공식
	return Enemy->GetAttributeSet()->GetAttackPower() * GridSettings->DamageMultiplier;
}

void UGA_BossLaserGrid::FaceTarget()
{
	if (!Enemy.IsValid() || !IsValid(ExecutionTarget))
	{
		return;
	}

	FVector Direction = ExecutionTarget->GetActorLocation() - Enemy->GetActorLocation();
	Direction.Z = 0.f;
	if (!Direction.IsNearlyZero())
	{
		Enemy->SetActorRotation(Direction.Rotation());
	}
}

bool UGA_BossLaserGrid::IsExecutionValid() const
{
	return IsActive() && Enemy.IsValid() && Enemy->IsAlive() && PatternComponent.IsValid()
		&& PatternComponent->IsRequestRunning(RequestId) && IsValid(Grid) && GridSettings;
}

void UGA_BossLaserGrid::EndWithResult(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UGA_BossLaserGrid::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 구독을 먼저 풀어 StopPattern 중 완료 통지가 다시 들어오지 않게 함
	if (IsValid(Grid))
	{
		Grid->OnSequenceFinished.RemoveDynamic(this, &UGA_BossLaserGrid::HandleSequenceFinished);

		// 중간에 끝나면(취소) 이 GA가 시작한 시퀀스의 남은 경고·레이저 정리
		if (bStartedGrid && Grid->IsPlayingOnce())
		{
			Grid->StopPattern();
		}
	}
	bStartedGrid = false;

	if (Enemy.IsValid())
	{
		if (bOwnsRotationLock) Enemy->ReleaseRotationLock();
		if (bOwnsMovementLock) Enemy->ReleaseAttackMovementLock();
	}
	bOwnsRotationLock = false;
	bOwnsMovementLock = false;

	if (bWasCancelled && IsActive() && Enemy.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("[BossLaserGrid] %s: 취소"), *Enemy->GetName());
	}

	Grid = nullptr;
	GridSettings = nullptr;
	ExecutionTarget = nullptr;
	RequestId = 0;
	VolleysFired = 0;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
