// 보스 패턴: 플레이어 주위 원형으로 포탑을 하늘에서 떨어뜨리고, 일정 시간 뒤 모두 파괴한 다음 반복합니다.
// 다른 패턴과 같이 EnemyPatternComponent가 패턴 정의(UTurretRainPatternSettings)를 통해 실행합니다.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Enemy_TurretRain.generated.h"

class AEnemyCharacter;
class ATurret;
class UEnemyPatternComponent;
class UTurretRainPatternSettings;

UCLASS()
class MYNICELITTLECAT_API UGA_Enemy_TurretRain : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UGA_Enemy_TurretRain();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void StartCycle();
	void DropNext();
	void DestroyTurrets();
	void DestroyBackgroundTurrets();
	void DestroyWithFX(ATurret* Turret) const;
	void CheckCancel();
	bool FindGround(const FVector& Around, FVector& OutGround) const;

	UPROPERTY(Transient)
	TObjectPtr<UTurretRainPatternSettings> Settings;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ATurret>> Turrets;

	// 백그라운드 모드에서 패턴 종료 후에도 남아 있는 포탑 (어빌리티 인스턴스는 액터당 하나라 다음 실행까지 유지)
	UPROPERTY(Transient)
	TArray<TObjectPtr<ATurret>> BackgroundTurrets;

	// 백그라운드 포탑 파괴용 FX 설정 (패턴이 끝나 Settings가 비워져도 사용)
	UPROPERTY(Transient)
	TObjectPtr<UTurretRainPatternSettings> BackgroundSettings;

	FTimerHandle BackgroundDestroyTimer;

	TWeakObjectPtr<AEnemyCharacter> Enemy;
	TWeakObjectPtr<UEnemyPatternComponent> PatternComponent;
	TWeakObjectPtr<AActor> Target;

	FTimerHandle StepTimer;
	FTimerHandle CancelCheckTimer;
	FVector CycleCenter = FVector::ZeroVector;
	float AngleOffset = 0.f;
	int32 DroppedThisCycle = 0;
	int32 CyclesDone = 0;
	int64 RequestId = 0;
	bool bOwnsRotationLock = false;
};
