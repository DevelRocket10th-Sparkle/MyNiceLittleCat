/// 작성자: 최요환
/// 작성일자: 2026-10-15
///
/// 보스전 배리어 생성장치입니다.
/// 비활성(숨김, 피해 무시) 상태로 시작하고, ABarrier가 켜질 때 ActivateGenerator()로 가동됩니다.
/// 내구도를 가지며 피해(UGameplayStatics::ApplyDamage 등 엔진 표준 TakeDamage 경로)를 받아 0이 되면 무력화됩니다.
/// 무력화되면 OnGeneratorDisabled를 브로드캐스트하고 활성 상태를 끕니다(LinkedActors에 Deactivate 전달).
/// 받을 피해 종류는 AllowedDamageTypes로 제한하고(예: 70% 페이즈는 자성 코인, 30% 페이즈는 반사 레이저),
/// 추가 조건이 필요하면 CanBeDamagedBy를 오버라이드합니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/InteractableBase.h"
#include "BarrierGenerator.generated.h"

class ABarrierGenerator;
class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGeneratorDisabled, ABarrierGenerator*, Generator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGeneratorDurabilityChanged, ABarrierGenerator*, Generator, float, CurrentDurability, float, MaxDurability);

UCLASS()
class MYNICELITTLECAT_API ABarrierGenerator : public AInteractableBase
{
	GENERATED_BODY()

public:
	ABarrierGenerator();

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// 가동: 내구도 회복 + 무력화 해제 + 표시 (배리어 활성화 시 호출)
	UFUNCTION(BlueprintCallable, Category = "Barrier")
	void ActivateGenerator();

	// 정지: 숨김 + 피해 무시 (배리어 붕괴/해제 시 호출)
	UFUNCTION(BlueprintCallable, Category = "Barrier")
	void DeactivateGenerator();

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

	// 이 피해를 받을지 판단 (기본: 가동 중 + 무력화 전 + 허용된 피해 타입)
	UFUNCTION(BlueprintNativeEvent, Category = "Barrier")
	bool CanBeDamagedBy(AActor* DamageCauser, TSubclassOf<UDamageType> DamageTypeClass) const;

	bool IsAllowedDamageType(TSubclassOf<UDamageType> DamageTypeClass) const;

	void DisableGenerator(AActor* DisableInstigator);

	void SetGeneratorPresence(bool bPresent);

protected:
	// 최대 내구도 (피해량 합계가 이 값에 도달하면 무력화)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier", meta = (ClampMin = "0.01"))
	float MaxDurability = 3.0f;

	// 받을 수 있는 피해 타입 (비어 있으면 모든 피해 허용, 하위 클래스 포함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier")
	TArray<TSubclassOf<UDamageType>> AllowedDamageTypes;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Barrier")
	float CurrentDurability = 0.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Barrier")
	bool bDisabled = false;
};
