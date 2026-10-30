/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 아이템 획득 UI 위젯 구현

#include "WIdgets/Item/ItemAcquireWidget.h"
#include "Items/ItemData.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "TimerManager.h"

FText UItemAcquireWidget::MakeItemLine(const FItemStack& Stack)
{
	if (!Stack.Item)
	{
		return FText::GetEmpty();
	}
	return FText::Format(INVTEXT("{0} ×{1}"), Stack.Item->GetDisplayNameOrDefault(), FText::AsNumber(Stack.Count));
}

void UItemAcquireWidget::ShowItems(const TArray<FItemStack>& Items, float Duration, bool bShowCloseHint)
{
	if (CloseHintText)
	{
		CloseHintText->SetVisibility(bShowCloseHint ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (ItemListText)
	{
		TArray<FText> Lines;
		for (const FItemStack& Stack : Items)
		{
			Lines.Add(MakeItemLine(Stack));
		}
		ItemListText->SetText(FText::Join(INVTEXT("\n"), Lines));
	}

	if (ItemIcon)
	{
		UTexture2D* Icon = Items.Num() > 0 && Items[0].Item ? Items[0].Item->Icon.LoadSynchronous() : nullptr;
		if (Icon)
		{
			ItemIcon->SetBrushFromTexture(Icon);
		}
		ItemIcon->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
	K2_OnItemsShown(Items);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimer);
		if (Duration > 0.f)
		{
			World->GetTimerManager().SetTimer(HideTimer, this, &UItemAcquireWidget::HideItems, Duration, false);
		}
	}
}

void UItemAcquireWidget::HideItems()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimer);
	}

	SetVisibility(ESlateVisibility::Collapsed);
	K2_OnHidden();
}
