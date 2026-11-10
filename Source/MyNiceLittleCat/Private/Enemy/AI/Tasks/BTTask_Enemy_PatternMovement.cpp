#include "Enemy/AI/Tasks/BTTask_Enemy_PatternMovement.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "Navigation/PathFollowingComponent.h"

#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Components/EnemyPhaseComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

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

	AEnemyCharacter* Enemy = CachedAI.IsValid()
		? Cast<AEnemyCharacter>(CachedAI->GetPawn())
		: nullptr;
	if (!IsValid(Enemy) || !Enemy->CanRunEnemyAI())
	{
		return EBTNodeResult::Failed;
	}

	CachedPatternComp = Enemy->GetPatternComponent();
	if (!CachedPatternComp.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	CachedBoss = Cast<ABossCharacter>(Enemy);

	// 보스
	if (CachedBoss.IsValid())
	{
		CachedPhaseComp = CachedBoss->GetPhaseComponent();

		if (!CachedPhaseComp.IsValid()
			|| !CachedPhaseComp->IsActive()
			|| CachedPhaseComp->IsCurrentPhaseLogOnly())
		{
			return EBTNodeResult::Failed;
		}

		CachedPhaseExecutionId = CachedPhaseComp->GetCurrentPhaseExecutionId();

		if (CachedPhaseExecutionId == 0)
		{
			return EBTNodeResult::Failed;
		}
	}
	// 일반 Enemy
	else
	{
		// 일반 Enemy는 실행 중인 패턴이 있어야 함
		FEnemyPatternMovementSettings Movement;
		AActor* Target = nullptr;

		// 현재 실행 중인 패턴 요청이 없으면 이동 처리 없이 성공 종료
		if (!CachedPatternComp->GetActiveMovement(Movement, Target, PatternRequestId))
		{
			return EBTNodeResult::Succeeded;
		}
	}

	// 이후 Tick에서 상태 변화 여부를 비교할 최초 상태 저장
	FEnemyPatternMovementSettings InitialMovement;
	AActor* InitialTarget = nullptr;

	// 현재 실행 중인 패턴 요청의 이동 정보를 출력 인자로 받아옴
	CachedPatternComp->GetActiveMovement(InitialMovement, InitialTarget, PreviousRequestId);

	bPreviousRecovering = CachedPatternComp->IsRecovering();	// 현재 후딜 여부
	bPreviousLocked = Enemy->IsAttackMovementLocked();			// 현재 이동 잠금 여부
	PreviousPatternId = CachedPatternComp->GetActivePatternId();

	// 이동 정책 및 목표 갱신 시간 초기화
	UpdateRemaining = FMath::Max(0.05f, UpdateInterval);

	RecoveryStartedHandle = CachedPatternComp->OnRecoveryStarted.AddUObject(this, &UBTTask_Enemy_PatternMovement::RefreshMovement);
	
	CachedPath = CachedAI->GetPathFollowingComponent();
	if (CachedPhaseComp.IsValid() && CachedPath.IsValid())
	{
		MoveFinishedHandle = CachedPath->OnRequestFinished.AddUObject(this, &UBTTask_Enemy_PatternMovement::OnMoveFinished);
	}

	RefreshMovement();	// 구독 직후 상태 업데이트를 위해 직접 한 번 호출
	return EBTNodeResult::InProgress;
}

