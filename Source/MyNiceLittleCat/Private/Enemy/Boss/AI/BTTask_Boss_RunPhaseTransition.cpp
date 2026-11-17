// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Boss/AI/BTTask_Boss_RunPhaseTransition.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Components/EnemyPatternComponent.h"

UBTTask_Boss_RunPhaseTransition::UBTTask_Boss_RunPhaseTransition()
{
	NodeName = TEXT("Run Boss Phase Transition");

	// 보스별 구독 상태를 보관하므로 인스턴스 생성
	bCreateNodeInstance = true;
	bNotifyTaskFinished = true;

	// 결과는 이벤트로 받으므로 Tick 불필요
	bNotifyTick = false;
}

EBTNodeResult::Type UBTTask_Boss_RunPhaseTransition::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Cleanup();

	AAIController* AI = OwnerComp.GetAIOwner();
	ABossCharacter* BossCharacter = AI ? Cast<ABossCharacter>(AI->GetPawn()) : nullptr;
	if (!BossCharacter || !BossCharacter->IsEncounterActive())
	{
		return EBTNodeResult::Failed;
	}

	Boss = BossCharacter;
	OwnerTree = &OwnerComp;
	TransitionId = BossCharacter->GetPendingTransitionId();

	AI->StopMovement();

	// 공격 GA가 아직 끝나지 않았으면 전환하지 않음: 데코레이터 구성이 잘못된 경우이므로 실패 처리
	if (TransitionId == 0 || !BossCharacter->GetPatternComponent()->IsIdle())
	{
		BossCharacter->ScheduleEncounterFailure();
		Cleanup();
		return EBTNodeResult::Failed;
	}

	// 연출이 없으면 BeginPhaseTransition 안에서 결과가 바로 오므로 시작 전에 구독
	ResultHandle = BossCharacter->OnPhaseTransitionResult.AddUObject(this, &UBTTask_Boss_RunPhaseTransition::HandleTransitionResult);

	bExecuting = true;
	const bool bStarted = BossCharacter->BeginPhaseTransition();
	bExecuting = false;

	if (!bStarted)
	{
		BossCharacter->ScheduleEncounterFailure();
		Cleanup();
		return EBTNodeResult::Failed;
	}

	// 시작과 동시에 끝난 경우: 반환값으로 바로 완료
	if (ImmediateResult.IsSet())
	{
		const bool bSucceeded = ImmediateResult.GetValue();
		Cleanup();
		return bSucceeded ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_Boss_RunPhaseTransition::HandleTransitionResult(int64 InTransitionId, bool bSucceeded)
{
	if (InTransitionId != TransitionId)
	{
		return;
	}

	// ExecuteTask 실행 중이면 결과만 기록 (InProgress 반환 전에 FinishLatentTask를 부르면 안 됨)
	if (bExecuting)
	{
		ImmediateResult = bSucceeded;
		return;
	}

	UBehaviorTreeComponent* OwnerComp = OwnerTree.Get();
	Cleanup();

	if (OwnerComp)
	{
		FinishLatentTask(*OwnerComp, bSucceeded ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
	}
}

EBTNodeResult::Type UBTTask_Boss_RunPhaseTransition::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 전투 중 전환이 끊기면 보스방을 초기화하여 페이즈가 어중간한 상태로 남지 않게 함
	if (Boss.IsValid() && Boss->IsEncounterActive())
	{
		Boss->ScheduleEncounterFailure();
	}

	Cleanup();
	return EBTNodeResult::Aborted;
}

void UBTTask_Boss_RunPhaseTransition::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	if (TaskResult != EBTNodeResult::Succeeded && Boss.IsValid() && Boss->IsEncounterActive())
	{
		Boss->ScheduleEncounterFailure();
	}

	Cleanup();
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UBTTask_Boss_RunPhaseTransition::Cleanup()
{
	if (Boss.IsValid())
	{
		Boss->OnPhaseTransitionResult.Remove(ResultHandle);
	}

	ResultHandle.Reset();
	Boss.Reset();
	OwnerTree.Reset();
	TransitionId = 0;
	bExecuting = false;
	ImmediateResult.Reset();
}
