/// 작성자: 최요환
/// 작성일자: 2026-10-25
///
/// 보스 격자 레이저 패턴 GA 구현

#include "Gimmicks/Boss/Abilities/GA_BossLaserGrid.h"
#include "Gimmicks/Boss/Abilities/BossLaserGridPatternSettings.h"
#include "Gimmicks/Boss/BossGimmickComponent.h"
#include "Gimmicks/Boss/LaserGridPattern.h"
#include "Gimmicks/Boss/LaserGridSequenceData.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/GAS/EnemyAttributeSet.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"

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

// 요청 수신, Enemy 생존 확인, CommitAbility는 부모 ActivateAbility가 처리 → 여기서는 실행 조건만 검사
bool UGA_BossLaserGrid::CanStartPattern() const
{
	// 멤버 Grid는 StartPattern에서 채우므로 지역 변수 이름을 따로 씀 (이름 가림 방지)
	ALaserGridPattern* FoundGrid = FindGridPattern();
	if (!GetPatternSettings<UBossLaserGridPatternSettings>() || !FoundGrid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossLaserGrid] %s: 패턴 %s에 BossLaserGridPatternSettings가 없거나 격자 액터를 찾지 못했습니다. 매니저 PhaseGimmickActors를 확인하세요."),
			*Enemy->GetName(), *ExecutionPattern.PatternId.ToString());
		return false;
	}

	// 레벨에서 자동 시작 등으로 이미 돌고 있으면 겹치지 않게 실패 처리
	if (FoundGrid->IsPatternActive())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossLaserGrid] %s: 격자 %s가 이미 실행 중입니다."), *Enemy->GetName(), *FoundGrid->GetName());
		return false;
	}

	return true;
}

bool UGA_BossLaserGrid::StartPattern()
{
	// CanStartPattern과 같은 설정·페이즈로 다시 찾으므로 같은 격자 액터 (사이에는 CommitAbility뿐)
	GridSettings = GetPatternSettings<UBossLaserGridPatternSettings>();
	Grid = FindGridPattern();
	VolleysFired = 0;

	UE_LOG(LogTemp, Log, TEXT("[BossLaserGrid] %s: 시작 (%d회, 피해 %.1f)"), *Enemy->GetName(), GridSettings->VolleyCount, GetDamagePerHitOverride());

	// 보스를 지점으로 옮기지 않고 지금 자리에서 실행 (이동은 보스 쪽 이동 패턴 담당)
	if (GridSettings->bLockMovement)
	{
		if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
		{
			AI->StopMovement();
		}
		Enemy->GetCharacterMovement()->StopMovementImmediately();

		// 이동/회전 잠금 해제는 부모(UEnemyPatternAbility)가 EndAbility에서 처리
		LockMovement();
	}

	if (ExecutionPattern.bFaceTargetOnStart)
	{
		FaceTarget();
	}

	if (ExecutionPattern.bLockRotation)
	{
		LockRotation();
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
	return true;
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
	// StartPattern 전(GridSettings가 비어 있을 때)에도 GridActorKey가 적용되도록 부모가 받은 설정 사용
	const UBossLaserGridPatternSettings* Settings = GetPatternSettings<UBossLaserGridPatternSettings>();
	if (!Enemy.IsValid() || !Settings)
	{
		return nullptr;
	}

	const UBossGimmickComponent* Gimmick = Enemy->FindComponentByClass<UBossGimmickComponent>();
	if (!Gimmick)
	{
		return nullptr;
	}

	FName Key = Settings->GridActorKey;
	if (Key.IsNone())
	{
		const ABossCharacter* Boss = Cast<ABossCharacter>(Enemy.Get());
		Key = Boss ? Boss->GetCurrentPhaseId() : NAME_None;
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
	return Enemy->GetAttributeSet()->GetAttackPower() * ExecutionPattern.DamageMultiplier;
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
	// 활성·생존·요청 진행 확인은 부모 도우미, 격자 고유 조건만 추가
	return IsPatternExecutionValid() && IsValid(Grid) && GridSettings;
}

void UGA_BossLaserGrid::EndWithResult(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

// 부모 EndAbility가 먼저 호출 → 이 정리 뒤에 부모가 이동/회전 잠금 해제, ExecutionTarget/RequestId 등 초기화, Super::EndAbility 진행
void UGA_BossLaserGrid::StopPattern(bool bWasCancelled)
{
	// 구독을 먼저 풀어 Grid->StopPattern 중 완료 통지가 다시 들어오지 않게 함
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

	if (bWasCancelled && IsActive() && Enemy.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("[BossLaserGrid] %s: 취소"), *Enemy->GetName());
	}

	Grid = nullptr;
	GridSettings = nullptr;
	VolleysFired = 0;
}
