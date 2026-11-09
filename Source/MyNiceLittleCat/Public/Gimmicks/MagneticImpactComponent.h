// 던져진(빠르게 날아가는) 물리 물체가 적에게 부딪히면 대미지를 주고 밀쳐내는 컴포넌트입니다.
// 자성 박스, 포탑 등 루트가 Simulate Physics인 액터에 붙입니다.
// 충돌 직전 속도가 MinImpactSpeed 이상일 때만 작동하고, 속도에 비례해 대미지가 커집니다.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MagneticImpactComponent.generated.h"

class UGameplayEffect;
class USoundBase;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UMagneticImpactComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMagneticImpactComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// 이 속도(cm/s) 이상으로 부딪혀야 대미지
	UPROPERTY(EditAnywhere, Category = "Impact", meta = (ClampMin = "0.0"))
	float MinImpactSpeed = 600.f;

	// 이 속도에서 BaseDamage, 더 빠르면 MaxDamageScale배까지 증가
	UPROPERTY(EditAnywhere, Category = "Impact", meta = (ClampMin = "1.0"))
	float ReferenceSpeed = 1500.f;

	UPROPERTY(EditAnywhere, Category = "Impact", meta = (ClampMin = "0.0"))
	float BaseDamage = 20.f;

	UPROPERTY(EditAnywhere, Category = "Impact", meta = (ClampMin = "1.0"))
	float MaxDamageScale = 2.f;

	// SetByCaller Data.Damage 를 쓰는 대미지 GE (근접 공격과 같은 것)
	UPROPERTY(EditAnywhere, Category = "Impact")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// 적을 밀어내는 힘 (LaunchCharacter 속도, cm/s)
	UPROPERTY(EditAnywhere, Category = "Impact", meta = (ClampMin = "0.0"))
	float KnockbackStrength = 900.f;

	UPROPERTY(EditAnywhere, Category = "Impact", meta = (ClampMin = "0.0"))
	float KnockbackUpward = 250.f;

	// 부딪힌 뒤 물체 속도에 곱할 값 (0 = 그 자리에서 멈춤, 1 = 그대로 튕겨나감)
	UPROPERTY(EditAnywhere, Category = "Impact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BounceVelocityScale = 0.3f;

	// 같은 적을 연속으로 때리지 않도록 하는 간격
	UPROPERTY(EditAnywhere, Category = "Impact", meta = (ClampMin = "0.0"))
	float SameTargetCooldown = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Impact")
	TObjectPtr<USoundBase> ImpactSound;

private:
	TWeakObjectPtr<UPrimitiveComponent> Body;
	FVector LastVelocity = FVector::ZeroVector;
	TMap<TWeakObjectPtr<AActor>, float> LastHitTime;
};
