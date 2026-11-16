// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.15.
* 수정 일자: 2026.11.16. (출력 키를 문자열 대신 Selector로 지정)
* Enemy가 복귀 위치에서 벗어났는지 Blackboard에 기록
* - 입력: BlackboardKey (복귀 위치, Vector) / 출력: IsAwayFromHomeKey (Bool)
*/

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BTService_Enemy_UpdateHomeState.generated.h"

UCLASS()
class MYNICELITTLECAT_API UBTService_Enemy_UpdateHomeState : public UBTService_BlackboardBase
{
	GENERATED_BODY()

public :

	// 갱신 주기와 키 타입 필터 설정
	UBTService_Enemy_UpdateHomeState();

protected :

	// 출력 키 이름 → 키 ID 변환
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

	// 자식 브랜치의 조건을 검사하기 전에 최초 갱신
	virtual void OnSearchStart(FBehaviorTreeSearchData& SearchData) override;

	// 활성 상태에서 일정 간격으로 갱신
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private :

	// 복귀 위치와의 거리 계산 후 기록
	void UpdateHomeState(UBehaviorTreeComponent& OwnerComp) const;

protected :

	// 출력: 복귀 위치에서 HomeArrivalRadius보다 멀리 있는지 (Bool)
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector IsAwayFromHomeKey;
};
