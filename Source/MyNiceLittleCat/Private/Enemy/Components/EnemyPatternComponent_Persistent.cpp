// Fill out your copyright notice in the Description page of Project Settings.
// UEnemyPatternComponent의 상시 패턴(보스 페이즈) 부분: 랜덤 선택과 별개 트랙으로 계속 실행 (2026.11.20. 파일 분리)


#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/EnemyLog.h"

#include "Engine/World.h"
#include "TimerManager.h"

#include "Enemy/AI/EnemyAIController.h"
#include "Enemy/EnemyCharacter.h"

namespace
{
	// 상시 패턴이 조건(차단, 타깃, 거리·시야, 위험 태그) 때문에 시작하지 못했을 때 다시 시도하는 간격
	constexpr float PersistentRetrySeconds = 0.5f;

	// 시작 지연 0인 상시 패턴의 첫 시도 (타이머는 0초를 받지 않음)
	constexpr float PersistentMinDelaySeconds = 0.01f;
}

void UEnemyPatternComponent::SetPersistentPatterns(const TArray<FEnemyPersistentPatternEntry>& Entries)
{
	StopPersistentPatterns();

	if (bStopped)
	{
		return;
	}

	for (const FEnemyPersistentPatternEntry& Entry : Entries)
	{
		FString Error;
		if (!Entry.Validate(Error) || !HasPattern(Entry.PatternId))
		{
			UE_LOG(LogEnemy, Warning, TEXT("[EnemyPattern] %s: skipped persistent pattern '%s' (unknown or invalid)"),
				*GetNameSafe(GetOwner()), *Entry.PatternId.ToString());
			continue;
		}

		// 보조 실행 규칙: 몸은 랜덤 선택 패턴이 쓰므로 몸을 쓰지 않는 패턴만
		if (!Catalog.CanRunAsCompanion(Entry.PatternId))
		{
			UE_LOG(LogEnemy, Warning, TEXT("[EnemyPattern] %s: persistent pattern '%s' needs Can Run As Companion (and an ability class that does not use the body)"),
				*GetNameSafe(GetOwner()), *Entry.PatternId.ToString());
			continue;
		}

		FEnemyPersistentPatternState& State = PersistentStates.AddDefaulted_GetRef();
		State.Entry = Entry;
	}

	// 배열을 다 채운 뒤 예약: 예약 중 배열이 재할당되지 않도록
	for (int32 Index = 0; Index < PersistentStates.Num(); ++Index)
	{
		SchedulePersistentStart(Index, PersistentStates[Index].Entry.StartDelay);
	}
}

void UEnemyPatternComponent::StopPersistentPatterns()
{
	TArray<int64, TInlineAllocator<4>> RunningIds;

	UWorld* World = GetWorld();
	for (FEnemyPersistentPatternState& State : PersistentStates)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(State.StartTimer);
		}

		if (State.RequestId != 0)
		{
			RunningIds.Add(State.RequestId);
		}
	}

	// 상태를 먼저 비움: 아래 취소로 들어오는 종료 처리가 다시 시작을 예약하지 않음
	PersistentStates.Reset();

	for (const int64 RequestId : RunningIds)
	{
		CancelRequest(RequestId);
	}
}

void UEnemyPatternComponent::SchedulePersistentStart(int32 StateIndex, float Delay)
{
	UWorld* World = GetWorld();
	if (!World || !PersistentStates.IsValidIndex(StateIndex))
	{
		return;
	}

	World->GetTimerManager().SetTimer(PersistentStates[StateIndex].StartTimer,
		FTimerDelegate::CreateUObject(this, &UEnemyPatternComponent::StartPersistentPattern, StateIndex),
		FMath::Max(Delay, PersistentMinDelaySeconds), false);
}

void UEnemyPatternComponent::StartPersistentPattern(int32 StateIndex)
{
	if (bStopped || !PersistentStates.IsValidIndex(StateIndex) || PersistentStates[StateIndex].RequestId != 0)
	{
		return;
	}

	const FName PatternId = PersistentStates[StateIndex].Entry.PatternId;
	const FEnemyPatternDefinition* Pattern = Catalog.Find(PatternId);
	const AEnemyCharacter* Enemy = GetEnemy();
	AActor* Target = ResolvePersistentTarget();

	// 전환 중 차단, AI 정지, 같은 패턴 실행 중, 거리·시야·GA 조건, 위험 태그·위험도(보조 실행 자원 기준)
	// - 상시 패턴은 페이즈 랜덤 목록에 없어도 실행 (랜덤 목록에도 넣으면 주 패턴으로 뽑혀 몸을 씀)
	const bool bReady = Pattern && IsValid(Enemy) && Enemy->CanRunEnemyAI() && !bRequestsBlocked && ASC.IsValid()
		&& !IsPatternRunning(PatternId) && CanExecutePattern(*Pattern, Target, true)
		&& CanJoinNow(*Pattern, true);

	if (!bReady)
	{
		SchedulePersistentStart(StateIndex, PersistentRetrySeconds);
		return;
	}

	// 보조 슬롯으로 만들되 상시 표시: 몸 설정은 끄고, 동시 실행 수에는 세지 않음
	FEnemyPatternSlot& Slot = AddSlot(*Pattern, Target, false);
	Slot.bPersistent = true;
	const int64 RequestId = Slot.RequestId;
	PersistentStates[StateIndex].RequestId = RequestId;

	if (!ActivateSlot(RequestId))
	{
		// 활성화 실패는 슬롯이 이미 지워졌으므로 상태만 되돌리고 다시 시도
		if (PersistentStates.IsValidIndex(StateIndex) && PersistentStates[StateIndex].RequestId == RequestId)
		{
			PersistentStates[StateIndex].RequestId = 0;
			SchedulePersistentStart(StateIndex, PersistentRetrySeconds);
		}
	}
}

void UEnemyPatternComponent::HandlePersistentSlotFinished(int64 RequestId, EEnemyPatternResult Result)
{
	const int32 StateIndex = PersistentStates.IndexOfByPredicate(
		[RequestId](const FEnemyPersistentPatternState& State)
		{
			return State.RequestId == RequestId;
		});

	// 정지(StopPersistentPatterns)로 이미 비운 상태면 무시
	if (StateIndex == INDEX_NONE || bStopped)
	{
		return;
	}

	PersistentStates[StateIndex].RequestId = 0;

	// 정상 완료: 재시작 간격이 있으면 다시 실행, 0이면 이번 페이즈에서 끝
	if (Result == EEnemyPatternResult::Completed)
	{
		const float RestartInterval = PersistentStates[StateIndex].Entry.RestartInterval;
		if (RestartInterval > 0.f)
		{
			SchedulePersistentStart(StateIndex, RestartInterval);
		}
		return;
	}

	// 실패, 시간 초과, 외부 취소: 페이즈 동안 계속 굴러가야 하므로 잠시 뒤 다시 시도
	SchedulePersistentStart(StateIndex, PersistentRetrySeconds);
}

AActor* UEnemyPatternComponent::ResolvePersistentTarget() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AEnemyAIController* AI = Pawn ? Cast<AEnemyAIController>(Pawn->GetController()) : nullptr;
	return AI ? AI->GetCombatTarget() : nullptr;
}
