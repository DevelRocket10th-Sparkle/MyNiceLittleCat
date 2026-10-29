/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// E로 누르는 버튼입니다. 누르면 켜지고 LinkedActors에 신호를 보냅니다.
/// 동작은 ButtonMode 하나로 정합니다:
///   Once  = 한 번 누르면 계속 켜짐, 다시 누를 수 없음
///   Toggle = 누를 때마다 켜짐/꺼짐
///   Timed = 누르면 ActiveDuration 동안 켜졌다가 자동으로 꺼짐 (시간 제한 퍼즐), 켜져 있는 동안 다시 누를 수 없음
/// 베이스의 Toggleable은 ButtonMode가 정하므로 버튼 디테일에서는 숨깁니다.
/// Mesh(루트) = 받침, 눌리는 부분은 ButtonMover 아래 ButtonMesh에 붙입니다. 메시는 BP에서 지정합니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/InteractableBase.h"
#include "GimmickButton.generated.h"

class UGimmickMoverComponent;

UENUM(BlueprintType)
enum class EGimmickButtonMode : uint8
{
	// 한 번 누르면 계속 켜짐
	Once,
	// 누를 때마다 켜짐/꺼짐
	Toggle,
	// 누르면 일정 시간 켜졌다가 자동으로 꺼짐
	Timed
};

UCLASS()
class MYNICELITTLECAT_API AGimmickButton : public AInteractableBase
{
	GENERATED_BODY()

public:
	AGimmickButton();

	virtual void Interact_Implementation(AActor* InteractInstigator) override;
	virtual bool CanInteract_Implementation(AActor* InteractInstigator) const override;

	// 자동으로 꺼지기까지 남은 시간 (타이머 없으면 0)
	UFUNCTION(BlueprintPure, Category = "Gimmick|Button")
	float GetRemainingActiveTime() const;

	UFUNCTION(BlueprintPure, Category = "Gimmick|Button")
	EGimmickButtonMode GetButtonMode() const { return ButtonMode; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator) override;

	void HandleActiveTimeExpired();

	// 베이스의 Toggleable을 ButtonMode에 맞춤
	void ApplyButtonMode();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGimmickMoverComponent> ButtonMover;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ButtonMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Button")
	EGimmickButtonMode ButtonMode = EGimmickButtonMode::Once;

	// Timed: 켜진 뒤 자동으로 꺼지기까지 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Button", meta = (ClampMin = "0.1", Units = "s", EditCondition = "ButtonMode == EGimmickButtonMode::Timed", EditConditionHides))
	float ActiveDuration = 3.f;

private:
	FTimerHandle ActiveTimer;
};
