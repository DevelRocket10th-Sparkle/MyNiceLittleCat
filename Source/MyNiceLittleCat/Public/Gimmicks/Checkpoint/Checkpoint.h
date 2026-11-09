/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 체크포인트 액터입니다.
/// 플레이어가 상호작용(E)하면 UGameProgressSubsystem에 현재 체크포인트로 등록하고 진행상태를 저장합니다.
/// 사망 후 부활(UGameProgressSubsystem::RespawnPlayer) 시 RespawnPoint 위치·방향에서 다시 시작합니다.
/// 현재 등록된 체크포인트만 활성 상태(bActivated)가 되며, 다른 체크포인트를 등록하면 자동으로 꺼집니다.
/// 활성 상태 변화는 베이스의 OnActivationChanged / LinkedActors 신호로 연출(불 켜짐 등)에 연결할 수 있습니다.
/// 이미 활성인 체크포인트에도 다시 상호작용할 수 있고, 그때마다 다시 저장합니다.
/// bPlayRestTransition이면 저장 후 휴식 연출(입력 잠금 + 화면 어둡게 → 밝게, UScreenTransitionSubsystem)을 재생하고,
/// 화면이 완전히 어두울 때 On Rest Screen Dark, 끝나면 On Rest Finished BP 이벤트를 호출합니다. 연출 중에는 상호작용할 수 없습니다.
///
/// [메뉴·빠른 이동] (2026-10-26 추가)
/// 상호작용하면 체크포인트를 발견 목록에 등록하고, MenuWidgetClass가 있으면 메뉴(휴식 / 이동 / 나가기)를 엽니다.
/// 휴식(Rest): 마지막 체크포인트로 등록·저장 + 휴식 연출 + 화면이 어두울 때 체력 회복과 OnPlayerRested 방송.
/// 이동(TravelTo): 발견한 체크포인트로 빠른 이동. 같은 맵이면 리스폰, 다른 맵이면 그 맵을 열어 체크포인트에서 시작.
/// MenuWidgetClass가 비어 있으면 상호작용 즉시 휴식합니다 (기존 동작).
/// TODO(합의): 몬스터 리셋은 OnPlayerRested에 연결
///
/// [켜짐·꺼짐 외형] (2026-11-06 추가)
/// 활성 상태(현재 체크포인트)에 따라 컴포넌트 태그로 외형을 바꿉니다. BP 그래프 없이 메시 배치·태그만으로 연출을 만들 수 있습니다.
/// - StateOnTag(기본 CheckpointOn): 켜졌을 때만 보임. 라이트는 밝기가 0 → 원래 값으로, 나이아가라 등 이펙트는 켜짐/꺼짐
/// - StateOffTag(기본 CheckpointOff): 꺼졌을 때만 보임 (예: 꺼진 장작)
/// - StateGlowTag(기본 CheckpointGlow): 머티리얼 스칼라 파라미터 StateGlowParameterName을 0(꺼짐) → 1(켜짐)
/// 전환은 StateBlendTime 동안 부드럽게, 게임 시작 시 상태는 바로 적용

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/InteractableBase.h"
#include "Frameworks/Transition/ScreenTransitionSubsystem.h"
#include "Frameworks/Save/GameProgressSaveGame.h"
#include "Checkpoint.generated.h"

class UArrowComponent;
class UCheckpointMenuWidget;
class ULightComponent;
class UMaterialInstanceDynamic;
class AGimmickActorBase;

UCLASS()
class MYNICELITTLECAT_API ACheckpoint : public AInteractableBase
{
	GENERATED_BODY()

public:
	ACheckpoint();

	// IInteractableInterface
	virtual void Interact_Implementation(AActor* InteractInstigator) override;
	virtual bool CanInteract_Implementation(AActor* InteractInstigator) const override;

	// 저장에 쓰는 Id (비어 있으면 액터 이름)
	UFUNCTION(BlueprintPure, Category = "Checkpoint")
	FName GetCheckpointId() const;

	// 부활 위치 (발 위치)·방향
	UFUNCTION(BlueprintPure, Category = "Checkpoint")
	FTransform GetRespawnTransform() const;

	// 빠른 이동 목록에 표시할 이름 (비어 있으면 Id)
	UFUNCTION(BlueprintPure, Category = "Checkpoint")
	FText GetCheckpointDisplayName() const;

	// 휴식: 마지막 체크포인트로 등록·저장 + 휴식 연출 + 체력 회복
	UFUNCTION(BlueprintCallable, Category = "Checkpoint")
	void Rest(AActor* InteractInstigator);

