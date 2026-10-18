/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 보스 패턴의 자성 코인입니다.
/// 물리 메시 + UMagneticComponent로 구성되어 펫 자석 능력에 끌리거나 밀립니다.
/// 자력을 받은 직후(MagneticForceGraceTime 이내) MinImpactSpeed 이상으로 대상에 부딪히면
/// UGameplayStatics::ApplyDamage(DamageTypeClass = UDamageType_MagneticCoin)로 피해를 줍니다.
/// 기본 대상은 ABarrierGenerator이며, 생성장치는 AllowedDamageTypes로 자성 코인 피해만 받도록 설정합니다.
/// 극성은 스폰 시 보스 패턴이 SetPolarity로 지정합니다.
/// TODO: 낙하(경고 데칼, 착지 피해) → 바닥 → 던져짐 → 소멸 상태 처리는 다음 단계에서 추가

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gimmicks/Magnetic/MagneticTypes.h"
#include "MagneticCoin.generated.h"

class UStaticMeshComponent;
class UMagneticComponent;
class UDamageType;
class AMagneticCoin;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMagneticCoinDamaged, AMagneticCoin*, Coin, AActor*, DamagedActor);

UCLASS()
class MYNICELITTLECAT_API AMagneticCoin : public AActor
{
	GENERATED_BODY()

public:
	AMagneticCoin();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "MagneticCoin")
	void SetPolarity(EMagneticPolarity NewPolarity);

	UFUNCTION(BlueprintPure, Category = "MagneticCoin")
	EMagneticPolarity GetPolarity() const;

public:
	// 대상이 실제로 피해를 받았을 때 브로드캐스트 (bDestroyOnDamage면 직후 소멸)
	UPROPERTY(BlueprintAssignable, Category = "MagneticCoin")
	FOnMagneticCoinDamaged OnCoinDamaged;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void HandleMagneticForce(UMagneticComponent* InMagneticComponent, FVector Force);

	// 이 대상에 피해를 줄 수 있는지 (대상 클래스·속도·자력 조건·쿨다운)
	bool CanDamageTarget(const AActor* OtherActor) const;

	bool IsDamageTargetClass(const AActor* OtherActor) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMagneticComponent> MagneticComponent;

	// 한 번 부딪힐 때 주는 피해량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage", meta = (ClampMin = "0.0"))
	float Damage = 1.f;

	// 피해 타입 (생성장치 AllowedDamageTypes 판별용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage")
	TSubclassOf<UDamageType> DamageTypeClass;

	// 이 클래스(하위 포함) 액터에만 피해 (비어 있으면 모든 액터)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage")
	TArray<TSubclassOf<AActor>> DamageTargetClasses;

	// 부딪힐 때 이 속도 이상이어야 피해 (굴러가다 살짝 닿는 경우 제외)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage", meta = (ClampMin = "0.0"))
	float MinImpactSpeed = 300.f;

	// true면 자력을 받은 직후에만 피해 (자석 능력으로 날린 코인만 유효)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage")
	bool bRequireMagneticForce = true;

	// 마지막으로 자력을 받은 뒤 피해를 인정하는 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage", meta = (ClampMin = "0.0", EditCondition = "bRequireMagneticForce"))
	float MagneticForceGraceTime = 0.5f;

	// 피해 후 다시 피해를 줄 수 있을 때까지의 시간 (한 번 부딪힐 때 히트 이벤트가 여러 번 오는 것 방지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage", meta = (ClampMin = "0.0"))
	float DamageCooldown = 0.5f;

	// 피해를 주면(대상이 실제로 피해를 받으면) 코인 소멸
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage")
	bool bDestroyOnDamage = true;

private:
	// 히트 이벤트 시점엔 충돌로 감속돼 있을 수 있어 이전 틱 속도도 함께 사용
	float PreviousSpeed = 0.f;

	float LastMagneticForceTime = -1.f;

	float LastDamageTime = -1.f;
};
