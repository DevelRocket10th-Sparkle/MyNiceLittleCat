// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/AI/Tasks/BTTask_Enemy_PatternMovement.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Navigation/PathFollowingComponent.h"

#include "Enemy/AI/EnemyBlackboardKeys.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/EnemyCharacter.h"

namespace
{
	// 너무 짧은 간격으로 매 Tick 경로를 다시 요청하지 않도록 하는 최소 간격
	constexpr float MinUpdateInterval = 0.05f;

	// Pawn 또는 Pawn의 컴포넌트에서 기본 이동 제공자 검색
	IEnemyLocomotionProvider* FindLocomotionProvider(AActor* Pawn)
	{
		if (!Pawn)
		{
			return nullptr;
		}

		if (IEnemyLocomotionProvider* Provider = Cast<IEnemyLocomotionProvider>(Pawn))
		{
			return Provider;
		}

		return Cast<IEnemyLocomotionProvider>(Pawn->FindComponentByInterface(UEnemyLocomotionProvider::StaticClass()));
	}
}

UBTTask_Enemy_PatternMovement::UBTTask_Enemy_PatternMovement()
{
	NodeName = TEXT("Apply Enemy Pattern Movement");

	// 이동 요청 ID와 구독을 인스턴스마다 보관
	bCreateNodeInstance = true;
	bNotifyTick = true;
	bNotifyTaskFinished = true;

	// 에디터에서 AActor 오브젝트 키만 고를 수 있게 하고, 기존 BT 에셋을 위해 기본 키 이름 지정
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_Enemy_PatternMovement, TargetKey), AActor::StaticClass());
	TargetKey.SelectedKeyName = EnemyBlackboardKeys::TargetActor;
}

void UBTTask_Enemy_PatternMovement::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	// 키 이름 → 키 ID 변환을 에셋 로드 시 한 번만 수행
	if (const UBlackboardData* Blackboard = GetBlackboardAsset())
	{
		TargetKey.ResolveSelectedKey(*Blackboard);
	}
}

EBTNodeResult::Type UBTTask_Enemy_PatternMovement::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Cleanup();

	CachedOwnerComp = &OwnerComp;
	CachedAI = OwnerComp.GetAIOwner();

	AEnemyCharacter* Enemy = CachedAI.IsValid() ? Cast<AEnemyCharacter>(CachedAI->GetPawn()) : nullptr;
	if (!IsValid(Enemy) || !Enemy->CanRunEnemyAI() || !Enemy->GetPatternComponent())
	{
		return EBTNodeResult::Failed;
	}

	CachedEnemy = Enemy;
	CachedPatternComp = Enemy->GetPatternComponent();
	CachedPath = CachedAI->GetPathFollowingComponent();

	/* 실행 모드 결정: 기본 이동 규칙이 있으면 규칙 모드, 없으면 패턴 모드 */

	IEnemyLocomotionProvider* Provider = FindLocomotionProvider(Enemy);
	if (Provider && Provider->GetLocomotionContextId() != 0)
	{
		CachedLocomotion = TWeakInterfacePtr<IEnemyLocomotionProvider>(Provider);
		LocomotionContextId = Provider->GetLocomotionContextId();
	}
	else
	{
		// 패턴 모드: 실행 중인 패턴 요청이 없으면 할 일이 없으므로 바로 성공
		FEnemyPatternMovementSettings Movement;
		AActor* PatternTarget = nullptr;
		if (!CachedPatternComp->GetActiveMovement(Movement, PatternTarget, PatternRequestId))
		{
			Cleanup();
			return EBTNodeResult::Succeeded;
		}
	}

	/* 상태 변화 감지용 최초 값 */

	FEnemyPatternMovementSettings InitialMovement;
	AActor* InitialTarget = nullptr;
	CachedPatternComp->GetActiveMovement(InitialMovement, InitialTarget, PreviousRequestId);

	bPreviousRecovering = CachedPatternComp->IsRecovering();
	bPreviousLocked = Enemy->IsAttackMovementLocked();
	PreviousPatternId = CachedPatternComp->GetActivePatternId();
	UpdateRemaining = FMath::Max(MinUpdateInterval, UpdateInterval);

	/* 구독 */

	// 후딜 시작 시 다음 Tick을 기다리지 않고 바로 이동 정리
	RecoveryStartedHandle = CachedPatternComp->OnRecoveryStarted.AddUObject(this, &UBTTask_Enemy_PatternMovement::RefreshMovement);

	// 목적지 도착 통지는 기본 이동 규칙 모드에서만 필요
	if (LocomotionContextId != 0 && CachedPath.IsValid())
	{
		MoveFinishedHandle = CachedPath->OnRequestFinished.AddUObject(this, &UBTTask_Enemy_PatternMovement::OnMoveFinished);
	}

	RefreshMovement();
	return EBTNodeResult::InProgress;
}

