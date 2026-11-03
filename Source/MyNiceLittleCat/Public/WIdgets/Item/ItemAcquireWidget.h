/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 아이템 획득 UI 위젯 베이스입니다 (WBP가 상속해서 모양을 만듦).
/// UInventorySubsystem::AddItems가 ShowItems를 호출하면 일정 시간 표시한 뒤 숨깁니다.
/// BindWidgetOptional: ItemIcon(첫 아이템 아이콘), ItemListText("포션 ×1" 줄 목록), CloseHintText(닫기 안내). 연출은 BP 이벤트 On Items Shown / On Hidden.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/ItemTypes.h"
#include "ItemAcquireWidget.generated.h"

class UImage;
class UTextBlock;

UCLASS(Abstract)
class MYNICELITTLECAT_API UItemAcquireWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 표시 (이미 표시 중이면 내용 교체·시간 다시 시작). Duration <= 0이면 자동으로 숨기지 않음
	// bShowCloseHint: 닫기 안내(CloseHintText, 예: "아무 키나 눌러 계속") 표시
	UFUNCTION(BlueprintCallable, Category = "ItemAcquire")
	void ShowItems(const TArray<FItemStack>& Items, float Duration, bool bShowCloseHint = false);

	UFUNCTION(BlueprintCallable, Category = "ItemAcquire")
	void HideItems();

	// "이름 ×개수" 한 줄
	UFUNCTION(BlueprintPure, Category = "ItemAcquire")
	static FText MakeItemLine(const FItemStack& Stack);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "ItemAcquire", meta = (DisplayName = "On Items Shown"))
	void K2_OnItemsShown(const TArray<FItemStack>& Items);

	UFUNCTION(BlueprintImplementableEvent, Category = "ItemAcquire", meta = (DisplayName = "On Hidden"))
	void K2_OnHidden();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "ItemAcquire", meta = (BindWidgetOptional))
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(BlueprintReadOnly, Category = "ItemAcquire", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemListText;

	// 닫기 안내 (아무 키나 눌러 닫을 때만 보임)
	UPROPERTY(BlueprintReadOnly, Category = "ItemAcquire", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CloseHintText;

private:
	FTimerHandle HideTimer;
};
