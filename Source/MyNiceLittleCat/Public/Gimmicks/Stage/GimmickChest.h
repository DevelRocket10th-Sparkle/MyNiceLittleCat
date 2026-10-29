/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// E로 여는 보물 상자입니다. 열면 Contents의 아이템을 인벤토리에 넣고 획득 UI를 표시합니다.
/// 한 번 열면 끝 (다시 열 수 없음, 안내 UI도 사라짐). 상태 저장(bSaveState)이 기본으로 켜져 있어
/// 체크포인트에서 저장한 뒤에는 열린 채로 시작하고, 이때는 아이템을 다시 주지 않습니다 (아이템도 같은 세이브에 들어 있음).
/// Mesh(루트) = 몸통, 뚜껑은 LidMover 아래 LidMesh에 붙입니다 (회전 중심 = LidMover = 뒤쪽 경첩). 메시는 BP에서 지정합니다.
/// 열기 연출(소리·이펙트)은 On Chest Opened 이벤트 또는 LidMover의 OnMoveStarted/Finished에 연결합니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/InteractableBase.h"
#include "Items/ItemTypes.h"
#include "GimmickChest.generated.h"

class UGimmickMoverComponent;

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

protected:
	virtual void HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator) override;

	// 열렸을 때 (저장된 상태로 시작할 때는 호출 안 됨). AcquiredItems = 실제로 받은 아이템 (최대 개수로 잘린 것 제외)
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Chest", meta = (DisplayName = "On Chest Opened"))
	void K2_OnChestOpened(AActor* Opener, const TArray<FItemStack>& AcquiredItems);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGimmickMoverComponent> LidMover;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LidMesh;

	// 들어 있는 아이템
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Chest")
	TArray<FItemStack> Contents;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Chest")
	bool bShowAcquireUI = true;
};
