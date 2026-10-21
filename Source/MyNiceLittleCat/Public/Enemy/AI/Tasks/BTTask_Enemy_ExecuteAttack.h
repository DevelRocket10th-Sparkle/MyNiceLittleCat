// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.16.
* 공격 요청 후 해당 공격의 종료 대기
*/

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"

#include "GameplayAbilitySpec.h"

#include "BTTask_Enemy_ExecuteAttack.generated.h"

class UAbilitySystemComponent;
class UEnemyPatternComponent;
struct FAbilityEndedData;

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

	// 브랜치 중단 시 이 Task가 요청한 공격만 취소
	// 브랜치 중단 시 이 Task가 시작한 요청만 취소
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private :

	// 현재 능력의 종료 이벤트만 받아 완료 또는 취소 기록
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);
	
	// 이 Task가 소유한 공격 및 이벤트 구독 정리
	void Cleanup(bool bCancelAttack);

private :

	// 기존 테스트 공격 경로의 ASC 약한 참조
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;
	// 새 패턴 경로에서 실행을 요청한 컴포넌트
	TWeakObjectPtr<UEnemyPatternComponent> CachedPattern;
	// 이 Task가 소유한 공격 요청 번호
	int64 PatternRequestId = 0;

	// 기존 테스트 경로에서 이 Task가 시작한 GA 핸들
	FGameplayAbilitySpecHandle AttackHandle;

	// ASC 종료 이벤트 구독을 해제할 핸들
	FDelegateHandle AbilityEndedDelegateHandle;
	
	// 기존 테스트 경로에서 이 Task가 공격을 시작했는지 여부
	bool bOwnsAttack = false;
	// 기존 테스트 공격의 종료 수신 여부
	bool bAttackEnded = false;
	// 기존 테스트 공격의 취소 여부
	bool bAttackCancelled = false;

};
