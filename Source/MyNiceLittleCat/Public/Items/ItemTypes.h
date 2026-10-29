/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 아이템 공용 타입 (종류 enum, 아이템 + 개수 묶음)
/// 보물 상자 내용물, 인벤토리 조회, 획득 UI가 함께 사용합니다.

#pragma once

#include "CoreMinimal.h"
#include "ItemTypes.generated.h"

class UItemData;

UENUM(BlueprintType)
enum class EItemType : uint8
{
	// 사용하면 줄어드는 아이템 (포션 등)
	Consumable,
	// 잠긴 문·상자 등을 여는 열쇠 (아이템 하나 = 열쇠 하나)
	Key,
	// 재화
	Currency,
	// 능력 해금 (AbilityTag로 구분)
	Ability
};

// 아이템과 개수
USTRUCT(BlueprintType)
struct FItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TObjectPtr<UItemData> Item;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "1"))
	int32 Count = 1;

	FItemStack() = default;
	FItemStack(UItemData* InItem, int32 InCount) : Item(InItem), Count(InCount) {}
};
