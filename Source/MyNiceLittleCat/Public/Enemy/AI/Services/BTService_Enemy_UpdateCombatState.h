// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.16.
* 수정 일자: 2026.11.16. (Blackboard 키를 문자열 대신 Selector로 지정)
* Enemy 거리, 시야, 공격 준비 상태를 검사해 Blackboard 판단 값 갱신
* - 입력: 타깃 키 / 출력: 사거리, 시야, 공격 시작 가능 키
* - 모든 키는 BT 에디터에서 선택 (기본값은 기존 키 이름 → 기존 BT 에셋은 다시 설정하지 않아도 동작)
*/

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_Enemy_UpdateCombatState.generated.h"

UCLASS()
class MYNICELITTLECAT_API UBTService_Enemy_UpdateCombatState : public UBTService
{
	GENERATED_BODY()

public :

	// 갱신 주기와 키 타입 필터 설정
	UBTService_Enemy_UpdateCombatState();

protected :

	// 키 이름 → 키 ID 변환 (에셋 로드 시 한 번)
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

	// 자식 브랜치의 조건을 검사하기 전에 최초 갱신
	virtual void OnSearchStart(FBehaviorTreeSearchData& SearchData) override;

	// 활성 상태에서 일정 간격으로 갱신
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	// 에디터 노드에 사용 중인 키 표시
	virtual FString GetStaticDescription() const override;

private :

	// 판단 값 계산 후 Blackboard 기록
	void UpdateCombatState(UBehaviorTreeComponent& OwnerComp) const;

protected :

	// 입력: 전투 타깃 (Object, AActor)
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetKey;

	// 출력: 거리/높이 조건을 만족하는 패턴이 있는지 (Bool)
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector IsInAttackRangeKey;

	// 출력: 타깃까지 시야 확보 (Bool)
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector HasLineOfSightKey;

	// 출력(선택): 지금 실행 가능한 자동 선택 패턴이 있는지 (Bool), BB에 키가 없으면 기록하지 않음
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector CanStartAttackKey;
};
