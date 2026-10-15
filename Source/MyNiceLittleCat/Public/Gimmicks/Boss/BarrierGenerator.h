// 작성자: 최요환
// 작성일자: 2026-10-15
//
// 보스전 배리어 생성장치입니다.
// 내구도를 가지며 피해(UGameplayStatics::ApplyDamage 등 엔진 표준 TakeDamage 경로)를 받아 0이 되면 무력화됩니다.
// 무력화되면 OnGeneratorDisabled를 브로드캐스트하고 활성 상태를 끕니다(LinkedActors에 Deactivate 전달).
// 어떤 피해를 받을지(극성 일치 등)는 CanBeDamagedBy를 오버라이드해서 결정합니다.
// ABarrier가 이 장치들을 구독해서 전부 무력화되면 붕괴합니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/InteractableBase.h"
#include "BarrierGenerator.generated.h"

class ABarrierGenerator;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGeneratorDisabled, ABarrierGenerator*, Generator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGeneratorDurabilityChanged, ABarrierGenerator*, Generator, float, CurrentDurability, float, MaxDurability);

UCLASS()
class MYNICELITTLECAT_API ABarrierGenerator : public AInteractableBase
{
	GENERATED_BODY()

public:
	ABarrierGenerator();

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// 내구도 회복 + 무력화 해제 (배리어 재활성화 시 호출)
	UFUNCTION(BlueprintCallable, Category = "Barrier")
	void ResetGenerator();

	UFUNCTION(BlueprintPure, Category = "Barrier")
	bool IsDisabled() const { return bDisabled; }

	UFUNCTION(BlueprintPure, Category = "Barrier")
	float GetCurrentDurability() const { return CurrentDurability; }

	UFUNCTION(BlueprintPure, Category = "Barrier")
	float GetMaxDurability() const { return MaxDurability; }

public:
	UPROPERTY(BlueprintAssignable, Category = "Barrier")
	FOnGeneratorDisabled OnGeneratorDisabled;

	UPROPERTY(BlueprintAssignable, Category = "Barrier")
	FOnGeneratorDurabilityChanged OnDurabilityChanged;

protected:
	virtual void BeginPlay() override;

	// 이 피해를 받을지 판단 (기본: 무력화 전이면 모두 허용)
	// TODO(합의): 자성 코인 작업 시 극성 일치 조건 추가 예정
	UFUNCTION(BlueprintNativeEvent, Category = "Barrier")
	bool CanBeDamagedBy(AActor* DamageCauser) const;

	void DisableGenerator(AActor* DisableInstigator);

protected:
	// 최대 내구도 (피해량 합계가 이 값에 도달하면 무력화)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier", meta = (ClampMin = "0.01"))
	float MaxDurability = 3.0f;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Barrier")
	float CurrentDurability = 0.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Barrier")
	bool bDisabled = false;
};
