// 보스 패턴: 플레이어의 예측 위치 앞쪽으로 폭탄 몬스터를 하늘에서 떨어뜨립니다.
// 떨어진 폭탄은 스스로 플레이어를 쫓아가 자폭합니다 (패턴이 정리하지 않음).
#pragma once

#include "CoreMinimal.h"
#include "Enemy/GAS/Abilities/EnemyPatternAbility.h"
#include "GA_Enemy_BombDrop.generated.h"

class AEnemyCharacter;
class ABombEnemy;
class UBombDropPatternSettings;

UCLASS()
class MYNICELITTLECAT_API UGA_Enemy_BombDrop : public UEnemyPatternAbility
{
	GENERATED_BODY()
protected:
	// 설정 값, 타깃, 실행 제한 시간 검사 (CommitAbility 전, 실패하면 부모가 취소 종료)
	virtual bool CanStartPattern() const override;
	// 폭탄 수 확인, 회전 잠금, 취소 감시 타이머, 첫 폭탄 낙하
	virtual bool StartPattern() override;
	// 타이머와 실행 참조 정리 (잠금 해제와 요청 정리는 부모가 처리)
	virtual void StopPattern(bool bWasCancelled) override;

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

	// 부모 ExecutionTarget의 약한 참조 사본: 기존 조준·취소 코드가 그대로 사용
	TWeakObjectPtr<AActor> Target;

	FTimerHandle StepTimer;
	FTimerHandle CancelCheckTimer;
	int32 Dropped = 0;
	bool bCleanupBound = false;
};
