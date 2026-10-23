#include "Enemy/AI/Tasks/BTTask_Enemy_ExecuteAttack.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/Components/EnemyPatternComponent.h"

UBTTask_Enemy_ExecuteAttack::UBTTask_Enemy_ExecuteAttack()
{
	NodeName = TEXT("Execute Enemy Attack");
	bCreateNodeInstance = true;
	bNotifyTick = true;
	bNotifyTaskFinished = true;
	BlackboardKey.AddObjectFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTTask_Enemy_ExecuteAttack, BlackboardKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_Enemy_ExecuteAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	AEnemyCharacter* Enemy = IsValid(AI) ? Cast<AEnemyCharacter>(AI->GetPawn()) : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!IsValid(Enemy) || !Enemy->CanRunEnemyAI() || !IsValid(Blackboard))
		return EBTNodeResult::Failed;

	CachedPattern = Enemy->GetPatternComponent();
	if (!CachedPattern.IsValid() || !CachedPattern->HasPatterns())
	{
		Cleanup();
		return EBTNodeResult::Failed;
	}

	// 타깃이 필요 없는 패턴도 있으므로 실행 조건은 컴포넌트에서 판단
	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName));
	const bool bStarted = PatternId.IsNone()
		? CachedPattern->TryStartAttack(Target, PatternRequestId)
		: CachedPattern->TryStartPattern(PatternId, Target, PatternRequestId);
	if (!bStarted)
	{
		Cleanup();
		return EBTNodeResult::Failed;
	}
	return EBTNodeResult::InProgress;
}

void UBTTask_Enemy_ExecuteAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
	const EEnemyPatternResult Result = CachedPattern.IsValid()
		? CachedPattern->GetRequestResult(PatternRequestId) : EEnemyPatternResult::Invalid;
	if (Result == EEnemyPatternResult::Running) return;
	Cleanup();
	FinishLatentTask(OwnerComp, Result == EEnemyPatternResult::Completed
		? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
}

void UBTTask_Enemy_ExecuteAttack::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	Cleanup();
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

EBTNodeResult::Type UBTTask_Enemy_ExecuteAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Cleanup();
	return EBTNodeResult::Aborted;
}

void UBTTask_Enemy_ExecuteAttack::Cleanup()
{
	// 진행 중인 요청은 취소하고, 끝난 요청은 결과 보관을 해제
	if (CachedPattern.IsValid()) CachedPattern->ReleaseRequest(PatternRequestId);
	CachedPattern.Reset();
	PatternRequestId = 0;
}
