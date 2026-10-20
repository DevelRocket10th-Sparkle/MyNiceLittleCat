// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.16.
* Enemy 거리, 시야 등을 검사해 판단 정보 갱신
*/

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_Enemy_UpdateCombatState.generated.h"

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UBTService_Enemy_UpdateCombatState : public UBTService
{
	GENERATED_BODY()

public :

	UBTService_Enemy_UpdateCombatState();

protected :

	// 자식 브랜치의 조건을 검사하기 전에 최초 갱신
	virtual void OnSearchStart(FBehaviorTreeSearchData& SearchData) override;

	// 활성 상태에서 일정 간격으로 갱신
	virtual void TickNode(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		float DeltaSeconds
	) override;

private :

	void UpdateCombatState(UBehaviorTreeComponent& OwnerComp);
	
};
