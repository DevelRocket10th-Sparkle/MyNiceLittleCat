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

protected :

	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory
	) override;

	virtual void TickTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		float DeltaSeconds
	) override;

	virtual void OnTaskFinished(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		EBTNodeResult::Type TaskResult
	) override;

private :

	void HandleAbilityEnded(const FAbilityEndedData& EndedData);
	
	void Cleanup(bool bCancelAttack);

private :

	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	FGameplayAbilitySpecHandle AttackHandle;

	FDelegateHandle AbilityEndedDelegateHandle;
	
	bool bOwnsAttack = false;
	bool bAttackEnded = false;
	bool bAttackCancelled = false;

};
