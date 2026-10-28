#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Enemy_MoneyGun.generated.h"

class AEnemyCharacter;
class AMoneyGun;
class UEnemyPatternComponent;
class UMoneyGunPatternSettings;
class USceneComponent;

// 다른 공격과 동일하게 패턴 컴포넌트가 패턴 정의를 통해 실행한다.
UCLASS()
class MYNICELITTLECAT_API UGA_Enemy_MoneyGun : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UGA_Enemy_MoneyGun();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	USceneComponent* FindAnchor(FName Name) const;
	bool SpawnGun(USceneComponent* Anchor);
	void UpdatePattern();
	UPROPERTY(Transient)
	TObjectPtr<UMoneyGunPatternSettings> Settings;
	UPROPERTY(Transient)
	TArray<TObjectPtr<AMoneyGun>> Guns;
	TWeakObjectPtr<AEnemyCharacter> Enemy;
	TWeakObjectPtr<UEnemyPatternComponent> PatternComponent;
	TWeakObjectPtr<AActor> Target;
	FTimerHandle UpdateTimer;
	int64 RequestId = 0;
	float Damage = 0.f;
	bool bOwnsRotationLock = false;
};
