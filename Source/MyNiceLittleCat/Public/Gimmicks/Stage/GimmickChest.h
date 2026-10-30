/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// E로 여는 보물 상자입니다. 열면 Contents의 아이템을 인벤토리에 넣고 획득 연출을 진행합니다.
/// 획득 연출 (각 단계는 옵션):
///   1. 여는 순간 게임 일시정지 (플레이어 입력·몬스터·함정 멈춤, 뚜껑만 움직임) - bPauseGameDuringAcquire
///   2. 뚜껑이 다 열리고 AcquireUIDelay 뒤 획득 UI 표시 - bWaitLidOpenForAcquireUI
///   3. 아무 키나 누르면 UI를 닫고 게임 재개 - bCloseAcquireUIOnAnyKey (끄면 UI는 시간이 지나면 사라지고 게임은 UI 표시와 함께 재개)
/// 한 번 열면 끝 (다시 열 수 없음, 안내 UI도 사라짐). 상태 저장(bSaveState)이 기본으로 켜져 있어
/// 체크포인트에서 저장한 뒤에는 열린 채로 시작하고, 이때는 아이템을 다시 주지 않고 연출도 없습니다 (아이템도 같은 세이브에 들어 있음).
/// Mesh(루트) = 몸통, 뚜껑은 LidMover 아래 LidMesh에 붙입니다 (회전 중심 = LidMover = 뒤쪽 경첩). 메시는 BP에서 지정합니다.
/// 연출(소리·이펙트)은 On Chest Opened / On Acquire Revealed / On Acquire Finished 이벤트에 연결합니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/InteractableBase.h"
#include "Items/ItemTypes.h"
#include "Containers/Ticker.h"
#include "GimmickChest.generated.h"

class UGimmickMoverComponent;
class UItemAcquireWidget;
class UInputComponent;

UCLASS()
class MYNICELITTLECAT_API AGimmickChest : public AInteractableBase
{
	GENERATED_BODY()

public:
	AGimmickChest();

	UFUNCTION(BlueprintPure, Category = "Gimmick|Chest")
	bool IsOpened() const { return IsActivated(); }

	UFUNCTION(BlueprintPure, Category = "Gimmick|Chest")
	const TArray<FItemStack>& GetContents() const { return Contents; }

	// 획득 연출 진행 중 (열기 ~ UI 닫기)
	UFUNCTION(BlueprintPure, Category = "Gimmick|Chest")
	bool IsAcquireSequenceActive() const { return bSequenceActive; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator) override;

	// 열기 시작할 때 (저장된 상태로 시작할 때는 호출 안 됨). AcquiredItems = 실제로 받은 아이템 (최대 개수로 잘린 것 제외)
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Chest", meta = (DisplayName = "On Chest Opened"))
	void K2_OnChestOpened(AActor* Opener, const TArray<FItemStack>& AcquiredItems);

	// 획득 UI를 띄우는 시점 (아이템 연출·소리용)
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Chest", meta = (DisplayName = "On Acquire Revealed"))
	void K2_OnAcquireRevealed(AActor* Opener, const TArray<FItemStack>& AcquiredItems);

	// 획득 연출이 끝나고 게임이 다시 진행될 때
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Chest", meta = (DisplayName = "On Acquire Finished"))
	void K2_OnAcquireFinished(AActor* Opener);

	UFUNCTION()
	void HandleLidMoveFinished(UGimmickMoverComponent* Mover, bool bOpen);

private:
	void StartAcquireSequence(AActor* Opener, const TArray<FItemStack>& Acquired);
	void RevealAcquiredItems();
	void HandleCloseKeyPressed();
	void FinishAcquireSequence();
	void ClearCloseInput();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGimmickMoverComponent> LidMover;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LidMesh;

	// 들어 있는 아이템
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Chest")
	TArray<FItemStack> Contents;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Chest|Acquire")
	bool bShowAcquireUI = true;

	// 여는 동안 게임 일시정지 (플레이어 입력·몬스터·함정 멈춤, 뚜껑만 움직임)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Chest|Acquire")
	bool bPauseGameDuringAcquire = true;

	// 뚜껑이 다 열린 뒤 획득 UI를 표시 (끄면 여는 순간 바로 표시). 아이템은 여는 순간 인벤토리에 들어감
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Chest|Acquire", meta = (EditCondition = "bShowAcquireUI"))
	bool bWaitLidOpenForAcquireUI = true;

	// 뚜껑이 다 열린 뒤 UI까지 추가로 기다리는 시간 (일시정지 중에도 실제 시간으로 흐름)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Chest|Acquire", meta = (ClampMin = "0.0", Units = "s", EditCondition = "bShowAcquireUI && bWaitLidOpenForAcquireUI"))
	float AcquireUIDelay = 0.2f;

	// UI는 아무 키나 누를 때까지 유지, 누르면 닫고 게임 재개
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Chest|Acquire", meta = (EditCondition = "bShowAcquireUI"))
	bool bCloseAcquireUIOnAnyKey = true;

	// UI가 뜬 직후 키 입력을 무시하는 시간 (연타로 바로 닫히는 것 방지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Chest|Acquire", meta = (ClampMin = "0.0", Units = "s", EditCondition = "bShowAcquireUI && bCloseAcquireUIOnAnyKey"))
	float CloseInputDelay = 0.4f;

private:
	TArray<FItemStack> PendingAcquired;
	TWeakObjectPtr<AActor> PendingOpener;
	bool bSequenceActive = false;
	bool bWaitingLidOpen = false;
	bool bPausedGame = false;
	double CloseInputEnableTime = 0.0;

	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> CloseInput;

	UPROPERTY(Transient)
	TObjectPtr<UItemAcquireWidget> ShownWidget;

	FTSTicker::FDelegateHandle RevealTickerHandle;
};
