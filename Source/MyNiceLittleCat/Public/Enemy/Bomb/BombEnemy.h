// 폭탄 몬스터
// - 자성(Magnetic 컴포넌트 Magnet Enabled = true) / 절연체(false) 선택
// - 플레이어가 TriggerRadius 안에 들어오면 GA_SelfDestruct 점화
// - 펫이 잡으려 하면(자성일 때) 즉시 점화 → 들고 있다가 던질 수 있음
// - 던져진 상태에서 적에게 부딪히면 즉시 폭발
// - 예약 폭발 전에 죽으면 GA의 bExplodeOnDeath에 따라 폭발/불발
#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyCharacter.h"
#include "BombEnemy.generated.h"

class UGA_SelfDestruct;
class UMagneticComponent;
class APetCharacter;

UCLASS()
class MYNICELITTLECAT_API ABombEnemy : public AEnemyCharacter
{
	GENERATED_BODY()

public:
	ABombEnemy(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void Landed(const FHitResult& Hit) override;

	// GA_SelfDestruct 점화 (이미 점화/폭발했으면 무시)
	void TriggerSelfDestruct();

	// 퓨즈 무시하고 지금 폭발
	void DetonateNow();

	bool IsFuseLit() const { return ActiveSelfDestruct.IsValid(); }
	bool HasExploded() const { return bExploded; }
	bool IsGrabbed() const { return bGrabbed; }

	/* GA_SelfDestruct 전용 */
	void SetActiveSelfDestruct(UGA_SelfDestruct* Ability);
	void MarkExploded();
	void HandleExploded();

protected:
	virtual void BeginPlay() override;
	virtual void BeginDeath() override;

	UFUNCTION()
	void HandleCapsuleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	bool IsBeingGrabbed() const;
	void SetGrabbed(bool bNewGrabbed);
	bool IsValidThrowVictim(const AActor* Actor) const;
	void CheckThrownContact();

	// 끄면(Magnet Enabled = false) 절연체 폭탄: 펫이 못 잡음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bomb")
	TObjectPtr<UMagneticComponent> Magnetic;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bomb")
	TSubclassOf<UGA_SelfDestruct> SelfDestructAbility;

	// 플레이어가 이 거리(수평) 안에 들어오면 점화. 0이면 근접 점화 안 함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb", meta = (ClampMin = "0.0"))
	float TriggerRadius = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb", meta = (ClampMin = "0.0"))
	float TriggerHeightTolerance = 150.f;

	// 놓인(던져진) 뒤 이 시간 동안은 적과 닿으면 바로 폭발
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb", meta = (ClampMin = "0.0", Units = "s"))
	float ThrownWindowSeconds = 2.f;

private:
	TWeakObjectPtr<UGA_SelfDestruct> ActiveSelfDestruct;
	TWeakObjectPtr<APetCharacter> CachedPet;

	bool bExploded = false;
	bool bGrabbed = false;
	bool bThrown = false;
	float ThrownUntil = 0.f;
};