void UBTTask_Enemy_PatternMovement::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	AEnemyCharacter* Enemy = CachedAI.IsValid()
		? Cast<AEnemyCharacter>(CachedAI->GetPawn())
		: nullptr;

	// 종료 조건 설정
	// 유효한 Enemy, AI, PatternComponent인지 확인
	bool bShouldFinish = !IsValid(Enemy) || !Enemy->CanRunEnemyAI() || !CachedPatternComp.IsValid();
	if (!bShouldFinish)
	{
		if (CachedPhaseExecutionId != 0)
		{
			// 0이 아니면 보스 페이즈 이동으로 시작한 Task
			bShouldFinish =
				!CachedBoss.IsValid()						// 보스 유효성 검사
				|| !CachedPhaseComp.IsValid()				// 페이즈 컴포넌트 유효성 검사
				|| !CachedPhaseComp->IsActive()				// 페이즈 컴포넌트 활성화 검사
				|| CachedPhaseComp->IsCurrentPhaseLogOnly()	// 로그 전용 페이즈 여부 검사
				|| CachedPhaseComp->GetCurrentPhaseExecutionId() != CachedPhaseExecutionId;	// 현재 페이즈와 일치한 페이즈인지 검사
		}
		else
		{
			// 패턴 이동으로 시작한 Task
			bShouldFinish = !CachedPatternComp->IsRequestRunning(PatternRequestId);
		}
	}

	if (bShouldFinish)
	{
		Cleanup();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// 실행 요청, 후딜, 이동 잠금의 변화를 매 Tick 확인
	FEnemyPatternMovementSettings SeenMovement;
	AActor* SeenTarget = nullptr;
	int64 SeenRequestId = 0;
	
	CachedPatternComp->GetActiveMovement(SeenMovement, SeenTarget, SeenRequestId);

	const bool bRecovering = CachedPatternComp->IsRecovering();
	const bool bLocked = Enemy->IsAttackMovementLocked();
	const FName SeenPatternId = CachedPatternComp->GetActivePatternId();

	// 현재 관측한 상태와 마지막으로 관측된 상태가 다르면 갱신
	if (SeenRequestId != PreviousRequestId
		|| bRecovering != bPreviousRecovering
		|| bLocked != bPreviousLocked
		|| SeenPatternId != PreviousPatternId)
	{
		PreviousRequestId = SeenRequestId;
		bPreviousRecovering = bRecovering;
		bPreviousLocked = bLocked;
		PreviousPatternId = SeenPatternId;

		// 주기 갱신을 기다리지 않고 이번 Tick에 이동 정책 재평가
		UpdateRemaining = 0.f;
	}

	UpdateRemaining -= DeltaSeconds;

	// 갱신 시간이 됐거나 상태 변화로 즉시 갱신이 요청되면 이동 정책 재평가
	if (UpdateRemaining <= 0.f)
	{
		UpdateRemaining = FMath::Max(0.05f, UpdateInterval);
		RefreshMovement();
	}
}

FAIMoveRequest UBTTask_Enemy_PatternMovement::BuildChaseRequest(AActor* Target, float AcceptanceRadius)
{
	FAIMoveRequest Request(Target);

	Request.SetAcceptanceRadius(AcceptanceRadius);
	Request.SetReachTestIncludesAgentRadius(false);		// 패턴 사거리와 같은 중심 간 거리 기준
	Request.SetReachTestIncludesGoalRadius(false);
	Request.SetUsePathfinding(true);
	Request.SetAllowPartialPath(false);
	Request.SetCanStrafe(false);

	return Request;
}

FAIMoveRequest UBTTask_Enemy_PatternMovement::BuildWaypointRequest(const FVector& Destination, float AcceptanceRadius)
{
	FAIMoveRequest Request;

	Request.SetGoalLocation(Destination);
	Request.SetAcceptanceRadius(AcceptanceRadius);
	Request.SetReachTestIncludesAgentRadius(false);		// 패턴 사거리와 같은 중심 간 거리 기준
	Request.SetReachTestIncludesGoalRadius(false);
	Request.SetUsePathfinding(true);
	Request.SetAllowPartialPath(false);
	Request.SetCanStrafe(true);							// 시선은 Focus와 회전 설정이 결정

	return Request;
}

