#include "Pet/SpeechBubbleComponent.h"
#include "Widgets/Pet/SpeechBubbleWidget.h"

USpeechBubbleComponent::USpeechBubbleComponent()
{
	// 화면에 항상 정면으로, 위젯 원래 크기대로 그려요
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(true);

	// 펫 머리 위쯤. 펫 크기에 맞춰 BP 에서 조정
	SetRelativeLocation(FVector(0.f, 0.f, 120.f));

	// 처음엔 숨김. Say 할 때만 보임
	SetVisibility(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void USpeechBubbleComponent::Say(const FText& _Message, float _Duration)
{
	if (nullptr == GetUserWidgetObject()) 
		InitWidget();

	USpeechBubbleWidget* Bubble = Cast<USpeechBubbleWidget>(GetUserWidgetObject());
	if (nullptr == Bubble)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PetSpeechBubble] 말풍선 위젯이 없음 (Widget Class 확인): %s"), *_Message.ToString());
		return;
	}

	Bubble->SetMessage(_Message);
	Bubble->PlayPop();
	SetVisibility(true);

	const float Duration = (_Duration > 0.f) ? _Duration : DefaultDuration;
	GetWorld()->GetTimerManager().SetTimer(HideTimerHandle, this, &USpeechBubbleComponent::HideBubble, Duration, false);
}

void USpeechBubbleComponent::HideBubble()
{
	GetWorld()->GetTimerManager().ClearTimer(HideTimerHandle);
	SetVisibility(false);
}
