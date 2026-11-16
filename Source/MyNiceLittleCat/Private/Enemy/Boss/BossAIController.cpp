// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Boss/BossAIController.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BrainComponent.h"

#include "Enemy/AI/EnemyBlackboardKeys.h"
#include "Enemy/Boss/BossCharacter.h"

namespace
{
	// 방향을 유지하기 위해 시선 앞에 두는 가상 목표점까지의 거리 (이동 거리, 도착 반경 아님)
	constexpr float HoldFocusDistance = 1000.f;
}

/* 보스 캐릭터가 호출하는 명령 */

void ABossAIController::StopBossAI(const FString& Reason)
{
	if (UBrainComponent* Brain = GetBrainComponent())
	{
		Brain->StopLogic(Reason);
	}

	StopMovement();
	ClearPhaseHold();
	ClearFocus(EnemyFocusPriority::AttackFacing);
	ClearCombatTarget();
}

void ABossAIController::PrepareForPhaseTransition()
{
	StopMovement();

	// 이전 페이즈의 Hold Focus와 공격 정렬 Focus 해제, 기본 플레이어 Gameplay Focus는 유지
	ClearPhaseHold();
	ClearFocus(EnemyFocusPriority::AttackFacing);
}

void ABossAIController::ApplyPhaseRotation(bool bHold, float HoldYaw)
{
	if (bHold)
	{
		SetPhaseHoldYaw(HoldYaw);
	}
	else
	{
		ClearPhaseHold();
	}
}

void ABossAIController::RefreshBossBlackboard()
{
	const ABossCharacter* Boss = GetBoss();
	if (!Boss)
	{
		return;
	}

	SetBlackboardBool(CanActKey, Boss->CanBossAct());
	SetBlackboardBool(PhaseTransitionRequestedKey, Boss->IsPhaseTransitionInProgress());
}

/* 확장 지점 */

void ABossAIController::Tick(float DeltaSeconds)
{
	// 부모 Tick에서 회전을 계산하기 전에 Focus 위치 갱신: 보스가 이동해도 같은 월드 방향을 바라봄
	if (bPhaseHoldActive)
	{
		if (const APawn* ControlledPawn = GetPawn())
		{
			SetFocalPoint(ControlledPawn->GetPawnViewLocation() + PhaseHoldDirection * HoldFocusDistance,
				BossFocusPriority::PhaseHold);
		}
	}

	Super::Tick(DeltaSeconds);
}

void ABossAIController::OnUnPossess()
{
	ClearPhaseHold();
	Super::OnUnPossess();
}

bool ABossAIController::CacheBlackboardKeys(const UBlackboardComponent& BlackboardComponent)
{
	const bool bCommonKeysValid = Super::CacheBlackboardKeys(BlackboardComponent);

	// 보스 BT는 행동 가능 여부와 전환 요청 키로 분기하므로 둘 다 필수
	CanActKey = FindKey(BlackboardComponent, EnemyBlackboardKeys::CanAct, UBlackboardKeyType_Bool::StaticClass(), true);
	PhaseTransitionRequestedKey = FindKey(BlackboardComponent, EnemyBlackboardKeys::PhaseTransitionRequested,
		UBlackboardKeyType_Bool::StaticClass(), true);

	return bCommonKeysValid
		&& CanActKey != FBlackboard::InvalidKey
		&& PhaseTransitionRequestedKey != FBlackboard::InvalidKey;
}

void ABossAIController::OnBehaviorTreeStarted()
{
	// 공통 초기값(CanAct = true)을 실제 보스 상태로 덮어씀
	RefreshBossBlackboard();
}

/* 내부 처리 */

ABossCharacter* ABossAIController::GetBoss() const
{
	return Cast<ABossCharacter>(GetPawn());
}

void ABossAIController::SetPhaseHoldYaw(float Yaw)
{
	PhaseHoldDirection = FRotator(0.f, Yaw, 0.f).Vector();
	bPhaseHoldActive = true;

	// 다음 Tick을 기다리지 않고 즉시 Focus 설정
	if (const APawn* ControlledPawn = GetPawn())
	{
		SetFocalPoint(ControlledPawn->GetPawnViewLocation() + PhaseHoldDirection * HoldFocusDistance,
			BossFocusPriority::PhaseHold);
	}
}

void ABossAIController::ClearPhaseHold()
{
	bPhaseHoldActive = false;
	ClearFocus(BossFocusPriority::PhaseHold);
}
