#include "WIdgets/Pet/SpeechBubbleWidget.h"
#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h"

void USpeechBubbleWidget::SetMessage(const FText& _Message)
{
	MessageText->SetText(_Message);
}

void USpeechBubbleWidget::PlayPop()
{
	if (nullptr != PopAnim)
		PlayAnimation(PopAnim);
}
