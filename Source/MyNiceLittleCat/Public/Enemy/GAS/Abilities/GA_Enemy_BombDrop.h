// 보스 패턴: 플레이어의 예측 위치 앞쪽으로 폭탄 몬스터를 하늘에서 떨어뜨립니다.
// 떨어진 폭탄은 스스로 플레이어를 쫓아가 자폭합니다 (패턴이 정리하지 않음).
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Enemy_BombDrop.generated.h"

class AEnemyCharacter;
class ABombEnemy;
class UEnemyPatternComponent;
class UBombDropPatternSettings;

UCLASS()
class MYNICELITTLECAT_API UGA_Enemy_BombDrop : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UGA_Enemy_BombDrop();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void DropNext();
	void CheckCancel();
	FVector ComputeAimPoint(bool bScatter) const;
	bool FindGround(const FVector& Around, FVector& OutGround) const;
	int32 CountAliveBombs();

	// 보스가 죽거나(사망 시작) 사라지면(초기화·플레이어 사망 후 재생성) 떨군 폭탄을 모두 제거
	void BindOwnerCleanup(AEnemyCharacter* OwnerEnemy);
	void HandleOwnerDeathStarted(AEnemyCharacter* DeadEnemy);
	UFUNCTION()
	void HandleOwnerDestroyed(AActor* DestroyedActor);
	void KillSpawnedBombs();

	UPROPERTY(Transient)
	TObjectPtr<UBombDropPatternSettings> Settings;

	// 이 보스가 떨군 폭탄 (MaxAliveBombs 제한용). 인스턴스가 액터당 하나라 패턴이 끝나도 유지됨
	TArray<TWeakObjectPtr<ABombEnemy>> AliveBombs;

	TWeakObjectPtr<AEnemyCharacter> Enemy;
	TWeakObjectPtr<UEnemyPatternComponent> PatternComponent;
	TWeakObjectPtr<AActor> Target;

	FTimerHandle StepTimer;
	FTimerHandle CancelCheckTimer;
	int32 Dropped = 0;
	int64 RequestId = 0;
	bool bOwnsRotationLock = false;
	bool bCleanupBound = false;
};
