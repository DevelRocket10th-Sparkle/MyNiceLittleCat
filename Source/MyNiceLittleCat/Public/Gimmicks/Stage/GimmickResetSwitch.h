/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// E로 누르면 Targets의 기믹을 처음 상태로 되돌리는 스위치입니다 (순서 퍼즐이 막혔을 때 다시 시작).
/// 대상은 IGimmickResettableInterface 구현체만 처리합니다 (수신기: 꺼지고 충전 0, 거울: 시작 각도).
/// 켜짐/꺼짐 신호(LinkedActors)는 보내지 않으며, 스위치 자신의 상태도 쓰지 않습니다.
/// 누른 뒤 ResetCooldown 동안은 다시 누를 수 없습니다. Mesh(루트)는 BP에서 지정합니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/InteractableBase.h"
#include "GimmickResetSwitch.generated.h"

UCLASS()
class MYNICELITTLECAT_API AGimmickResetSwitch : public AInteractableBase
{
	GENERATED_BODY()

public:
	AGimmickResetSwitch();

	// IInteractableInterface
	virtual void Interact_Implementation(AActor* InteractInstigator) override;
	virtual bool CanInteract_Implementation(AActor* InteractInstigator) const override;

	// ResetTargets를 모두 되돌림 (쿨다운 무시)
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Reset")
	void ResetTargets(AActor* ResetInstigator);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 되돌린 직후 (BP 연출용: 소리 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Reset", meta = (DisplayName = "On Reset Triggered"))
	void K2_OnResetTriggered(AActor* ResetInstigator);

protected:
	// 되돌릴 기믹 (IGimmickResettableInterface 구현체만 처리)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Reset")
	TArray<TObjectPtr<AActor>> Targets;

	// 누른 뒤 다시 누를 수 있기까지 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Reset", meta = (ClampMin = "0.0", Units = "s"))
	float ResetCooldown = 1.f;

private:
	FTimerHandle CooldownTimer;
};
