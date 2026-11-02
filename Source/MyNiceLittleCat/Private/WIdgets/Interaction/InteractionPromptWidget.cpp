/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 상호작용 안내 UI(프롬프트) 위젯 베이스 구현

#include "WIdgets/Interaction/InteractionPromptWidget.h"
#include "Components/TextBlock.h"

void UInteractionPromptWidget::SetPrompt(const FText& InKeyText, const FText& InActionText)
{
	CurrentKeyText = InKeyText;
	CurrentActionText = InActionText;

	if (KeyText)
	{
		KeyText->SetText(InKeyText);
	}

	if (ActionText)
	{
		ActionText->SetText(InActionText);
		ActionText->SetVisibility(InActionText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	K2_OnPromptUpdated(InKeyText, InActionText);
}

void UInteractionPromptWidget::NotifyShown()
{
	K2_OnPromptShown();
}

void UInteractionPromptWidget::NotifyHidden()
{
	K2_OnPromptHidden();
}

void UInteractionPromptWidget::NotifyPressed()
{
	K2_OnPromptPressed();
}