	// 발견한 체크포인트로 빠른 이동 (화면 어두울 때 리스폰 / 맵 이동)
	UFUNCTION(BlueprintCallable, Category = "Checkpoint")
	void TravelTo(AActor* InteractInstigator, const FCheckpointSaveData& Destination);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// 활성 상태가 바뀌면 켜짐·꺼짐 외형 전환
	UFUNCTION()
	void HandleStateVisualActivation(AGimmickActorBase* Gimmick, bool bNewActivated, AActor* ChangeInstigator);

	// 태그 붙은 컴포넌트·라이트 기본 밝기·발광 MID 모으기 (BeginPlay 한 번)
	void CacheStateVisuals();

	// 현재 StateBlend 값을 컴포넌트에 반영
	void ApplyStateBlend();

	// 서브시스템의 체크포인트가 바뀌면 활성 표시 갱신
	UFUNCTION()
	void HandleCheckpointChanged(FName NewCheckpointId);

	// 상호작용으로 등록될 때마다 호출 (저장 알림 UI·사운드 등 BP 연출용)
	UFUNCTION(BlueprintImplementableEvent, Category = "Checkpoint", meta = (DisplayName = "On Checkpoint Interacted"))
	void K2_OnCheckpointInteracted(AActor* InteractInstigator);

	// 휴식 연출 중 화면이 완전히 어두워졌을 때 (체력 회복 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Checkpoint", meta = (DisplayName = "On Rest Screen Dark"))
	void K2_OnRestScreenDark(AActor* InteractInstigator);

	// 휴식 연출이 끝나고 입력이 풀렸을 때
	UFUNCTION(BlueprintImplementableEvent, Category = "Checkpoint", meta = (DisplayName = "On Rest Finished"))
	void K2_OnRestFinished(AActor* InteractInstigator);

	// 상호작용한 폰의 플레이어 컨트롤러로 휴식 연출 재생 (재생 못 하면 false)
	bool PlayRestTransition(AActor* InteractInstigator);

	// 체력 회복 + OnPlayerRested 방송
	void RestorePlayer(AActor* InteractInstigator);

protected:
	// 부활 지점. 화살표 위치를 바닥(발 위치)에, 방향을 부활 후 바라볼 방향으로 맞춤
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> RespawnPoint;

	// 맵 안에서 체크포인트를 구분하는 Id. 비워 두면 액터 이름 사용
	// (액터 이름은 복제·재배치하면 바뀌므로 정식 맵에서는 지정 권장)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Checkpoint")
	FName CheckpointId;

	// 빠른 이동 목록에 보일 이름 (예: "저택 현관")
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Checkpoint")
	FText DisplayName;

	// 상호작용 시 열 메뉴 (휴식 / 이동 / 나가기). 비우면 바로 휴식
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Menu")
	TSubclassOf<UCheckpointMenuWidget> MenuWidgetClass;

	// 빠른 이동 화면 전환 설정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Travel")
	FScreenTransitionSettings TravelTransition;

	// 상호작용 시 바로 세이브 슬롯에 저장
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	bool bSaveOnInteract = true;

	// 상호작용 시 휴식 연출(입력 잠금 + 화면 어둡게 → 밝게) 재생
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Rest")
	bool bPlayRestTransition = true;

	// 휴식 연출 시간·색·입력 잠금 설정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Rest", meta = (EditCondition = "bPlayRestTransition"))
	FScreenTransitionSettings RestTransition;

	// 켜졌을 때만 보이는 컴포넌트 태그 (라이트는 밝기 페이드, 이펙트는 켜기/끄기)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Visual")
	FName StateOnTag = TEXT("CheckpointOn");

	// 꺼졌을 때만 보이는 컴포넌트 태그
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Visual")
	FName StateOffTag = TEXT("CheckpointOff");

	// 켜지면 발광하는 메시 태그 (모든 머티리얼 슬롯에 StateGlowParameterName 0 → 1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Visual")
	FName StateGlowTag = TEXT("CheckpointGlow");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Visual")
	FName StateGlowParameterName = TEXT("CheckpointGlow");

	// 켜짐·꺼짐 전환 시간(초). 0이면 바로
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Visual", meta = (ClampMin = "0.0"))
	float StateBlendTime = 0.8f;

private:
	// 0 = 꺼짐, 1 = 켜짐
	float StateBlend = 0.f;
	float StateBlendTarget = 0.f;
	bool bStateVisualsCached = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> StateOnComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> StateOffComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ULightComponent>> StateLights;

	TArray<float> StateLightBaseIntensities;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> StateGlowMaterials;
};
