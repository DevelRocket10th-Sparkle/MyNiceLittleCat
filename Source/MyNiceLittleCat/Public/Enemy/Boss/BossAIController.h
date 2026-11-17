// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.17.
* 보스 전용 AI Controller
* - 보스 Blackboard 키(CanAct, PhaseTransitionRequested) 기록을 전담: 보스 캐릭터는 Blackboard를 모름
* - 페이즈 기준 방향 유지(Hold Focus), 전투 정지/전환 준비 시 AI 정리
* - 타깃은 보스방 매니저가 지정하므로 시야 인지는 사용하지 않음
*/

#pragma once

#include "CoreMinimal.h"
#include "Enemy/AI/EnemyAIController.h"
#include "BossAIController.generated.h"

class ABossCharacter;

// 페이즈 기준 방향 Focus 우선순위: Gameplay(플레이어) < PhaseHold < AttackFacing
namespace BossFocusPriority
{
	inline const EAIFocusPriority::Type PhaseHold =
		static_cast<EAIFocusPriority::Type>(EAIFocusPriority::Gameplay + 1);
}

UCLASS()
class MYNICELITTLECAT_API ABossAIController : public AEnemyAIController
{
	GENERATED_BODY()

public :

	/* 보스 캐릭터가 호출하는 명령 */

	// 전투 정지: BT 정지, 이동/Focus/타깃 정리
	void StopBossAI(const FString& Reason);

	// 전환 요청 직후: 이동 정지, 이전 페이즈 Hold와 공격 정렬 Focus 해제 (플레이어 Focus는 유지)
	void PrepareForPhaseTransition();

	// 페이즈 시작 시 기준 방향 설정: bHold면 현재 방향 유지, 아니면 해제
	void ApplyPhaseRotation(bool bHold, float HoldYaw);

	// 보스 상태를 Blackboard 판단 키에 반영
	void RefreshBossBlackboard();

protected :

	// Hold 방향의 가상 목표점을 부모 Tick의 회전 계산 전에 갱신
	virtual void Tick(float DeltaSeconds) override;

	// 빙의 해제 시 Hold 정리
	virtual void OnUnPossess() override;

	// 보스 타깃은 매니저가 지정
	virtual bool ShouldTrackPerceivedTargets() const override { return false; }

	// 공통 키 + 보스 필수 키(CanAct, PhaseTransitionRequested) 검증
	virtual bool CacheBlackboardKeys(const UBlackboardComponent& BlackboardComponent) override;

	// BT 시작 직후 현재 보스 상태 반영
	virtual void OnBehaviorTreeStarted() override;

private :

	// 현재 빙의한 보스
	ABossCharacter* GetBoss() const;

	// 기준 방향 저장 후 즉시 Focus 설정
	void SetPhaseHoldYaw(float Yaw);

	// 기준 방향 Focus 해제
	void ClearPhaseHold();

private :

	// 전환 요청 키 ID
	FBlackboard::FKey PhaseTransitionRequestedKey = FBlackboard::InvalidKey;

	// 페이즈 시작 방향을 바라보는 Focus 사용 여부
	bool bPhaseHoldActive = false;

	// 월드 기준으로 유지할 수평 방향
	FVector PhaseHoldDirection = FVector::ForwardVector;
};
