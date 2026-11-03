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

class AEnemyCharacter;
class UEnemyPatternComponent;
class AAIController;

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UBTTask_Enemy_ExecuteAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public :

	UBTTask_Enemy_ExecuteAttack();
	
protected :

	// 실행할 패턴을 확정하고, 필요하면 방향 회전 후 실행
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

	// 선택해 둔 패턴을 재추첨 없이 실행
	// - 실행 직전 조건은 컴포넌트가 재검사
	bool StartAttack();

public :

	// BT에서 직접 지정 (None이면 자동 추첨)
	UPROPERTY(EditAnywhere, Category = "Pattern")
	FName PatternId;

	// 타깃과의 수평 각도 차이가 이 값 이하면 공격 시작
	UPROPERTY(EditAnywhere, Category = "Facing", meta = (ClampMin = "0.1", Units = "deg"))
	float FacingTolerance = 5.f;

	// 이 시간 안에 방향 정렬을 완료하지 못하면 실패
	UPROPERTY(EditAnywhere, Category = "Facing", meta = (ClampMin = "0.1", Units = "s"))
	float FacingTimeout = 3.f;

private :

	// 실행을 요청한 패턴 컴포넌트
	TWeakObjectPtr<UEnemyPatternComponent> CachedPatternComp;
	// 이 Task가 소유한 공격 요청 번호
	int64 PatternRequestId = 0;

	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;	// 회전할 Enemy
	TWeakObjectPtr<AActor> CachedTarget;			// 회전 방향 목표 Target

	bool bWaitingForFacing = false;
	float FacingElapsed = 0.f;

	// 이번 실행에서 선택한 패턴 (회전 후에도 같은 패턴 실행)
	FName SelectedPatternId = NAME_None;

	// 이 Task가 설정한 공격 Focus를 해제할 컨트롤러
	TWeakObjectPtr<AAIController> CachedAI;

	// 이 Task가 AttackFacing Focus를 설정했는지 기록
	bool bOwnsAttackFacingFocus = false;

};
