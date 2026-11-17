// 보스 패턴: 플레이어 주위 원형으로 포탑을 하늘에서 떨어뜨리고, 일정 시간 뒤 모두 파괴한 다음 반복합니다.
// 다른 패턴과 같이 EnemyPatternComponent가 패턴 정의(UTurretRainPatternSettings)를 통해 실행합니다.
#pragma once

#include "CoreMinimal.h"
#include "Enemy/GAS/Abilities/EnemyPatternAbility.h"
#include "GA_Enemy_TurretRain.generated.h"

class AEnemyCharacter;
class ATurret;
class UTurretRainPatternSettings;

UCLASS()
class MYNICELITTLECAT_API UGA_Enemy_TurretRain : public UEnemyPatternAbility
{
	GENERATED_BODY()
protected:
	// 설정 값, 타깃, 실행 제한 시간 검사 (CommitAbility 전, 실패하면 부모가 취소 종료)
	virtual bool CanStartPattern() const override;
	// 백그라운드 포탑 확인, 회전 잠금, 정리 바인딩, 취소 감시 타이머, 첫 주기 시작
	virtual bool StartPattern() override;
	// 타이머와 남은 포탑, 실행 참조 정리 (잠금 해제와 요청 정리는 부모가 처리)
	virtual void StopPattern(bool bWasCancelled) override;

private:
	void StartCycle();
	void DropNext();
	void DestroyTurrets();
	void DestroyBackgroundTurrets();
	void DestroyWithFX(ATurret* Turret) const;
	void CheckCancel();
	bool FindGround(const FVector& Around, FVector& OutGround) const;

	// 보스가 죽거나(사망 시작) 사라지면(초기화·플레이어 사망 후 재생성) 떨군 포탑을 모두 파괴
	void BindOwnerCleanup(AEnemyCharacter* OwnerEnemy);
	void HandleOwnerDeathStarted(AEnemyCharacter* DeadEnemy);
	UFUNCTION()
	void HandleOwnerDestroyed(AActor* DestroyedActor);
	void KillSpawnedTurrets();

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

	// 부모 ExecutionTarget의 약한 참조 사본: 기존 취소 확인·바닥 검사가 그대로 사용
	TWeakObjectPtr<AActor> Target;

	FTimerHandle StepTimer;
	FTimerHandle CancelCheckTimer;
	FVector CycleCenter = FVector::ZeroVector;
	float AngleOffset = 0.f;
	int32 DroppedThisCycle = 0;
	int32 CyclesDone = 0;
	bool bCleanupBound = false;
};
