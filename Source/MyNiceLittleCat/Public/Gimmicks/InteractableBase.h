/// 작성자: 최요환
/// 작성일자: 2026-10-15
///
/// 상호작용 기믹 베이스 액터입니다.
/// 레버, 스위치 등 플레이어가 상호작용하는 기믹은 이 클래스를 상속해서 만듭니다.
/// 상호작용하면 활성 상태(bActivated)가 바뀌고, 상태 변경 델리게이트를 브로드캐스트한 뒤
/// LinkedActors 중 IActivatableInterface 구현체에 Activate/Deactivate 신호를 보냅니다.
/// AlignMode와 InteractionPoint 컴포넌트로 상호작용 전 캐릭터의 위치·방향 정렬을 지정합니다.
/// 감지 거리 판정은 InteractionVolume 컴포넌트(없으면 메시)에서 플레이어와 가장 가까운 점 기준입니다.
/// 포커스 중 안내 UI는 PromptText(동작 텍스트)와 PromptAnchorTag 컴포넌트(없으면 메시 윗면) 위치로 표시됩니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractableInterface.h"
#include "InteractableBase.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;
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
	virtual FInteractAlignment GetInteractAlignment_Implementation(AActor* InteractInstigator) const override;
	virtual FVector GetInteractFocusLocation_Implementation(AActor* InteractInstigator) const override;
	virtual FInteractPromptInfo GetInteractPrompt_Implementation(AActor* InteractInstigator) const override;

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

	// 프리미티브에서 From과 가장 가까운 점 (콜리전 우선, 없으면 바운드 박스)
	static FVector GetClosestPointOn(const UPrimitiveComponent* Primitive, const FVector& From);

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
	// 기본 꺼짐: 외곽선 머티리얼이 아직 없고, 켜면 플레이어 실루엣 PP(커스텀 뎁스 사용)가 이 기믹 뒤에서 가려짐
	// TODO: 포커스 표시가 필요해지면 커스텀 뎁스 대신 Overlay Material 방식 검토 (실루엣과 충돌 없음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Focus")
	bool bHighlightOnFocus = false;

	// 상호작용 전 캐릭터 정렬 방식
	// SnapToPoint: InteractionPoint 컴포넌트 중 가장 가까운 지점으로 이동,
	//              지점이 없으면 액터 중심에서 플레이어 쪽으로 AlignRadius만큼 떨어진 위치로 이동
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Align")
	EInteractAlignMode AlignMode = EInteractAlignMode::FaceOnly;

	// 지점 컴포넌트가 없을 때 액터 중심으로부터 설 거리 (0이면 바라보기만)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Align", meta = (ClampMin = "0.0", EditCondition = "AlignMode == EInteractAlignMode::SnapToPoint"))
	float AlignRadius = 0.f;

	// 목표 위치가 이보다 멀면 이동하지 않고 바라보기만 함 (먼 거리를 미끄러지듯 이동하는 것 방지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Align", meta = (ClampMin = "0.0", EditCondition = "AlignMode == EInteractAlignMode::SnapToPoint"))
	float MaxAlignDistance = 150.f;

	// 포커스 중 안내 UI(키 + 동작 텍스트) 표시
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Prompt")
	bool bShowPrompt = true;

	// 동작 텍스트 (예: "당기기"). 비면 키만 표시
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Prompt", meta = (EditCondition = "bShowPrompt"))
	FText PromptText;

	// 활성 상태일 때 동작 텍스트 (예: 켜기/끄기). 비면 PromptText 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Prompt", meta = (EditCondition = "bShowPrompt"))
	FText PromptTextActivated;

	// 이 태그가 붙은 씬 컴포넌트가 있으면 그 위치에 표시, 없으면 메시 바운드 윗면 중앙
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Prompt", meta = (EditCondition = "bShowPrompt"))
	FName PromptAnchorTag = TEXT("InteractPrompt");

	// 표시 위치에 더할 월드 오프셋
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Prompt", meta = (EditCondition = "bShowPrompt"))
	FVector PromptOffset = FVector(0.f, 0.f, 30.f);

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Interaction")
	bool bActivated = false;
};
