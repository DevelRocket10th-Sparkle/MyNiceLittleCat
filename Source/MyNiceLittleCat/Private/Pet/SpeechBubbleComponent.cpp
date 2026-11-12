#include "Pet/SpeechBubbleComponent.h"
#include "Widgets/Pet/SpeechBubbleWidget.h"
#include "Pet/PetCharacter.h"
#include "Pet/Data/PetSoundData.h"

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

	// 새로 뜨거나 내용이 바뀔 때만 소리
	// 의도: 튜토리얼이 같은 말을 다시 Say 해서 시간만 늘릴 때 소리가 계속 울리지 않게
	const bool bShouldPlaySound = (false == IsVisible()) || (false == LastMessage.EqualTo(_Message));
	LastMessage = _Message;

	Bubble->SetMessage(_Message);
	Bubble->PlayPop();
	SetVisibility(true);

	if (true == bShouldPlaySound)
		PlaySaySound();

	const float Duration = (_Duration > 0.f) ? _Duration : DefaultDuration;
	GetWorld()->GetTimerManager().SetTimer(HideTimerHandle, this, &USpeechBubbleComponent::HideBubble, Duration, false);
}

void USpeechBubbleComponent::HideBubble()
{
	GetWorld()->GetTimerManager().ClearTimer(HideTimerHandle);
	SetVisibility(false);
}

void USpeechBubbleComponent::PlaySaySound() const
{
	// 펫에 붙은 말풍선이면 펫 사운드 데이터의 말풍선 소리 (2D)
	// 의도: 튜토리얼, 이스터에그 등 어디서 Say 해도 같은 소리가 나도록 말풍선 쪽으로 모음
	//       보스처럼 펫이 아닌 액터의 말풍선은 부르는 쪽(BossSay)에서 따로 재생
	const APetCharacter* Pet = Cast<APetCharacter>(GetOwner());
	if (false == IsValid(Pet)) return;

	if (const UPetSoundData* Sounds = Pet->GetSoundData())
		Pet->PlayPetSound2D(Sounds->SpeechBubbleSound);
}