void UBTTask_Enemy_PatternMovement::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	if (ShouldFinish())
	{
		Cleanup();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	/* 실행 요청, 후딜, 이동 잠금, 패턴 선택의 변화를 매 Tick 확인 */

	FEnemyPatternMovementSettings SeenMovement;
	AActor* SeenTarget = nullptr;
	int64 SeenRequestId = 0;
	CachedPatternComp->GetActiveMovement(SeenMovement, SeenTarget, SeenRequestId);

	const bool bRecovering = CachedPatternComp->IsRecovering();
	const bool bLocked = CachedEnemy->IsAttackMovementLocked();
	const FName SeenPatternId = CachedPatternComp->GetActivePatternId();

	const bool bStateChanged = SeenRequestId != PreviousRequestId
		|| bRecovering != bPreviousRecovering
		|| bLocked != bPreviousLocked
		|| SeenPatternId != PreviousPatternId;

	if (bStateChanged)
	{
		PreviousRequestId = SeenRequestId;
		bPreviousRecovering = bRecovering;
		bPreviousLocked = bLocked;
		PreviousPatternId = SeenPatternId;

		// 주기 갱신을 기다리지 않고 이번 Tick에 재평가
		UpdateRemaining = 0.f;
	}

	UpdateRemaining -= DeltaSeconds;
	if (UpdateRemaining <= 0.f)
	{
		UpdateRemaining = FMath::Max(MinUpdateInterval, UpdateInterval);
		RefreshMovement();
	}
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

/* 이동 결정 */

void UBTTask_Enemy_PatternMovement::RefreshMovement()
{
	AEnemyCharacter* Enemy = CachedEnemy.Get();
	UPathFollowingComponent* Path = CachedPath.Get();
	if (!CachedAI.IsValid() || !Enemy || !Path || !CachedPatternComp.IsValid() || !Enemy->CanRunEnemyAI() || ShouldFinish())
	{
		CancelOwnedMove();
		return;
	}

	const bool bLocomotionMode = LocomotionContextId != 0;

	FEnemyPatternMovementSettings Movement;
	AActor* PatternTarget = nullptr;
	int64 RequestId = 0;
	const bool bHasPattern = CachedPatternComp->GetActiveMovement(Movement, PatternTarget, RequestId);

	// 패턴 모드는 자신이 담당한 패턴 요청에서만 동작
	if (!bLocomotionMode && (!bHasPattern || RequestId != PatternRequestId))
	{
		CancelOwnedMove();
		return;
	}

	/* 1) 이동 종류 결정 */

	EResolvedMove Move = EResolvedMove::Stop;
	AActor* Target = nullptr;
	FVector Destination = FVector::ZeroVector;
	float AcceptanceRadius = 0.f;

	// 공격 실행 중 Stop/AbilityDriven은 패턴이 결정, 그 외에는 기본 이동 규칙
	const bool bFollowLocomotion = !bHasPattern || Movement.Policy == EEnemyPatternMovementPolicy::FollowLocomotion;

	if (!bFollowLocomotion)
	{
		Move = Movement.Policy == EEnemyPatternMovementPolicy::AbilityDriven ? EResolvedMove::AbilityDriven : EResolvedMove::Stop;
	}
	else if (bLocomotionMode)
	{
		// 실행 중 패턴 → 선택된 패턴 → 기본 순으로 예외 적용
		FEnemyLocomotionIntent Intent;
		if (!CachedLocomotion.IsValid() || !CachedLocomotion->ResolveLocomotion(CachedPatternComp->GetActivePatternId(), Intent))
		{
			CancelOwnedMove();
			return;
		}

		AcceptanceRadius = Intent.AcceptanceRadius;

		switch (Intent.Mode)
		{
		case EEnemyLocomotionMode::ChaseTarget:
			Move = EResolvedMove::Chase;
			Target = GetBlackboardTarget();
			break;

		case EEnemyLocomotionMode::MoveToLocation:
			Move = EResolvedMove::MoveToLocation;
			Destination = Intent.Destination;
			break;

		default:
			Move = EResolvedMove::Stop;
			break;
		}
	}
	// 패턴 모드의 FollowLocomotion은 기본 이동 규칙이 없으므로 Stop

	/* 2) 이전과 달라졌으면 이전 이동 취소 */

	const bool bFirstMove = !bHasAppliedMove;
	const bool bMoveChanged = bFirstMove
		|| Move != AppliedMove
		|| !FMath::IsNearlyEqual(AcceptanceRadius, AppliedAcceptanceRadius)
		|| Target != AppliedTarget.Get()
		|| (Move == EResolvedMove::MoveToLocation && !Destination.Equals(AppliedDestination, 0.1f));

	if (bMoveChanged)
	{
		CancelOwnedMove();
		bHasAppliedMove = true;
		AppliedMove = Move;
		AppliedAcceptanceRadius = AcceptanceRadius;
		AppliedTarget = Target;
		AppliedDestination = Destination;
	}

	// GA가 만든 이동 요청을 반복해서 취소하지 않음
	if (Move == EResolvedMove::AbilityDriven)
	{
		return;
	}

	// 기존 일반 이동(BT MoveTo 등)에서 처음 한 번만 제어권 인계
	if (bFirstMove && Path->GetStatus() != EPathFollowingStatus::Idle)
	{
		Path->AbortMove(*this, FPathFollowingResultFlags::NewRequest, Path->GetCurrentRequestId());
	}

	/* 3) 정지 조건 */

	if (Move == EResolvedMove::Stop || Enemy->IsAttackMovementLocked()
		|| (Move == EResolvedMove::Chase && !IsValid(Target)))
	{
		CancelOwnedMove();
		return;
	}

	// 이미 이동 중이면 같은 요청을 다시 보내지 않음
	if (Path->GetStatus() != EPathFollowingStatus::Idle)
	{
		return;
	}

	/* 4) 이동 요청 */

	OwnedMoveId = FAIRequestID::InvalidRequest;

	const FAIMoveRequest MoveRequest = Move == EResolvedMove::MoveToLocation
		? BuildLocationRequest(Destination, AcceptanceRadius)
		: BuildChaseRequest(Target, AcceptanceRadius);

	// 이미 도착한 목적지: 다음 지점으로 진행만 알림
	if (Path->HasReached(MoveRequest))
	{
		if (Move == EResolvedMove::MoveToLocation && CachedLocomotion.IsValid())
		{
			CachedLocomotion->NotifyLocomotionDestinationReached(LocomotionContextId);
		}
		return;
	}

	const FPathFollowingRequestResult Result = CachedAI->MoveTo(MoveRequest);
	if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		OwnedMoveId = Result.MoveId;
	}
	else if (Move == EResolvedMove::MoveToLocation && Result.Code == EPathFollowingRequestResult::AlreadyAtGoal
		&& CachedLocomotion.IsValid())
	{
		// MoveTo는 NavMesh에 투영한 좌표로 도착을 판정할 수 있음
		CachedLocomotion->NotifyLocomotionDestinationReached(LocomotionContextId);
	}
}

