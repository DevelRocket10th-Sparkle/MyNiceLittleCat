#include "Enemy/AI/Tasks/BTTask_TransitionPhase.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Components/EnemyPhaseComponent.h"
#include "Enemy/Components/EnemyPatternComponent.h"

UBTTask_TransitionPhase::UBTTask_TransitionPhase()
{
	NodeName = TEXT("Transition Boss Phase");
	bCreateNodeInstance = true;
	bNotifyTick = true;
	bNotifyTaskFinished = true;
}

// Task 시작 시 행동을 요청하고 비동기 완료까지 InProgress 반환
EBTNodeResult::Type UBTTask_TransitionPhase::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	TransitionId = 0;
	// 현재 보스를 제어하는 Controller
	AAIController* AI = OwnerComp.GetAIOwner();
	Boss = AI ? Cast<ABossCharacter>(AI->GetPawn()) : nullptr;
	if (!Boss.IsValid() || !Boss->IsEncounterActive())
	{
		return EBTNodeResult::Failed;
	}
	AI->StopMovement();
	if (!Boss->GetPatternComponent()->IsIdle() || !Boss->GetPhaseComponent()->BeginPendingTransition(TransitionId))
	{
		Boss->ScheduleEncounterFailure();
		return EBTNodeResult::Failed;
	}
	return EBTNodeResult::InProgress;
}

// 요청의 종료 결과를 확인하여 BT Task 완료
void UBTTask_TransitionPhase::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (!Boss.IsValid())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	// 다음 페이즈를 실제 적용하는 컴포넌트
	UEnemyPhaseComponent* Phase = Boss->GetPhaseComponent();
	// 정리 전에 확인한 이번 요청의 결과
	const EEnemyPatternResult Result = Phase->GetTransitionResult(TransitionId);
	if (Result == EEnemyPatternResult::Running)
	{
		return;
	}
	if (Result == EEnemyPatternResult::Completed && Phase->CompleteTransition(TransitionId))
	{
		TransitionId = 0;
		Boss->FinishPhaseTransition();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
	else
	{
		Boss->ScheduleEncounterFailure();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
	}
}

// 브랜치 중단 시 이 Task가 시작한 요청만 취소
EBTNodeResult::Type UBTTask_TransitionPhase::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (Boss.IsValid() && Boss->IsEncounterActive())
	{
		Boss->ScheduleEncounterFailure();
	}
	TransitionId = 0;
	return EBTNodeResult::Aborted;
}

// Task 완료 및 중단 모두 실행 참조와 요청 번호 정리
void UBTTask_TransitionPhase::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	if (TaskResult != EBTNodeResult::Succeeded && Boss.IsValid() && Boss->IsEncounterActive())
		Boss->ScheduleEncounterFailure();
	Boss.Reset();
	TransitionId = 0;
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}
