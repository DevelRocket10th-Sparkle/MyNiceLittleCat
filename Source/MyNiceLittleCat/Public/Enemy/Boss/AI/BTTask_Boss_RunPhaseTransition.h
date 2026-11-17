// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.17. (BTTask_Enemy_TransitionPhase 대체)
* 보스 페이즈 전환 Task
* - BT는 "언제 전환할지(공격이 끝난 뒤)"만 결정하고, 전환 연출과 다음 페이즈 적용은 보스가 처리
* - 보스의 전환 결과 이벤트를 받아 Task를 끝냄 (매 Tick 결과를 확인하지 않음)
*/

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_Boss_RunPhaseTransition.generated.h"

class ABossCharacter;
class UBehaviorTreeComponent;

UCLASS()
class MYNICELITTLECAT_API UBTTask_Boss_RunPhaseTransition : public UBTTaskNode
{
	GENERATED_BODY()

public :

	// 보스별 Task 인스턴스 사용 (이벤트 구독과 전환 번호를 인스턴스에 보관)
	UBTTask_Boss_RunPhaseTransition();

protected :

	// 기존 공격의 종료를 확인한 뒤 전환 시작: 연출이 없으면 즉시 완료될 수 있음
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// 브랜치 중단: 전투 중이면 실패 처리 (사망/초기화 중에는 재요청하지 않음)
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// Task 완료와 중단 모두 구독과 전환 번호 정리
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

private :

	// 보스의 전환 결과 수신
	void HandleTransitionResult(int64 InTransitionId, bool bSucceeded);

	// 구독 해제와 참조 초기화
	void Cleanup();

private :

	// 전환 중인 보스
	TWeakObjectPtr<ABossCharacter> Boss;

	// Task를 끝낼 BT 컴포넌트
	TWeakObjectPtr<UBehaviorTreeComponent> OwnerTree;

	// 보스 결과 이벤트 구독 핸들
	FDelegateHandle ResultHandle;

	// 이 Task가 기다리는 전환 번호
	int64 TransitionId = 0;

	// ExecuteTask 안에서 결과가 바로 온 경우를 구분 (FinishLatentTask 대신 반환값으로 처리)
	bool bExecuting = false;

	// ExecuteTask 안에서 받은 결과
	TOptional<bool> ImmediateResult;
};
