/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 아이템 한 종류의 정보를 담는 데이터 에셋입니다 (DA_Item_Potion, DA_Item_Key_Stage1, DA_Item_Coin 등).
/// 에셋 자체가 아이템의 이름표라서, 인벤토리는 이 에셋을 키로 개수를 셉니다.
/// 능력 해금 아이템은 AbilityTag로 구분하고, 실제 능력 켜기는 해당 담당 쪽에서 인벤토리를 확인합니다. TODO(합의)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Items/ItemTypes.h"
#include "ItemData.generated.h"

class UTexture2D;

UCLASS(BlueprintType)
class MYNICELITTLECAT_API UItemData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	EItemType ItemType = EItemType::Consumable;

	// 가질 수 있는 최대 개수 (0이면 제한 없음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0"))
	int32 MaxCount = 0;

	// 능력 해금 아이템의 능력 태그 (예: Ability.Pet.Magnet)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (EditCondition = "ItemType == EItemType::Ability", EditConditionHides))
	FGameplayTag AbilityTag;

	// 획득 UI에 표시
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bShowInAcquireUI = true;

	// 표시 이름 (비면 에셋 이름)
	UFUNCTION(BlueprintPure, Category = "Item")
	FText GetDisplayNameOrDefault() const { return DisplayName.IsEmpty() ? FText::FromString(GetName()) : DisplayName; }
};
