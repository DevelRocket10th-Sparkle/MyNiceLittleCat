#include "Enemy/AI/Tasks/BTTask_Enemy_ExecuteAttack.h"

#include "GameFramework/CharacterMovementComponent.h"	// 공격 시 캐릭터 회전 처리에 필요

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/Components/EnemyPatternComponent.h"

#include "Enemy/AI/EnemyAIController.h"

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
	Cleanup();

	// AIC 찾기
	AAIController* AI = OwnerComp.GetAIOwner();

	// Enemy 찾기
	AEnemyCharacter* Enemy = IsValid(AI)
		? Cast<AEnemyCharacter>(AI->GetPawn())
		: nullptr;

	// BlackboardComponent 찾기
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

	// AIC, Enenmy, BlackboardComponent 중 하나라도 없으면 실패 처리
	if (!IsValid(Enemy) || !Enemy->CanRunEnemyAI() || !IsValid(BlackboardComp))
	{
		return EBTNodeResult::Failed;
	}

	CachedEnemy = Enemy;
	CachedAI = AI;
	CachedPatternComp = Enemy->GetPatternComponent();	// 패턴 컴포넌트 찾기

	// Pattern 정보 없으면 실패 처리
	if (!CachedPatternComp.IsValid() || !CachedPatternComp->HasPatterns())
	{
		Cleanup();
		return EBTNodeResult::Failed;
	}

	// 타깃 설정
	AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(BlackboardKey.SelectedKeyName));
	CachedTarget = Target;

	// PatternId는 BT 설정값으로 유지
	// 이번 실행에서 확정한 값만 SelectedPatternId에 저장
	SelectedPatternId = PatternId;

	if (SelectedPatternId.IsNone())
	{
		// 자동 선택: 실행 가능한 후보 중 한 번만 추첨
		if (!CachedPatternComp->PickAttackPattern(Target, SelectedPatternId))
		{
			Cleanup();
			return EBTNodeResult::Failed;
		}
	}
	else
	{
		// 직접 지정: 가중치와 무관하게 해당 패턴의 실행 가능 여부 검사
		if (!CachedPatternComp->CanStartPattern(SelectedPatternId, Target))
		{
			Cleanup();
			return EBTNodeResult::Failed;
		}
	}

	FEnemyPatternDefinition Definition;

	if (!CachedPatternComp->GetPatternDefinition(SelectedPatternId, Definition))
	{
		Cleanup();
		return EBTNodeResult::Failed;
	}
	
	// 실제 패턴 실행 전에 타깃 방향 회전을 기다려야 하는지 검사
	const bool bNeedsFacing = !Definition.bLogOnly && Definition.bFaceTargetOnStart && IsValid(Target);
	if (!bNeedsFacing)
	{
		// 사전 방향 회전이 필요 없으면 선택한 패턴을 즉시 실행
		if (!StartAttack())
		{
			Cleanup();
			return EBTNodeResult::Failed;
		}

		return EBTNodeResult::InProgress;
	}

	// 회전 때문에 이동을 강제로 멈추지 않음
	// 실행 중 정지는 패턴 이동 정책 또는 GA의 이동 잠금이 담당
	AI->SetFocus(Target, EnemyFocusPriority::AttackFacing);
	bOwnsAttackFacingFocus = true;

	FacingElapsed = 0.f;
	bWaitingForFacing = true;

	return EBTNodeResult::InProgress;
}

