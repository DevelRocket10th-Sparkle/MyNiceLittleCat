// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BTService_Enemy_UpdateHomeState.generated.h"

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UBTService_Enemy_UpdateHomeState : public UBTService_BlackboardBase
{
	GENERATED_BODY()

public :

	UBTService_Enemy_UpdateHomeState();

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

	void UpdateHomeState(UBehaviorTreeComponent& OwnerComp);
	
};