void UBTTask_Enemy_PatternMovement::RefreshMovement()
{
	if (!CachedAI.IsValid() || !CachedPatternComp.IsValid())
	{
		CancelOwnedMove();
		return;
	}

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(CachedAI->GetPawn());

	UPathFollowingComponent* Path = CachedPath.Get();

	// Enemy와 Path가 유효하지 않으면 이동 취소
	if (!IsValid(Enemy) || !Enemy->CanRunEnemyAI() || !Path)
	{
		CancelOwnedMove();
		return;
	}

	const bool bBossMovement = CachedPhaseExecutionId != 0;

	if (bBossMovement)
	{
		// 이전 페이즈의 이동 Task가 새 페이즈를 처리하지 않도록 제한
		if (!CachedBoss.IsValid()
			|| !CachedPhaseComp.IsValid()
			|| !CachedPhaseComp->IsActive()
			|| CachedPhaseComp->IsCurrentPhaseLogOnly()
			|| CachedPhaseComp->GetCurrentPhaseExecutionId() != CachedPhaseExecutionId)
		{
			CancelOwnedMove();
			return;
		}
	}

	FEnemyPatternMovementSettings Movement;
	AActor* PatternTarget = nullptr;
	int64 RequestId = 0;
	const bool bHasPattern = CachedPatternComp->GetActiveMovement(Movement, PatternTarget, RequestId);

	// 일반 Enemy는 자신이 담당한 패턴 요청에서만 동작
	if (!bBossMovement && (!bHasPattern || RequestId != PatternRequestId))
	{
		CancelOwnedMove();
		return;
	}

	EResolvedMove Move = EResolvedMove::Stop;
	AActor* Target = nullptr;
	FVector Destination = FVector::ZeroVector;
	float AcceptanceRadius = 0.f;

	// 공격 실행 중 Stop/AbilityDriven은 패턴이 결정
	// 공격 전, 방향 정렬, UsePhaseDefault, 후딜은 이동 모드를 따름 (후딜은 이동 잠금으로 정지)
	const bool bFollowLocomotion = !bHasPattern || Movement.Policy == EEnemyPatternMovementPolicy::UsePhaseDefault;

	// 다음 행동 결정
	if (!bFollowLocomotion)
	{
		Move = Movement.Policy == EEnemyPatternMovementPolicy::AbilityDriven
			? EResolvedMove::AbilityDriven
			: EResolvedMove::Stop;
	}
	else if (bBossMovement)
	{
		const FBossPhaseDefinition* Phase = CachedPhaseComp->GetCurrentPhase();
		if (!Phase)
		{
			CancelOwnedMove();
			return;
		}
		AcceptanceRadius = Phase->MovementAcceptanceRadius;

		// 실행 중 패턴 -> 선택된 패턴 -> 페이즈 기본 순으로 결정
		switch (Phase->ResolveMovementMode(CachedPatternComp->GetActivePatternId()))
		{
		case EBossPhaseMovementMode::Stop:
			Move = EResolvedMove::Stop;
			break;

		case EBossPhaseMovementMode::Chasing:
		{	// 지역 변수 사용을 위해 스코프 명시
			Move = EResolvedMove::Chase;
			UBlackboardComponent* BlackboardComp = CachedAI->GetBlackboardComponent();
			Target = BlackboardComp
				? Cast<AActor>(BlackboardComp->GetValueAsObject(TEXT("TargetActor")))
				: nullptr;
			break;
		}

		case EBossPhaseMovementMode::Waypoints:
			// 목적지 확인
			if (!CachedPhaseComp->GetCurrentWaypoint(Destination))
			{
				CancelOwnedMove();
				return;
			}

			Move = EResolvedMove::Waypoint;
			break;

		default:
			CancelOwnedMove();
			return;
		}
	}
	// 일반 Enemy의 UsePhaseDefault는 Stop (Move 기본값)

	const bool bFirstPolicy = !bHasPolicy;
	const bool bPolicyChanged =
		bFirstPolicy
		|| Move != CachedMove
		|| !FMath::IsNearlyEqual(AcceptanceRadius, CachedAcceptanceRadius)
		|| Target != CachedTarget.Get()
		|| (Move == EResolvedMove::Waypoint && !Destination.Equals(CachedDestination, 0.1f));

	if (bPolicyChanged)
	{
		CancelOwnedMove();
		CachedMove = Move;
		CachedAcceptanceRadius = AcceptanceRadius;
		CachedTarget = Target;
		CachedDestination = Destination;
		bHasPolicy = true;
	}

	// GA가 만든 이동 요청을 반복해서 취소하지 않음
	if (Move == EResolvedMove::AbilityDriven)
	{
		return;
	}

	// 기존 일반 이동에서 처음 한 번만 제어권 인계
	if (bFirstPolicy && Path->GetStatus() != EPathFollowingStatus::Idle)
	{
		Path->AbortMove(*this, FPathFollowingResultFlags::NewRequest, Path->GetCurrentRequestId());
	}

	if (Move == EResolvedMove::Stop
		|| Enemy->IsAttackMovementLocked()
		|| (Move == EResolvedMove::Chase && !IsValid(Target)))
	{
		CancelOwnedMove();
		return;
	}

	if (Path->GetStatus() != EPathFollowingStatus::Idle)
	{
		return;
	}

	OwnedMoveId = FAIRequestID::InvalidRequest;
	const FAIMoveRequest MoveRequest =
		Move == EResolvedMove::Waypoint
		? BuildWaypointRequest(Destination, AcceptanceRadius)
		: BuildChaseRequest(Target, AcceptanceRadius);

	if (Path->HasReached(MoveRequest))
	{
		if (Move == EResolvedMove::Waypoint)
		{
			CachedPhaseComp->AdvanceWaypoint(CachedPhaseExecutionId);
		}

		return;
	}

	const FPathFollowingRequestResult Result = CachedAI->MoveTo(MoveRequest);
	if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		OwnedMoveId = Result.MoveId;
	}
	else if (Move == EResolvedMove::Waypoint && Result.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		// MoveTo는 NavMesh에 투영한 좌표로 도착을 판정할 수 있다.
		CachedPhaseComp->AdvanceWaypoint(CachedPhaseExecutionId);
	}
}