void UBTTask_Enemy_PatternMovement::OnMoveFinished(FAIRequestID RequestId, const FPathFollowingResult& Result)
{
	if (!OwnedMoveId.IsValid() || RequestId != OwnedMoveId)
	{
		return;
	}

	OwnedMoveId = FAIRequestID::InvalidRequest;

	const bool bReachedDestination = AppliedMove == EResolvedMove::MoveToLocation
		&& Result.IsSuccess() && CachedPath.IsValid() && CachedPath->DidMoveReachGoal();

	if (bReachedDestination && CachedLocomotion.IsValid())
	{
		CachedLocomotion->NotifyLocomotionDestinationReached(LocomotionContextId);
	}
}

void UBTTask_Enemy_PatternMovement::CancelOwnedMove()
{
	const FAIRequestID MoveId = OwnedMoveId;
	OwnedMoveId = FAIRequestID::InvalidRequest;

	// 늦게 실행된 BT 종료 처리가 다른 행동의 새 이동을 중단하지 않도록 자기 요청인지 확인
	UPathFollowingComponent* Path = CachedPath.Get();
	if (MoveId.IsValid() && Path && Path->GetCurrentRequestId() == MoveId)
	{
		Path->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, MoveId);
	}
}

bool UBTTask_Enemy_PatternMovement::ShouldFinish() const
{
	const AEnemyCharacter* Enemy = CachedEnemy.Get();
	if (!Enemy || !Enemy->CanRunEnemyAI() || !CachedPatternComp.IsValid())
	{
		return true;
	}

	// 기본 이동 규칙 모드: 제공자가 사라지거나 규칙 번호가 바뀌면(페이즈 변경) 종료
	if (LocomotionContextId != 0)
	{
		return !CachedLocomotion.IsValid() || CachedLocomotion->GetLocomotionContextId() != LocomotionContextId;
	}

	// 패턴 모드: 담당 패턴 요청이 끝나면 종료
	return !CachedPatternComp->IsRequestRunning(PatternRequestId);
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

	CachedAI.Reset();
	CachedEnemy.Reset();
	CachedPath.Reset();
	CachedPatternComp.Reset();
	CachedOwnerComp.Reset();
	CachedLocomotion.Reset();

	LocomotionContextId = 0;
	PatternRequestId = 0;

	PreviousRequestId = 0;
	bPreviousRecovering = false;
	bPreviousLocked = false;
	PreviousPatternId = NAME_None;
	UpdateRemaining = 0.f;

	bHasAppliedMove = false;
	AppliedMove = EResolvedMove::Stop;
	AppliedAcceptanceRadius = 0.f;
	AppliedTarget.Reset();
	AppliedDestination = FVector::ZeroVector;
}

AActor* UBTTask_Enemy_PatternMovement::GetBlackboardTarget() const
{
	const UBehaviorTreeComponent* OwnerComp = CachedOwnerComp.Get();
	const UBlackboardComponent* Blackboard = OwnerComp ? OwnerComp->GetBlackboardComponent() : nullptr;
	if (!Blackboard || !TargetKey.IsSet())
	{
		return nullptr;
	}

	return Cast<AActor>(Blackboard->GetValue<UBlackboardKeyType_Object>(TargetKey.GetSelectedKeyID()));
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

FAIMoveRequest UBTTask_Enemy_PatternMovement::BuildLocationRequest(const FVector& Destination, float AcceptanceRadius)
{
	FAIMoveRequest Request;
	Request.SetGoalLocation(Destination);
	Request.SetAcceptanceRadius(AcceptanceRadius);
	Request.SetReachTestIncludesAgentRadius(false);
	Request.SetReachTestIncludesGoalRadius(false);
	Request.SetUsePathfinding(true);
	Request.SetAllowPartialPath(false);
	Request.SetCanStrafe(true);							// 시선은 Focus와 회전 설정이 결정
	return Request;
}
