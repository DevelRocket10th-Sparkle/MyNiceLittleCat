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

	// 이동 정책 및 목표 갱신 시간 초기화
	UpdateRemaining = FMath::Max(0.05f, UpdateInterval);

	RecoveryStartedHandle = CachedPatternComp->OnRecoveryStarted.AddUObject(this, &UBTTask_Enemy_PatternMovement::RefreshMovement);
	
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

	// 현재 관측한 상태와 마지막으로 관측된 상태가 다르면 갱신
	if (SeenRequestId != PreviousRequestId
		|| bRecovering != bPreviousRecovering
		|| bLocked != bPreviousLocked)
	{
		PreviousRequestId = SeenRequestId;
		bPreviousRecovering = bRecovering;
		bPreviousLocked = bLocked;

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
	if (!CachedAI.IsValid() || !CachedPatternComp.IsValid())
	{
		CancelOwnedMove();
		return;
	}

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(CachedAI->GetPawn());

	UPathFollowingComponent* Path = CachedAI->GetPathFollowingComponent();

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
	AActor* Target = nullptr;
	int64 RequestId = 0;

	const bool bHasPattern = CachedPatternComp->GetActiveMovement(Movement, Target, RequestId);

	// 일반 Enemy이면서 패턴 정보가 유효하지 않은 경우 제한
	if (!bBossMovement
		&& (!bHasPattern || RequestId != PatternRequestId))
	{
		CancelOwnedMove();
		return;
	}

	bool bWaypointMovement = false;
	FVector Destination = FVector::ZeroVector;

	// 패턴이 없거나 패턴 정책이 UsePhaseDefault일 때만 true
	const bool bUsePhaseDefault =
		bBossMovement
		&& (!bHasPattern || Movement.Policy == EEnemyPatternMovementPolicy::UsePhaseDefault);

	if (bUsePhaseDefault)
	{
		// 실행 중 패턴이 없거나 UsePhaseDefault이므로 페이즈 기본 이동을 사용
		const FBossPhaseDefinition* Phase = CachedPhaseComp->GetCurrentPhase();

		if (!Phase)
		{
			CancelOwnedMove();
			return;
		}

		// 페이즈 이동에는 패턴별 거리 설정을 사용하지 않음
		Movement = FEnemyPatternMovementSettings();
		Movement.AcceptanceRadius = Phase->MovementAcceptanceRadius;
		Movement.bIncludeAgentRadius = false;
		Movement.bIncludeGoalRadius = false;
		Movement.bAllowPartialPath = false;
		Target = nullptr;

		// 페이즈 기본 이동 모드를 실제 실행할 이동 설정으로 변환
		switch (Phase->MovementMode)
		{
		case EBossPhaseMovementMode::Stop:
			Movement.Policy = EEnemyPatternMovementPolicy::Stop;
			break;

		case EBossPhaseMovementMode::Chasing:
		{	// 지역변수 선언을 위해 스코프를 강제로 만들어줌 (case는 자체 스코프를 만들지 않음)
			Movement.Policy = EEnemyPatternMovementPolicy::ChaseTarget;

			UBlackboardComponent* BlackboardComp = CachedAI->GetBlackboardComponent();

			Target = BlackboardComp
				? Cast<AActor>(BlackboardComp->GetValueAsObject(TEXT("TargetActor")))
				: nullptr;

			break;
		}
		case EBossPhaseMovementMode::Waypoints:
			// 실제 실행은 bWaypointMovement 분기로 구분
			Movement.Policy = EEnemyPatternMovementPolicy::UsePhaseDefault;

			// Waypoints 이동 모드일 때 목적지가 있는지 확인
			if (!CachedPhaseComp->GetCurrentWaypoint(Destination))
			{
				CancelOwnedMove();
				return;
			}

			bWaypointMovement = true;
			break;

		default:
			CancelOwnedMove();
			return;
		}
	}
	else if (Movement.Policy == EEnemyPatternMovementPolicy::UsePhaseDefault)
	{
		// 페이즈가 없는 일반 Enemy에서 이동 정책을 잘못 사용한 경우 Stop 정책 사용
		Movement.Policy = EEnemyPatternMovementPolicy::Stop;
	}

	const bool bFirstPolicy = !bHasPolicy;
	const bool bPolicyChanged =
		bFirstPolicy	// 아직 적용한 정책이 없어 최초 적용이 필요한 경우
		|| !(Movement == CachedMovement)	// 현재 이동 정책이랑 기록된 이동 정책이 다른 경우
		|| Target != CachedTarget.Get()		// 현재 이동 목표 타깃이 기록된 거랑 다른 경우
		|| bWaypointMovement != bCachedWaypointMovement	// 현재 Waypoint 이동 상태가 기록된 거랑 다른 경우
		|| (bWaypointMovement && !Destination.Equals(CachedDestination, 0.1f));	// 현재 Waypoint 이동 목적지가 기록된 거랑 다른 경우

	// 이동 정책이 바뀐 경우 이전 이동 취소 및 기록 갱신
	if (bPolicyChanged)
	{
		CancelOwnedMove();

		CachedMovement = Movement;
		CachedTarget = Target;
		bCachedWaypointMovement = bWaypointMovement;
		CachedDestination = Destination;
		bHasPolicy = true;
	}

	// GA가 만든 이동 요청을 이 Task가 반복해서 취소하지 않음
	if (Movement.Policy == EEnemyPatternMovementPolicy::AbilityDriven)
	{
		return;
	}

	// 기존 일반 이동에서 처음 한 번만 제어권을 인계 받음
	if (bFirstPolicy && Path->GetStatus() != EPathFollowingStatus::Idle)
	{
		Path->AbortMove(
			*this,
			FPathFollowingResultFlags::NewRequest,
			Path->GetCurrentRequestId()
		);
	}

	// 패턴에서 이동이 제한되는 경우 이동 취소
	// - 이동 정책이 Stop인 경우
	// - 공격 중 이동 금지인 경우
	// - 이동 정책이 Waypoint가 아닌데(Chasing) 타깃이 유효하지 않은 경우
	if (Movement.Policy == EEnemyPatternMovementPolicy::Stop
		|| Enemy->IsAttackMovementLocked()
		|| (!bWaypointMovement && !IsValid(Target)))
	{
		CancelOwnedMove();
		return;
	}

	// 진행 중인 요청을 매 갱신마다 덮어쓰지 않음
	if (Path->GetStatus() != EPathFollowingStatus::Idle)
	{
		return;
	}

	OwnedMoveId = FAIRequestID::InvalidRequest;
	FAIMoveRequest MoveRequest;
	if (bWaypointMovement)
	{
		// Waypoint 이동 모드를 사용하는 경우
		MoveRequest.SetGoalLocation(Destination);
		MoveRequest.SetAcceptanceRadius(Movement.AcceptanceRadius);
		MoveRequest.SetReachTestIncludesAgentRadius(false);
		MoveRequest.SetReachTestIncludesGoalRadius(false);
		MoveRequest.SetUsePathfinding(true);
		MoveRequest.SetAllowPartialPath(false);

		// 이동 방향과 바라보는 방향이 달라도 이동할 수 있도록 허용
		// 실제 시선 방향은 AI Focus, 캐릭터 회전 설정, 회전 잠금에 따라 결정
		MoveRequest.SetCanStrafe(true);
	}
	else
	{
		// Waypoint 이외의 이동 모드를 사용하는 경우
		MoveRequest = BuildMoveRequest(Target, Movement);
	}

	if (Path->HasReached(MoveRequest))
	{
		if (bWaypointMovement)
		{
			// Task 시작 시 저장한 페이즈 실행 번호를 사용
			CachedPhaseComp->AdvanceWaypoint(CachedPhaseExecutionId);
		}

		// 새 지점으로의 이동 요청은 다음 갱신에서 수행
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
	if (CachedPatternComp.IsValid())
	{
		CachedPatternComp->OnRecoveryStarted.Remove(RecoveryStartedHandle);
	}

	RecoveryStartedHandle.Reset();

	CancelOwnedMove();
	
	CachedAI.Reset();
	CachedPatternComp.Reset();
	CachedTarget.Reset();
	
	CachedBoss.Reset();
	CachedPhaseComp.Reset();

	PatternRequestId = 0;
	CachedPhaseExecutionId = 0;

	CachedMovement = FEnemyPatternMovementSettings();
	CachedDestination = FVector::ZeroVector;

	bHasPolicy = false;
	bCachedWaypointMovement = false;

	UpdateRemaining = 0.f;

	PreviousRequestId = 0;
	bPreviousRecovering = false;
	bPreviousLocked = false;
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
