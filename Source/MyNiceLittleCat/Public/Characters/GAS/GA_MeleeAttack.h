// 작성자: 강진구
// 작성일자: 2026-10-15
// 
// 플레이어 근접공격 GAS 어빌리티 입니다.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_MeleeAttack.generated.h"

class UAnimMontage;
/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UGA_MeleeAttack : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_MeleeAttack();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;


protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> AttackMontage;
};
