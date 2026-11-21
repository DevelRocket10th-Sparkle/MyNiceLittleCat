#include "Widgets/Pause/SoundVolumeRowWidget.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

void USoundVolumeRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (LabelText && !ChannelLabel.IsEmpty())
	{
		LabelText->SetText(ChannelLabel);
	}
}

void USoundVolumeRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	VolumeSlider->OnValueChanged.AddDynamic(this, &USoundVolumeRowWidget::HandleSliderChanged);
	// 마우스·패드로 끄던 슬라이더를 놓을 때만 저장
	VolumeSlider->OnMouseCaptureEnd.AddDynamic(this, &USoundVolumeRowWidget::HandleSliderReleased);
	VolumeSlider->OnControllerCaptureEnd.AddDynamic(this, &USoundVolumeRowWidget::HandleSliderReleased);
	MuteButton->OnClicked.AddDynamic(this, &USoundVolumeRowWidget::HandleMuteClicked);
}

void USoundVolumeRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

UAudioVolumeSubsystem* USoundVolumeRowWidget::GetVolumeSubsystem() const
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	return GI ? GI->GetSubsystem<UAudioVolumeSubsystem>() : nullptr;
}

void USoundVolumeRowWidget::HandleSliderChanged(float Value)
{
	if (UAudioVolumeSubsystem* Volume = GetVolumeSubsystem())
	{
		Volume->SetVolume(Channel, Value);
		// 음소거 상태에서 슬라이더를 움직이면 음소거 해제
		if (Volume->IsMuted(Channel))
		{
			Volume->SetMuted(Channel, false);
		}
	}
	Refresh();
}

void USoundVolumeRowWidget::HandleSliderReleased()
{
	if (UAudioVolumeSubsystem* Volume = GetVolumeSubsystem())
	{
		Volume->SaveIfDirty();
	}
}

void USoundVolumeRowWidget::HandleMuteClicked()
{
	if (UAudioVolumeSubsystem* Volume = GetVolumeSubsystem())
	{
		Volume->SetMuted(Channel, !Volume->IsMuted(Channel));
		Volume->SaveIfDirty();
	}
	Refresh();
}

void USoundVolumeRowWidget::Refresh()
{
	const UAudioVolumeSubsystem* Volume = GetVolumeSubsystem();
	if (!Volume)
	{
		return;
	}

	const float Value = Volume->GetVolume(Channel);
	const bool bMuted = Volume->IsMuted(Channel);

	// SetValue는 OnValueChanged를 부르지 않음
	VolumeSlider->SetValue(Value);

	if (ValueText)
	{
		ValueText->SetText(bMuted ? MutedValueText : FText::AsPercent(Value));
	}
	if (MuteLabel)
	{
		MuteLabel->SetText(bMuted ? UnmuteText : MuteText);
	}
}