void UBTTask_Enemy_ExecuteAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	/* 공격 시작 전 타깃 방향으로 바라보기 */

	// Enemy 찾아서 AI 상태 확인
	AEnemyCharacter* Enemy = CachedEnemy.Get();
	if (!IsValid(Enemy) || !Enemy->CanRunEnemyAI())
	{
		Cleanup();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;	// AI 실행 불가 상태면 Tick 멈추기
	}
	
	if (bWaitingForFacing)
	{
		// 바라볼 타깃 없으면 실패 처리
		AActor* Target = CachedTarget.Get();
		if (!IsValid(Target))
		{
			Cleanup();
			FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
			return;
		}

		FacingElapsed += DeltaSeconds;

		FVector Direction = Target->GetActorLocation() - Enemy->GetActorLocation();
		Direction.Z = 0.f;	// 높이 차이를 제외하고 수평 방향만 계산

		const float DesiredYaw = Direction.Rotation().Yaw;	// 목표 방향의 Yaw
		const float YawError = FMath::Abs(
			FMath::FindDeltaAngleDegrees(Enemy->GetActorRotation().Yaw, DesiredYaw)
		);	// 목표 방향과 현재 Enemy 방향의 차이의 절대값

		// 목표 방향을 바라보고 있으면 true, 아니면 false
		const bool bFacingTarget = Direction.IsNearlyZero() || YawError <= FacingTolerance;
	
		if (!bFacingTarget)
		{
			// 회전하는 데 너무 오래 걸리면 실패 처리
			if (FacingElapsed >= FacingTimeout)
			{
				Cleanup();
				FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
			}

			return;
		}

		// 목표 방향으로 회전을 완료하면 선택해 둔 패턴 실행
		bWaitingForFacing = false;

		// 회전 완료 후 공격 시도 실패하면 실패 처리
		if (!StartAttack())
		{
			Cleanup();
			FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		}
	
		return;
	}

	// Running에는 GA 실행 뿐만 아니라 후딜 대기도 포함
	// Completed가 되어야 공격 Task를 성공 종료
	const EEnemyPatternResult Result = CachedPatternComp.IsValid()
		? CachedPatternComp->GetRequestResult(PatternRequestId)
		: EEnemyPatternResult::Invalid;

	if (Result == EEnemyPatternResult::Running)
		return;

	Cleanup();
	FinishLatentTask(
		OwnerComp,
		Result == EEnemyPatternResult::Completed
		? EBTNodeResult::Succeeded
		: EBTNodeResult::Failed
	);
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
	// 진행 중인 요청은 취소하고, 끝난 요청은 결과 보관 해제
	if (CachedPatternComp.IsValid())
	{
		CachedPatternComp->ReleaseRequest(PatternRequestId);
	}

	// 이 Task가 설정한 공격 Focus만 해제
	// -> 해제 후 남아 있는 가장 높은 Focus가 회전 목표가 됨
	// -> 실제 몸 회전은 회전 잠금과 CharacterMovement의 회전 설정을 따름
	if (bOwnsAttackFacingFocus)
	{
		if (AAIController* AI = CachedAI.Get())
		{
			// 다른 Pawn으로 재빙의한 컨트롤러의 Focus는 건드리지 않음
			if (AI->GetPawn() == CachedEnemy.Get())
			{
				AI->ClearFocus(EnemyFocusPriority::AttackFacing);
			}
		}
	}

	bOwnsAttackFacingFocus = false;
	
	// 캐싱 해놓은 것들 초기화
	CachedPatternComp.Reset();
	CachedEnemy.Reset();
	CachedTarget.Reset();
	CachedAI.Reset();

	// 기억 해놓은 상태 초기화
	SelectedPatternId = NAME_None;
	PatternRequestId = 0;
	bWaitingForFacing = false;
	FacingElapsed = 0.f;
}

bool UBTTask_Enemy_ExecuteAttack::StartAttack()
{
	// 패턴 컴포넌트나 선택된 패턴이 준비 안 됐으면 실패 처리
	if (!CachedPatternComp.IsValid()
		|| SelectedPatternId.IsNone())
		return false;

	// 사전에 선택한 패턴을 재추첨 없이 실행
	// -> 회전을 기다리는 동안 조건이 바뀔 수 있으므로 TryStartPattern에서 재검사
	return CachedPatternComp->TryStartPattern(SelectedPatternId, CachedTarget.Get(), PatternRequestId);
}
