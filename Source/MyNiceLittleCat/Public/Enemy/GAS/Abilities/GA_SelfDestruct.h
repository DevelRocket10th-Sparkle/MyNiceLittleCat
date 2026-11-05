// 폭탄 몬스터 자폭 GA
// - 퓨즈(카운트다운) 동안 메시 변형 + 오버레이 머티리얼 점멸 + 퓨즈 VFX/SFX
// - 시간이 다 되면 폭발: 범위 대미지(플레이어/적), 넉백, 물리 오브젝트 밀어내기, 폭발 VFX/SFX/카메라 셰이크
// - Explode()는 const라 CDO에서도 호출 가능 (사망 즉시 폭발 등 GA가 안 켜진 상태에서 사용)
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_SelfDestruct.generated.h"

class ABombEnemy;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class UNiagaraComponent;
class USoundBase;
class UAudioComponent;
class UGameplayEffect;
class UCameraShakeBase;

UCLASS()
class MYNICELITTLECAT_API UGA_SelfDestruct : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_SelfDestruct();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// 퓨즈를 건너뛰고 즉시 폭발 (던져서 적에 맞았을 때 등)
	void DetonateNow();

	// 실제 폭발 처리. GA 인스턴스가 없어도(CDO) 호출 가능
	void Explode(ABombEnemy* Bomb) const;

	bool ShouldExplodeOnDeath() const { return bExplodeOnDeath; }

protected:
	/* 공통 */

	// 예약된 폭발 전에 죽으면(체력 0) 그 자리에서 바로 폭발할지 여부. false면 그냥 사망(불발)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct")
	bool bExplodeOnDeath = true;

	// 퓨즈 점화 ~ 폭발까지 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct", meta = (ClampMin = "0.1", Units = "s"))
	float FuseSeconds = 2.f;

	/* 1. 메시 변형 */

	// 폭발 직전까지 부풀어 오르는 비율 (0.25 = 25% 커짐)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Deform", meta = (ClampMin = "0.0"))
	float InflateAmount = 0.25f;

	// 맥동(찌그러짐/늘어남) 세기
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Deform", meta = (ClampMin = "0.0"))
	float PulseAmount = 0.12f;

	// 맥동/점멸 속도 (Hz). 시작 → 폭발 직전으로 점점 빨라짐
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Deform", meta = (ClampMin = "0.1"))
	float PulseFrequencyStart = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Deform", meta = (ClampMin = "0.1"))
	float PulseFrequencyEnd = 12.f;

	/* 2. 오버레이 머티리얼 */

	// 메시 위에 덧씌울 점멸 머티리얼 (Translucent 권장). 비워두면 점멸 없음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Overlay")
	TObjectPtr<UMaterialInterface> OverlayMaterial;

	// 점멸 값(0~1)을 넣을 스칼라 파라미터 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Overlay")
	FName OverlayParameterName = TEXT("Flash");

	/* 3. VFX / SFX 슬롯 */

	// 퓨즈 타는 동안 붙어 있는 이펙트 (불꽃 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|FX")
	TObjectPtr<UNiagaraSystem> FuseEffect;

	// 퓨즈 이펙트/사운드를 붙일 소켓 또는 본 (슬라임 스켈레톤이면 Crown)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|FX")
	FName FuseAttachSocket = TEXT("Crown");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|FX")
	TObjectPtr<USoundBase> FuseSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|FX")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|FX")
	FVector ExplosionEffectScale = FVector(1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|FX")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|FX")
	TSubclassOf<UCameraShakeBase> ExplosionShake;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|FX", meta = (ClampMin = "0.0"))
	float ShakeInnerRadius = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|FX", meta = (ClampMin = "0.0"))
	float ShakeOuterRadius = 1500.f;

	/* 폭발 */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion", meta = (ClampMin = "0.0"))
	float Radius = 350.f;

	// 플레이어가 받는 대미지 (ApplyPlayerHit)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion", meta = (ClampMin = "0.0"))
	float PlayerDamage = 1.f;

	// 다른 적에게 주는 대미지 (SetByCaller Data.Damage, 중심에서 멀수록 최대 50%까지 감소)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion", meta = (ClampMin = "0.0"))
	float EnemyDamage = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion")
	TSubclassOf<UGameplayEffect> EnemyDamageEffect;

	// 적 넉백 (질량 100 기준, 무거울수록 덜 밀림)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion", meta = (ClampMin = "0.0"))
	float KnockbackStrength = 1200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion", meta = (ClampMin = "0.0"))
	float KnockbackUpward = 400.f;

	// 물리 오브젝트(상자, 포탑 등)에 주는 방사형 충격 (속도 변화량)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion", meta = (ClampMin = "0.0"))
	float PhysicsImpulse = 1500.f;

private:
	void TickFuse();
	void RestoreVisuals();

	TWeakObjectPtr<ABombEnemy> Bomb;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OverlayMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PreviousOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> FuseEffectComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> FuseSoundComponent;

	FTimerHandle FuseTickTimer;
	FTimerHandle DetonateTimer;

	FVector BaseMeshScale = FVector(1.f);
	float Elapsed = 0.f;
	float Phase = 0.f;
	bool bVisualsApplied = false;
};
