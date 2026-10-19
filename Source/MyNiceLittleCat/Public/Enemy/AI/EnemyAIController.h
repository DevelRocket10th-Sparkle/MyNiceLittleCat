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

};
