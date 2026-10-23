// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.16.
* 공격 요청 후 해당 공격의 종료 대기
*/

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"

#include "BTTask_Enemy_ExecuteAttack.generated.h"

class UEnemyPatternComponent;

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UBTTask_Enemy_ExecuteAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public :

	UBTTask_Enemy_ExecuteAttack();

	// 빈 값이면 가중치 추첨, 지정하면 해당 패턴을 직접 요청
	UPROPERTY(EditAnywhere, Category = "Pattern")
	FName PatternId;

protected :

	// Task 시작 시 행동을 요청하고 비동기 완료까지 InProgress 반환
	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory
	) override;

	// 요청의 종료 결과를 확인하여 BT Task 완료
	virtual void TickTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		float DeltaSeconds
	) override;

	// Task 완료 및 중단 모두 실행 참조와 요청 번호 정리
	virtual void OnTaskFinished(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		EBTNodeResult::Type TaskResult
	) override;

	// 브랜치 중단 시 이 Task가 시작한 요청만 취소
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private :

	// 이 Task가 소유한 패턴 요청 해제
	void Cleanup();

private :

	// 실행을 요청한 패턴 컴포넌트
	TWeakObjectPtr<UEnemyPatternComponent> CachedPattern;
	// 이 Task가 소유한 공격 요청 번호
	int64 PatternRequestId = 0;

};
