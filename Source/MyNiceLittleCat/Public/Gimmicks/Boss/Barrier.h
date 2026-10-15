// 작성자: 최요환
// 작성일자: 2026-10-15
//
// 보스전 배리어입니다.
// 에디터에서 지정한 생성장치(Generators)를 구독하고, 배리어가 켜져 있는 동안 전부 무력화되면 붕괴합니다.
// 보스와는 느슨하게 연결합니다: 보스 페이즈가 ActivateBarrier()를 호출하고 OnBarrierBroken을 구독하며,
// 보스 피격 처리에서 IsBarrierActive()로 피해 무효 여부를 판단합니다. 메시는 시각 표현용입니다.
// IActivatableInterface도 구현해서 레버 등 다른 기믹으로도 켜고 끌 수 있습니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ActivatableInterface.h"
#include "Barrier.generated.h"

class UStaticMeshComponent;
class ABarrierGenerator;
class ABarrier;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBarrierStateChanged, ABarrier*, Barrier);

// TODO(합의): 보스 담당과 배리어 활성화 / 깨짐 알림 방식 확정 필요 (현재: 보스가 ABarrier를 직접 참조)
UCLASS()
class MYNICELITTLECAT_API ABarrier : public AActor, public IActivatableInterface
{
	GENERATED_BODY()

public:
	ABarrier();

	// IActivatableInterface
	virtual void Activate_Implementation(AActor* Activator) override;
	virtual void Deactivate_Implementation(AActor* Activator) override;

	// 배리어 켜기: 생성장치 전부 리셋 후 표시
	UFUNCTION(BlueprintCallable, Category = "Barrier")
	void ActivateBarrier();

	// 배리어 끄기 (깨짐 이벤트 없이 끔, 페이즈 종료 등 외부 사유)
	UFUNCTION(BlueprintCallable, Category = "Barrier")
	void DeactivateBarrier();

	UFUNCTION(BlueprintPure, Category = "Barrier")
	bool IsBarrierActive() const { return bBarrierActive; }

public:
	UPROPERTY(BlueprintAssignable, Category = "Barrier")
	FOnBarrierStateChanged OnBarrierActivated;

	// 생성장치 전부 무력화로 붕괴
	UPROPERTY(BlueprintAssignable, Category = "Barrier")
	FOnBarrierStateChanged OnBarrierBroken;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleGeneratorDisabled(ABarrierGenerator* Generator);

	bool AreAllGeneratorsDisabled() const;

	void BreakBarrier();

	// 켜짐/꺼짐 시각 처리 (메시 표시 전환). BP에서 연출 추가 가능
	void ApplyVisualState();

	UFUNCTION(BlueprintImplementableEvent, Category = "Barrier", meta = (DisplayName = "On Barrier State Changed"))
	void K2_OnBarrierStateChanged(bool bActive, bool bBroken);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// 이 배리어를 유지하는 생성장치들 (레벨에서 지정)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Barrier")
	TArray<TObjectPtr<ABarrierGenerator>> Generators;

	// 시작하자마자 켤지 여부 (보스전에선 보통 false, 보스 페이즈가 켬)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier")
	bool bActivateOnBeginPlay = false;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Barrier")
	bool bBarrierActive = false;
};
