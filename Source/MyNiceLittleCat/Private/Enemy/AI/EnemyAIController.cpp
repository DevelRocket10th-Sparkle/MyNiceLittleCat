// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/AI/EnemyAIController.h"

#include "BrainComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

#include "Enemy/EnemyCharacter.h"
#include "Enemy/Datas/EnemyConfigData.h"

namespace EnemyBlackboardKeys
{
	const FName TargetActor(TEXT("TargetActor"));
	const FName HomeLocation(TEXT("HomeLocation"));
	const FName CanAct(TEXT("CanAct"));
}

AEnemyAIController::AEnemyAIController()
{
	// AI 로직 자동 실행 방지
	// -> Enemy의 초기화 완료를 확인한 뒤 직접 시작
	bStartAILogicOnPossess = false;
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	TryStartEnemyAI();
}

void AEnemyAIController::OnUnPossess()
{
	if (UBrainComponent* Brain = GetBrainComponent())
	{
		Brain->StopLogic(TEXT("Enemy unpossessed"));
	}

	StopMovement();			// 움직임 멈추기
	ClearCombatTarget();	// 전투 종료
}

void AEnemyAIController::TryStartEnemyAI()
{
	/* 현재 제어 중인 Pawn 확인 */
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetPawn());
	if (!IsValid(Enemy))
		return;

	/* Enemy 초기화 여부 및 생존 여부 확인 */
	if (!Enemy->IsEnemyInitialized() || !Enemy->IsAlive())
		return;

	/* 이미 실행 중이면 중복 시작 방지 */
	if (UBrainComponent* Brain = GetBrainComponent())
	{
		if (Brain->IsRunning())
			return;
	}

	/* Enemy 설정과 BT 확인 */
	const UEnemyConfigData* Config = Enemy->GetEnemyConfig();

	if (!IsValid(Config) || !IsValid(Config->BehaviorTree))
		return;

	UBehaviorTree* Tree = Config->BehaviorTree;
	
	/* Blackboard 준비 */
	UBlackboardComponent* BlackboardComp = nullptr;
	if (!UseBlackboard(Tree->BlackboardAsset, BlackboardComp))
		return;

	/* 초기 값 설정 */
	BlackboardComp->SetValueAsVector(EnemyBlackboardKeys::HomeLocation, Enemy->GetActorLocation());

	BlackboardComp->SetValueAsBool(EnemyBlackboardKeys::CanAct, true);

	/* BT 실행 */
	if (!RunBehaviorTree(Tree))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Failed to run BehaviorTree"), *GetName());
	}
}

void AEnemyAIController::SetCombatTarget(AActor * NewTarget)
{
	// 타깃 유효성 검사
	if (!IsValid(NewTarget))
	{
		ClearCombatTarget();
		return;
	}

	// 자기 자신을 타깃으로 지정하지 않음
	if (NewTarget == GetPawn())
		return;

	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!IsValid(BlackboardComp))
		return;
	
	BlackboardComp->SetValueAsObject(EnemyBlackboardKeys::TargetActor, NewTarget);
}

void AEnemyAIController::ClearCombatTarget()
{
	if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
	{
		BlackboardComp->ClearValue(EnemyBlackboardKeys::TargetActor);
	}

	ClearFocus(EAIFocusPriority::Gameplay);
}