void UBTTask_Enemy_PatternMovement::OnMoveFinished(FAIRequestID RequestId, const FPathFollowingResult& Result)
{
	if (!OwnedMoveId.IsValid() || RequestId != OwnedMoveId)
	{
		return;
	}

	OwnedMoveId = FAIRequestID::InvalidRequest;
	if (CachedMove == EResolvedMove::Waypoint && Result.IsSuccess()
		&& CachedPath.IsValid() && CachedPath->DidMoveReachGoal() && CachedPhaseComp.IsValid())
	{
		CachedPhaseComp->AdvanceWaypoint(CachedPhaseExecutionId);
	}
}

void UBTTask_Enemy_PatternMovement::CancelOwnedMove()
{
	const FAIRequestID MoveId = OwnedMoveId;
	OwnedMoveId = FAIRequestID::InvalidRequest;
	UPathFollowingComponent* Path = CachedPath.Get();
	if (MoveId.IsValid() && Path && Path->GetCurrentRequestId() == MoveId)
	{
		// 늦게 실행된 BT 종료 처리가 다른 행동의 새 이동을 중단하지 않도록 한다.
		Path->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, MoveId);
	}
}

void UBTTask_Enemy_PatternMovement::Cleanup()
{
	if (CachedPatternComp.IsValid())
	{
		CachedPatternComp->OnRecoveryStarted.Remove(RecoveryStartedHandle);
	}

	RecoveryStartedHandle.Reset();

	if (CachedPath.IsValid())
	{
		CachedPath->OnRequestFinished.Remove(MoveFinishedHandle);
	}
	MoveFinishedHandle.Reset();
	CancelOwnedMove();
	CachedPath.Reset();
	
	CachedAI.Reset();
	CachedPatternComp.Reset();
	CachedTarget.Reset();
	
	CachedBoss.Reset();
	CachedPhaseComp.Reset();

	PatternRequestId = 0;
	CachedPhaseExecutionId = 0;

	CachedDestination = FVector::ZeroVector;

	bHasPolicy = false;

	UpdateRemaining = 0.f;

	PreviousRequestId = 0;
	bPreviousRecovering = false;
	bPreviousLocked = false;

	CachedMove = EResolvedMove::Stop;
	CachedAcceptanceRadius = 0.f;
	PreviousPatternId = NAME_None;
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
