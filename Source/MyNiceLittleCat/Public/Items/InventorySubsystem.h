/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 플레이어가 가진 아이템(아이템 데이터 에셋 → 개수)을 관리하는 게임 인스턴스 서브시스템입니다.
/// 플레이어 폰에 두지 않는 이유: 부활할 때 폰을 새로 만들어서 컴포넌트 상태가 사라지고, 맵을 옮겨도 유지해야 하기 때문.
/// 저장은 기믹 상태와 같은 방식: UGameProgressSubsystem::SaveProgress() 직전에 세이브 데이터에 기록하고, 로드·새 게임 때 다시 읽습니다.
/// 획득 경로(보물 상자, 드랍 등)는 AddItems(..., bShowAcquireUI)를 호출하면 프로젝트 설정 > Item의 획득 UI가 표시됩니다.
/// TODO(합의): 포션 사용(개수 감소 + 회복)은 플레이어 담당 쪽에서 GetItemCount/HasItem/OnItemChanged로 연결
/// 능력 해금(2026-11-05): 능력 아이템(ItemType Ability + AbilityTag)을 가지고 있으면 HasAbility(태그)가 true.
///   처음 얻는 순간 OnAbilityUnlocked(태그, 아이템) 알림 (세이브에서 읽어 올 때는 알리지 않음).
///   능력을 실제로 켜고 끄는 쪽(펫 다이얼 등)은 각 담당이 HasAbility·OnAbilityUnlocked로 연결.
///   테스트: 프로젝트 설정 > Item > bTestUnlockAllAbilities면 HasAbility가 항상 true

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Items/ItemTypes.h"
#include "InventorySubsystem.generated.h"

class UItemData;
class UItemAcquireWidget;
class UGameProgressSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInventoryItemChanged, UItemData*, Item, int32, NewCount, int32, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryItemsAcquired, const TArray<FItemStack>&, Items);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventoryAbilityUnlocked, FGameplayTag, AbilityTag, UItemData*, Item);

UCLASS()
class MYNICELITTLECAT_API UInventorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 추가 (MaxCount 넘으면 거기까지). 실제로 늘어난 개수 반환
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 AddItem(UItemData* Item, int32 Count = 1);

	// 여러 개 추가 + 획득 UI 표시 (실제로 늘어난 것만 표시). WorldContext는 UI를 띄울 월드
	UFUNCTION(BlueprintCallable, Category = "Inventory", meta = (WorldContext = "WorldContextObject"))
	TArray<FItemStack> AddItems(const UObject* WorldContextObject, const TArray<FItemStack>& InItems, bool bShowAcquireUI = true);

	// 제거 (부족하면 아무것도 안 하고 false)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(UItemData* Item, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(const UItemData* Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasItem(const UItemData* Item, int32 Count = 1) const { return GetItemCount(Item) >= Count; }

	// 이 능력 태그(정확히 일치)를 가진 능력 해금 아이템이 있는지 (테스트 설정 bTestUnlockAllAbilities면 항상 true)
	UFUNCTION(BlueprintPure, Category = "Inventory|Ability")
	bool HasAbility(FGameplayTag AbilityTag) const;

	// 가진 아이템 전체 (개수 1 이상)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	TArray<FItemStack> GetAllItems() const;

	// 획득 UI 표시 (프로젝트 설정 > Item의 위젯). bWaitForClose면 자동으로 숨기지 않고 닫기 안내를 표시 (닫기는 호출한 쪽이 HideItems)
	UFUNCTION(BlueprintCallable, Category = "Inventory", meta = (WorldContext = "WorldContextObject"))
	UItemAcquireWidget* ShowAcquireUI(const UObject* WorldContextObject, const TArray<FItemStack>& InItems, bool bWaitForClose = false);

public:
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryItemChanged OnItemChanged;

	// AddItems로 실제로 얻은 아이템 (획득 연출 등)
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryItemsAcquired OnItemsAcquired;

	// 능력 해금 아이템을 처음 얻었을 때 (펫 능력 설명·다이얼 갱신 등)
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Ability")
	FOnInventoryAbilityUnlocked OnAbilityUnlocked;

private:
	void LoadFromSave();
	void WriteToSave(UGameProgressSaveGame* SaveGame);
	void SetCount(UItemData* Item, int32 NewCount);

private:
	UPROPERTY(Transient)
	TMap<TObjectPtr<UItemData>, int32> Items;

	UPROPERTY(Transient)
	TObjectPtr<UItemAcquireWidget> AcquireWidget;

	FDelegateHandle CollectSaveHandle;
	FDelegateHandle SaveReplacedHandle;
};
