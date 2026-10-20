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
/// TODO(합의): 휴식 중 체력 회복·몬스터 리셋 여부는 기획 확정 후 On Rest Screen Dark에 연결

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/InteractableBase.h"
#include "Frameworks/Transition/ScreenTransitionSubsystem.h"
#include "Checkpoint.generated.h"

class UArrowComponent;

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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	// 상호작용한 폰의 플레이어 컨트롤러로 휴식 연출 재생
	void PlayRestTransition(AActor* InteractInstigator);

protected:
	// 부활 지점. 화살표 위치를 바닥(발 위치)에, 방향을 부활 후 바라볼 방향으로 맞춤
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> RespawnPoint;

	// 맵 안에서 체크포인트를 구분하는 Id. 비워 두면 액터 이름 사용
	// (액터 이름은 복제·재배치하면 바뀌므로 정식 맵에서는 지정 권장)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Checkpoint")
	FName CheckpointId;

	// 상호작용 시 바로 세이브 슬롯에 저장
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	bool bSaveOnInteract = true;

	// 상호작용 시 휴식 연출(입력 잠금 + 화면 어둡게 → 밝게) 재생
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Rest")
	bool bPlayRestTransition = true;

	// 휴식 연출 시간·색·입력 잠금 설정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Rest", meta = (EditCondition = "bPlayRestTransition"))
	FScreenTransitionSettings RestTransition;
};
