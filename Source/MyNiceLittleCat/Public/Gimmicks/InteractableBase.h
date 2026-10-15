// 작성자: 최요환
// 작성일자: 2026-10-15
//
// 상호작용 기믹 베이스 액터입니다.
// 레버, 스위치 등 플레이어가 상호작용하는 기믹은 이 클래스를 상속해서 만듭니다.
// 상호작용하면 활성 상태(bActivated)가 바뀌고, 상태 변경 델리게이트를 브로드캐스트한 뒤
// LinkedActors 중 IActivatableInterface 구현체에 Activate/Deactivate 신호를 보냅니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractableInterface.h"
#include "InteractableBase.generated.h"

class UStaticMeshComponent;
class AInteractableBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInteractableActivationChanged, AInteractableBase*, Interactable, bool, bActivated, AActor*, ChangeInstigator);

UCLASS(Abstract)
class MYNICELITTLECAT_API AInteractableBase : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AInteractableBase();

	// IInteractableInterface
	virtual void Interact_Implementation(AActor* InteractInstigator) override;
	virtual bool CanInteract_Implementation(AActor* InteractInstigator) const override;
	virtual void OnFocusBegin_Implementation(AActor* InteractInstigator) override;
	virtual void OnFocusEnd_Implementation(AActor* InteractInstigator) override;

	// 활성 상태 변경 (같은 상태면 무시). 델리게이트 브로드캐스트 + 연결 대상에 신호 전달
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetActivated(bool bNewActivated, AActor* ChangeInstigator);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsActivated() const { return bActivated; }

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionEnabled(bool bEnabled) { bInteractionEnabled = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInteractionEnabled() const { return bInteractionEnabled; }

public:
	// 활성 상태가 바뀔 때 브로드캐스트
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractableActivationChanged OnActivationChanged;

protected:
	virtual void BeginPlay() override;

	// 상태 변경 시 자식 클래스용 처리 (C++)
	virtual void HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator) {}

	// 상태 변경 시 BP 연출용 이벤트 (레버 애니메이션, 사운드 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction", meta = (DisplayName = "On Activation Changed"))
	void K2_OnActivationChanged(bool bNewActivated, AActor* ChangeInstigator);

	// LinkedActors에 현재 상태 신호 전달
	void NotifyLinkedActors();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// 상호작용 가능 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	bool bInteractionEnabled = true;

	// true: 상호작용할 때마다 켜짐/꺼짐 토글, false: 한 번 켜지면 상호작용 잠김
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	bool bToggleable = true;

	// 시작 시 활성 상태 (BeginPlay에서 연결 대상에도 신호 전달)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	bool bStartActivated = false;

	// 상태 변경 시 신호를 받을 액터 (IActivatableInterface 구현체만 신호 수신)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Interaction")
	TArray<TObjectPtr<AActor>> LinkedActors;

	// 포커스 시 메시에 커스텀 뎁스 켜기 (외곽선 포스트프로세스용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Focus")
	bool bHighlightOnFocus = true;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Interaction")
	bool bActivated = false;
};
