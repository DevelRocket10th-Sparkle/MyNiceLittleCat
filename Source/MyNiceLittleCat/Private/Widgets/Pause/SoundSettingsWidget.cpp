#include "Widgets/Pause/SoundSettingsWidget.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

void USoundSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BGMSlider->OnValueChanged.AddDynamic(this, &USoundSettingsWidget::HandleBGMSliderChanged);
	SFXSlider->OnValueChanged.AddDynamic(this, &USoundSettingsWidget::HandleSFXSliderChanged);
	BGMMuteButton->OnClicked.AddDynamic(this, &USoundSettingsWidget::HandleBGMMuteClicked);
	SFXMuteButton->OnClicked.AddDynamic(this, &USoundSettingsWidget::HandleSFXMuteClicked);
}

void USoundSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

UAudioVolumeSubsystem* USoundSettingsWidget::GetVolumeSubsystem() const
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	return GI ? GI->GetSubsystem<UAudioVolumeSubsystem>() : nullptr;
}

void USoundSettingsWidget::HandleBGMSliderChanged(float Value) { OnSliderChanged(EAudioVolumeChannel::BGM, Value); }
void USoundSettingsWidget::HandleSFXSliderChanged(float Value) { OnSliderChanged(EAudioVolumeChannel::SFX, Value); }
void USoundSettingsWidget::HandleBGMMuteClicked() { ToggleMute(EAudioVolumeChannel::BGM); }
void USoundSettingsWidget::HandleSFXMuteClicked() { ToggleMute(EAudioVolumeChannel::SFX); }

void USoundSettingsWidget::OnSliderChanged(EAudioVolumeChannel Channel, float Value)
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

void USoundSettingsWidget::ToggleMute(EAudioVolumeChannel Channel)
{
	if (UAudioVolumeSubsystem* Volume = GetVolumeSubsystem())
	{
		Volume->SetMuted(Channel, !Volume->IsMuted(Channel));
	}
	Refresh();
}

void USoundSettingsWidget::Refresh()
{
	RefreshRow(EAudioVolumeChannel::BGM, BGMSlider, BGMValueText, BGMMuteLabel);
	RefreshRow(EAudioVolumeChannel::SFX, SFXSlider, SFXValueText, SFXMuteLabel);
}

void USoundSettingsWidget::RefreshRow(EAudioVolumeChannel Channel, USlider* Slider, UTextBlock* ValueText, UTextBlock* MuteLabel)
{
	const UAudioVolumeSubsystem* Volume = GetVolumeSubsystem();
	if (!Volume)
	{
		return;
	}

	const float Value = Volume->GetVolume(Channel);
	const bool bMuted = Volume->IsMuted(Channel);

	// SetValue는 OnValueChanged를 부르지 않음
	Slider->SetValue(Value);

	if (ValueText)
	{
		ValueText->SetText(bMuted ? MutedValueText : FText::AsPercent(Value));
	}
	if (MuteLabel)
	{
		MuteLabel->SetText(bMuted ? UnmuteText : MuteText);
	}
}
