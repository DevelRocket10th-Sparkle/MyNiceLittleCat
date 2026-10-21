// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/AI/Tasks/BTTask_Enemy_ExecuteAttack.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"

#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

#include "Enemy/EnemyCharacter.h"
#include "Enemy/Components/EnemyPatternComponent.h"

UBTTask_Enemy_ExecuteAttack::UBTTask_Enemy_ExecuteAttack()
{
	NodeName = TEXT("Execute Enemy Attack");

	// Enemy별 실행 상태를 멤버 변수에 보관하므로 인스턴스 생성
	bCreateNodeInstance = true;
	
	bNotifyTick = true;
	bNotifyTaskFinished = true;

	// Actor 타입 Blackboard 키만 선택하도록 제한
	BlackboardKey.AddObjectFilter(
		this,
		GET_MEMBER_NAME_CHECKED(
			UBTTask_Enemy_ExecuteAttack,
			BlackboardKey
		),
		AActor::StaticClass()
	);
}

// Task 시작 시 행동을 요청하고 비동기 완료까지 InProgress 반환
EBTNodeResult::Type UBTTask_Enemy_ExecuteAttack::ExecuteTask(UBehaviorTreeComponent & OwnerComp, uint8 * NodeMemory)
{
	// AI Controller
	AAIController* AI = OwnerComp.GetAIOwner();
	if (!IsValid(AI))
		return EBTNodeResult::Failed;
	
	// Enemy Character
	AEnemyCharacter* Enemy =
		IsValid(AI)
		? Cast<AEnemyCharacter>(AI->GetPawn())
		: nullptr;
	if (!IsValid(Enemy) || !Enemy->CanRunEnemyAI())
		return EBTNodeResult::Failed;

	// Blackboard
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!IsValid(Blackboard))
		return EBTNodeResult::Failed;

	// Target Actor
	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName));
	// 새 패턴은 타깃이 없어도 실행 조건을 만족할 수 있으므로 컴포넌트에서 판단
	if (Enemy->GetPatternComponent()->HasPatterns())
	{
		CachedPattern = Enemy->GetPatternComponent();
		// 이 Task에서 새 공격을 시작했는지 기록
		const bool bStarted = PatternId.IsNone()
			? CachedPattern->TryStartAttack(Target, PatternRequestId)
			: CachedPattern->TryStartPattern(PatternId, Target, PatternRequestId);
		if (!bStarted) { CachedPattern.Reset(); return EBTNodeResult::Failed; }
		return EBTNodeResult::InProgress;
	}
	if (!IsValid(Target) || Target == Enemy)
		return EBTNodeResult::Failed;

	// Enemy's ASC
	UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	if (!IsValid(ASC))
		return EBTNodeResult::Failed;

	// Game Ability Spec Handle
	const FGameplayAbilitySpecHandle Handle = Enemy->GetBasicAttackHandle();
	if (!Handle.IsValid())
		return EBTNodeResult::Failed;

	// Game Ablity Spec (from Handle)
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	
	// 다른 곳에서 이미 공격이 실행됐으면 이번 공격은 실패(취소) 처리
	if (!Spec || Spec->IsActive())
		return EBTNodeResult::Failed;

	// 공격 시작을 위한 초기화
	CachedASC = ASC;
	AttackHandle = Handle;

	bOwnsAttack = false;
	bAttackEnded = false;
	bAttackCancelled = false;

	// Ability 활성화 도중 종료될 수 있으므로 종료 델리게이트 먼저 구독
	AbilityEndedDelegateHandle = ASC->OnAbilityEnded.AddUObject(this, &UBTTask_Enemy_ExecuteAttack::HandleAbilityEnded);

	// Ability 활성화 시도
	if (!ASC->TryActivateAbility(AttackHandle))
	{
		Cleanup(false);
		return EBTNodeResult::Failed;
	}

	bOwnsAttack = true;

	// 시작 성공 (아직 공격 완료는 아님)
	return EBTNodeResult::InProgress;
}

// 요청의 종료 결과를 확인하여 BT Task 완료
void UBTTask_Enemy_ExecuteAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
	if (PatternRequestId != 0)
	{
		// 종료된 결과는 정리 전에 보관: 정리 후에는 Invalid가 됨
		const EEnemyPatternResult PatternResult = CachedPattern.IsValid()
			? CachedPattern->GetRequestResult(PatternRequestId) : EEnemyPatternResult::Invalid;
		if (PatternResult == EEnemyPatternResult::Running) return;
		Cleanup(false);
		FinishLatentTask(OwnerComp, PatternResult == EEnemyPatternResult::Completed
			? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
		return;
	}
	// 기존 테스트 경로에서도 ASC가 제거되면 대기를 종료
	if (!CachedASC.IsValid()) { Cleanup(false); FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }

	if (!bAttackEnded)
		return;
	
	if (!CachedASC.IsValid())
	{
		Cleanup(false);
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	
	// 정리 전에 보관한 실행 결과
	const EBTNodeResult::Type Result =
		bAttackCancelled
		? EBTNodeResult::Failed
		: EBTNodeResult::Succeeded;

	Cleanup(false);
	FinishLatentTask(OwnerComp, Result);
}

// Task 완료 및 중단 모두 실행 참조와 요청 번호 정리
void UBTTask_Enemy_ExecuteAttack::OnTaskFinished(UBehaviorTreeComponent & OwnerComp, uint8 * NodeMemory, EBTNodeResult::Type TaskResult)
{
	Cleanup(true);
	
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

// 현재 능력의 종료 이벤트만 받아 완료 또는 취소 기록
void UBTTask_Enemy_ExecuteAttack::HandleAbilityEnded(const FAbilityEndedData & EndedData)
{
	// 다른 Ability의 종료 이벤트 무시
	if (EndedData.AbilitySpecHandle != AttackHandle)
		return;

	bAttackEnded = true;
	bAttackCancelled = EndedData.bWasCancelled;
}

// 이 Task가 소유한 공격 및 이벤트 구독 정리
void UBTTask_Enemy_ExecuteAttack::Cleanup(bool bCancelAttack)
{
	if (CachedPattern.IsValid()) CachedPattern->ReleaseRequest(PatternRequestId);
	CachedPattern.Reset();
	PatternRequestId = 0;
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		// Ability 취소 시 종료 이벤트 발생하기 때문에 종료 이벤트 구독 해제
		ASC->OnAbilityEnded.Remove(AbilityEndedDelegateHandle);

		// Ability 취소
		if (bCancelAttack && bOwnsAttack && !bAttackEnded)
		{
			ASC->CancelAbilityHandle(AttackHandle);
		}
	}

	// ASC 초기화
	CachedASC.Reset();
	AbilityEndedDelegateHandle.Reset();
	AttackHandle = FGameplayAbilitySpecHandle();

	// Attack 진행 관련 정보 초기화
	bOwnsAttack = false;
	bAttackEnded = false;
	bAttackCancelled = false;
	
}

// 브랜치 중단 시 이 Task가 시작한 요청만 취소
EBTNodeResult::Type UBTTask_Enemy_ExecuteAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Cleanup(true);
	return EBTNodeResult::Aborted;
}
