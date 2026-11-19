// 일시정지 메뉴 안의 소리 설정 (BGM / SFX 볼륨, 음소거)
// 배치와 스타일은 WBP_SoundSettings에서 하고, 여기서는 이름이 같은 위젯에 기능만 연결합니다.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Frameworks/Sound/AudioVolumeSubsystem.h"
#include "SoundSettingsWidget.generated.h"

class USlider;
class UButton;
class UTextBlock;

UCLASS()
class MYNICELITTLECAT_API USoundSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UFUNCTION() void HandleBGMSliderChanged(float Value);
	UFUNCTION() void HandleSFXSliderChanged(float Value);
	UFUNCTION() void HandleBGMMuteClicked();
	UFUNCTION() void HandleSFXMuteClicked();

	void OnSliderChanged(EAudioVolumeChannel Channel, float Value);
	void ToggleMute(EAudioVolumeChannel Channel);
	void Refresh();
	void RefreshRow(EAudioVolumeChannel Channel, USlider* Slider, UTextBlock* ValueText, UTextBlock* MuteLabel);
	UAudioVolumeSubsystem* GetVolumeSubsystem() const;

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> BGMSlider;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> SFXSlider;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> BGMMuteButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> SFXMuteButton;

	// 볼륨 % 표시 (없어도 됨)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> BGMValueText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SFXValueText;

	// 음소거 버튼 글자 (없어도 됨)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> BGMMuteLabel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SFXMuteLabel;

	UPROPERTY(EditDefaultsOnly, Category = "Sound Settings")
	FText MuteText = NSLOCTEXT("SoundSettings", "Mute", "음소거");

	UPROPERTY(EditDefaultsOnly, Category = "Sound Settings")
	FText UnmuteText = NSLOCTEXT("SoundSettings", "Unmute", "음소거 해제");

	// 음소거 중일 때 % 대신 보여줄 글자
	UPROPERTY(EditDefaultsOnly, Category = "Sound Settings")
	FText MutedValueText = NSLOCTEXT("SoundSettings", "Muted", "꺼짐");
};
