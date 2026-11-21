// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.16.
* 수정 일자: 2026.11.17. (보스 전용 처리를 ABossAIController로 분리, Blackboard 키 검증 추가)
* 수정 일자: 2026.11.20. (타깃 키 찾기 도우미)
* 잡몹/보스 공통 AI Controller
* - BT 시작/종료, 타깃 설정/해제, 시야 인지
* - 파생 컨트롤러는 가상 함수로 인지 사용 여부, 추가 필수 키, BT 시작 후 처리를 확장
*/

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "EnemyAIController.generated.h"

class UAIPerceptionComponent;
class UBlackboardComponent;
class UAISenseConfig_Sight;
class UBlackboardKeyType;
struct FAIStimulus;

// Focus 우선순위: 숫자가 클수록 우선순위가 높음
// - Gameplay + 1은 ABossAIController의 PhaseHold가 사용
namespace EnemyFocusPriority
{
	// 사전 회전이 필요한 공격에서 설정: 공격, 후딜 종료 또는 Task 중단 시 해제
	inline const EAIFocusPriority::Type AttackFacing =
		static_cast<EAIFocusPriority::Type>(EAIFocusPriority::Gameplay + 2);
}

UCLASS()
class MYNICELITTLECAT_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public :

	// 시야 인지 구성, BT 자동 시작 비활성화
	AEnemyAIController();

	// Enemy 초기화와 빙의가 모두 완료되면 Blackboard 준비 후 BT 시작
	void TryStartEnemyAI();

	// 전투 타깃 지정: 인지 시스템, 보스방 매니저, 튜토리얼 등에서 호출
	UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
	void SetCombatTarget(AActor* NewTarget);

	// 전투 타깃 해제
	UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
	void ClearCombatTarget();

	// Getter: 현재 전투 타깃 (Blackboard TargetActor, 없으면 nullptr): 상시 패턴 등 BT 밖의 실행에서 사용
	UFUNCTION(BlueprintPure, Category = "Enemy|AI")
	AActor* GetCombatTarget() const;

protected :

	// 이동 의도(애니메이션용) 갱신
	virtual void Tick(float DeltaSeconds) override;

	// 빙의 직후 AI 시작 시도
	virtual void OnPossess(APawn* InPawn) override;

	// 빙의 해제 시 판단 로직, 이동, 타깃, Focus 정리
	virtual void OnUnPossess() override;

	/* 파생 컨트롤러 확장 지점 */

	// 시야 인지로 타깃을 잡을지 여부: 보스처럼 외부(매니저)가 타깃을 정하면 false
	virtual bool ShouldTrackPerceivedTargets() const { return true; }

	// Blackboard 키 ID 캐싱 및 필수 키 검증: 필수 키가 없거나 타입이 다르면 false → BT 시작 안 함
	// - 파생 클래스는 Super 호출 후 자기 키를 추가로 검증
	virtual bool CacheBlackboardKeys(const UBlackboardComponent& BlackboardComponent);

	// BT 실행 직후 호출: 파생 상태를 Blackboard에 반영
	virtual void OnBehaviorTreeStarted() {}

	/* Blackboard 도우미 */

	// 키를 찾아 ID를 반환: 없거나 타입이 다르면 InvalidKey (bRequired면 Error 로그)
	static FBlackboard::FKey FindKey(const UBlackboardComponent& Blackboard, FName KeyName,
		TSubclassOf<UBlackboardKeyType> ExpectedType, bool bRequired);

	// 캐싱한 키 ID로 Bool 값 기록: 키가 없으면 무시
	void SetBlackboardBool(FBlackboard::FKey KeyId, bool bValue);

	// 타깃 키 ID: BT 시작 전(튜토리얼처럼 빙의 직후 지정)이라 캐시가 없으면 이름으로 찾음
	FBlackboard::FKey ResolveTargetActorKey(const UBlackboardComponent& BlackboardComp) const;

	// 시야 인지 갱신: 플레이어 발견 시 타깃 지정, 놓치면 해제
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

protected :

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAIPerceptionComponent> EnemyPerception;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	/* 캐싱한 Blackboard 키 ID: BT 시작 시 한 번 찾아 두고 FName 검색 없이 사용 */

	FBlackboard::FKey TargetActorKey = FBlackboard::InvalidKey;	// 필수
	FBlackboard::FKey HomeLocationKey = FBlackboard::InvalidKey;	// 선택
	FBlackboard::FKey CanActKey = FBlackboard::InvalidKey;			// 선택 (보스는 필수)
};
