#include "Enemy/AI/Tasks/BTTask_Enemy_ExecuteAttack.h"

#include "GameFramework/CharacterMovementComponent.h"	// 공격 시 캐릭터 회전 처리에 필요

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
	Cleanup();

	// AIC 찾기
	AAIController* AI = OwnerComp.GetAIOwner();

	// Enemy 찾기
	AEnemyCharacter* Enemy = IsValid(AI)
		? Cast<AEnemyCharacter>(AI->GetPawn())
		: nullptr;

	// BlackboardComponent 찾기
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();

	// AIC, Enenmy, BlackboardComponent 중 하나라도 없으면 실패 처리
	if (!IsValid(Enemy) || !Enemy->CanRunEnemyAI() || !IsValid(Blackboard))
	{
		return EBTNodeResult::Failed;
	}

	CachedEnemy = Enemy;
	CachedPatternComp = Enemy->GetPatternComponent();	// 패턴 컴포넌트 찾기

	// Pattern 정보 없으면 실패 처리
	if (!CachedPatternComp.IsValid() || !CachedPatternComp->HasPatterns())
	{
		Cleanup();
		return EBTNodeResult::Failed;
	}

	// 타깃 설정
	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName));
	CachedTarget = Target;

	// 선택된 패턴 실행 시도 (선택된 패턴이 없으면 선택해서 시도)
	// 타깃이 필요 없는 패턴도 있으므로 실행 조건은 컴포넌트에서 판단
	if (!IsValid(Target))
	{
		if (!StartAttack())
		{
			Cleanup();
			return EBTNodeResult::Failed;	// 패턴 실행 실패
		}

		return EBTNodeResult::InProgress;	// 패턴 실행 성공
	}

	// 이동은 막되, 회전은 허용
	Enemy->AcquireAttackMovementLock();
	bOwnsFacingMovementLock = true;

	AI->StopMovement();
	Enemy->GetCharacterMovement()->StopMovementImmediately();
	Enemy->ConsumeMovementInputVector();

	AI->SetFocus(Target, EAIFocusPriority::Gameplay);

	FacingElapsed = 0.f;	// Cleaup()에서 이미 초기화하지만, 명시적으로 한 번 더 초기화
	bWaitingForFacing = true;

	return EBTNodeResult::InProgress;		// 패턴 실행 성공
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

		// 목표 방향으로 회전을 완료하면 공격 Ability가 이동 잠금을 관리하도록 넘김
		bWaitingForFacing = false;
		ReleaseFacingMovementLock();

		// 회전 완료 후 공격 시도 실패하면 실패 처리
		if (!StartAttack())
		{
			Cleanup();
			FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		}
	
		return;
	}

	/* 공격 시작 이후에는 기존처럼 요청 종료를 기다림 */
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
	// 정렬 중 BT가 중단돼도 이동 잠금이 남지 않도록 처리
	ReleaseFacingMovementLock();

	// 진행 중인 요청은 취소하고, 끝난 요청은 결과 보관을 해제
	if (CachedPatternComp.IsValid())
	{
		CachedPatternComp->ReleaseRequest(PatternRequestId);
	}
	
	// 캐싱 해놓은 것들 초기화
	CachedPatternComp.Reset();
	CachedEnemy.Reset();
	CachedTarget.Reset();

	// 기억 해놓은 상태 초기화
	PatternRequestId = 0;
	bWaitingForFacing = false;
	FacingElapsed = 0.f;
}

bool UBTTask_Enemy_ExecuteAttack::StartAttack()
{
	// 패턴 컴포넌트 준비 안 됐으면 false
	if (!CachedPatternComp.IsValid())
		return false;

	// 패턴 실행 요청
	return PatternId.IsNone()
		? CachedPatternComp->TryStartAttack(CachedTarget.Get(), PatternRequestId)
		: CachedPatternComp->TryStartPattern(PatternId, CachedTarget.Get(), PatternRequestId);
}

void UBTTask_Enemy_ExecuteAttack::ReleaseFacingMovementLock()
{
	if (!bOwnsFacingMovementLock)
		return;

	if (AEnemyCharacter* Enemy = CachedEnemy.Get())
	{
		Enemy->ReleaseAttackMovementLock();
	}

	bOwnsFacingMovementLock = false;
}
