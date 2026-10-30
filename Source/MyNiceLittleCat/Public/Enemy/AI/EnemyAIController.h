// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.16.
* 잡몹/보스의 AI Controller
* 공통 행동 구현
* - BT 시작/종료, 타깃 설정/해제
*/

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
struct FAIStimulus;

// Focus 우선순위: 숫자가 클수록 우선순위가 높음
namespace EnemyFocusPriority
{
	// 기존 Gameplay Foucs보다 높은 페이즈 기본 방향
	inline const EAIFocusPriority::Type PhaseHold =
		static_cast<EAIFocusPriority::Type>(EAIFocusPriority::Gameplay + 1);

	// 사전 회전이 필요한 공격에서 설정
	// - 공격, 후딜 종료 또는 Task 중단 시 해제
	inline const EAIFocusPriority::Type AttackFacing =
		static_cast<EAIFocusPriority::Type>(EAIFocusPriority::Gameplay + 2);
}

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:

	AEnemyAIController();

	// Enemy 초기화와 빙의가 모두 완료되면 BT 시작
	void TryStartEnemyAI();

	// 인지 시스템이나 전투 시작 이벤트에서 호출
	UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
	void SetCombatTarget(AActor* NewTarget);

	// 인지 시스템이나 전투 종료 이벤트에서 호출
	UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
	void ClearCombatTarget();

	// 페이즈 기준 방향을 저장하고, Hold Focus 설정
	void SetPhaseHoldYaw(float Yaw);

	// 페이즈 Hold Focus 해제
	void ClearPhaseHold();

protected :

	virtual void Tick(float DeltaSeconds) override;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
	
protected :

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAIPerceptionComponent> EnemyPerception;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

private :

	// 페이즈 시작 방향을 바라보는 Focus 사용 여부
	bool bPhaseHoldActive = false;

	// 월드 기준으로 유지할 수평 방향
	FVector PhaseHoldDirection = FVector::ForwardVector;

};
