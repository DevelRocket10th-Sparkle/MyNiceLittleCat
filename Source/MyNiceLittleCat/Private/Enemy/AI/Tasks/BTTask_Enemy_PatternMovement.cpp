#include "Enemy/AI/Tasks/BTTask_Enemy_PatternMovement.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "Navigation/PathFollowingComponent.h"

UBTTask_Enemy_PatternMovement::UBTTask_Enemy_PatternMovement()
{
	NodeName = TEXT("Apply Enemy Pattern Movement");
	bCreateNodeInstance = true;
	bNotifyTick = true;
	bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBTTask_Enemy_PatternMovement::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Cleanup();
	CachedAI = OwnerComp.GetAIOwner();
	AEnemyCharacter* Enemy = CachedAI.IsValid() ? Cast<AEnemyCharacter>(CachedAI->GetPawn()) : nullptr;
	if (!IsValid(Enemy) || !Enemy->CanRunEnemyAI())
	{
		return EBTNodeResult::Failed;
	}
	CachedPattern = Enemy->GetPatternComponent();
	FEnemyPatternMovementSettings Movement;
	AActor* Target = nullptr;
	if (!CachedPattern.IsValid() || !CachedPattern->GetActiveMovement(Movement, Target, PatternRequestId))
	{
		// 즉시 완료되는 로그 전용 패턴 등에서는 이동을 요청하지 않는다.
		return EBTNodeResult::Succeeded;
	}
	UpdateRemaining = FMath::Max(0.05f, UpdateInterval);
	RecoveryStartedHandle = CachedPattern->OnRecoveryStarted.AddUObject(this, &UBTTask_Enemy_PatternMovement::RefreshMovement);
	RefreshMovement();
	return EBTNodeResult::InProgress;
}

void UBTTask_Enemy_PatternMovement::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
	if (!CachedAI.IsValid() || !CachedPattern.IsValid() || !CachedPattern->IsRequestRunning(PatternRequestId))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}
	UpdateRemaining -= DeltaSeconds;
	if (UpdateRemaining <= 0.f)
	{
		UpdateRemaining = FMath::Max(0.05f, UpdateInterval);
		RefreshMovement();
	}
}

FAIMoveRequest UBTTask_Enemy_PatternMovement::BuildMoveRequest(AActor* Target, const FEnemyPatternMovementSettings& Settings)
{
	FAIMoveRequest Request(Target);
	Request.SetAcceptanceRadius(Settings.AcceptanceRadius);
	Request.SetReachTestIncludesAgentRadius(Settings.bIncludeAgentRadius);
	Request.SetReachTestIncludesGoalRadius(Settings.bIncludeGoalRadius);
	Request.SetUsePathfinding(true);
	Request.SetAllowPartialPath(Settings.bAllowPartialPath);
	Request.SetCanStrafe(false);
	return Request;
}

void UBTTask_Enemy_PatternMovement::RefreshMovement()
{
	FEnemyPatternMovementSettings Movement;
	AActor* Target = nullptr;
	int64 RequestId = 0;
	if (!CachedAI.IsValid() || !CachedPattern.IsValid()
		|| !CachedPattern->GetActiveMovement(Movement, Target, RequestId) || RequestId != PatternRequestId)
	{
		CancelOwnedMove();
		return;
	}
	UPathFollowingComponent* Path = CachedAI->GetPathFollowingComponent();
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(CachedAI->GetPawn());
	if (!Path || !IsValid(Enemy) || !Enemy->CanRunEnemyAI())
	{
		CancelOwnedMove();
		return;
	}
	const bool bFirstPolicy = !bHasPolicy;
	if (bFirstPolicy || !(Movement == CachedMovement) || Target != CachedTarget.Get())
	{
		CancelOwnedMove();
		CachedMovement = Movement;
		CachedTarget = Target;
		bHasPolicy = true;
	}
	if (Movement.Policy == EEnemyPatternMovementPolicy::AbilityDriven)
	{
		return;
	}
	if (bFirstPolicy && Path->GetStatus() != EPathFollowingStatus::Idle)
	{
		// BT의 이전 일반 이동에서 제어권을 한 번만 인계받는다. 이후에는 소유한 요청만 정리한다.
		Path->AbortMove(*this, FPathFollowingResultFlags::NewRequest, Path->GetCurrentRequestId());
	}
	if (Movement.Policy == EEnemyPatternMovementPolicy::Stop || Enemy->IsAttackMovementLocked() || !IsValid(Target))
	{
		CancelOwnedMove();
		return;
	}
	if (Path->GetStatus() != EPathFollowingStatus::Idle)
	{
		// 이동 목표 액터의 위치 갱신은 경로 추종에 맡긴다. 다른 요청도 반복 호출로 덮어쓰지 않는다.
		return;
	}
	OwnedMoveId = FAIRequestID::InvalidRequest;
	const FAIMoveRequest MoveRequest = BuildMoveRequest(Target, Movement);
	if (Path->HasReached(MoveRequest))
	{
		return;
	}
	const FPathFollowingRequestResult Result = CachedAI->MoveTo(MoveRequest);
	if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		OwnedMoveId = Result.MoveId;
	}
}

void UBTTask_Enemy_PatternMovement::CancelOwnedMove()
{
	if (CachedAI.IsValid() && OwnedMoveId.IsValid())
	{
		UPathFollowingComponent* Path = CachedAI->GetPathFollowingComponent();
		if (Path && Path->GetCurrentRequestId() == OwnedMoveId)
		{
			// 늦게 실행된 BT 종료 처리가 다른 행동의 새 이동을 중단하지 않도록 한다.
			Path->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, OwnedMoveId);
		}
	}
	OwnedMoveId = FAIRequestID::InvalidRequest;
}

void UBTTask_Enemy_PatternMovement::Cleanup()
{
	if (CachedPattern.IsValid())
	{
		CachedPattern->OnRecoveryStarted.Remove(RecoveryStartedHandle);
	}
	RecoveryStartedHandle.Reset();
	CancelOwnedMove();
	CachedAI.Reset();
	CachedPattern.Reset();
	CachedTarget.Reset();
	PatternRequestId = 0;
	bHasPolicy = false;
}

EBTNodeResult::Type UBTTask_Enemy_PatternMovement::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Cleanup();
	return EBTNodeResult::Aborted;
}

void UBTTask_Enemy_PatternMovement::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	Cleanup();
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}
