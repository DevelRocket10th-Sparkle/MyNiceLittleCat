/// 포탑 탄입니다. 직선으로 날아가 처음 맞은 대상에 대미지 GE(SetByCaller Data.Damage)를 주고 사라집니다.
/// 발사한 포탑, 플레이어(Instigator), 펫은 무시합니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TurretProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UGameplayEffect;

UCLASS()
class MYNICELITTLECAT_API ATurretProjectile : public AActor
{
	GENERATED_BODY()

public:
	ATurretProjectile();

	// 스폰 직후(FinishSpawning 전)에 설정. true면 플레이어만 맞고 적은 통과
	void SetHostile(bool bNewHostile) { bHostile = bNewHostile; }
	void SetDamageEffect(TSubclassOf<UGameplayEffect> InEffect) { DamageEffectClass = InEffect; }
protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float Damage = 1.f;

private:
	bool bHostile = false;
	TSubclassOf<UGameplayEffect> DamageEffectClass;
};
