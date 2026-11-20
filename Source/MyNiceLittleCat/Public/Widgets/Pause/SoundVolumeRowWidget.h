/*
* 작성자: 전선현
* 작성 일자: 2026.11.21.
* 소리 설정의 채널 한 줄 (볼륨 슬라이더, 음소거 버튼, 값 표시)
* - WBP_SoundVolumeRow의 부모: 레이아웃은 WBP, 코드는 이름이 같은 위젯에 기능만 연결
* - 설정 화면(WBP_SoundSettings)에 채널마다 하나씩 배치하고 Channel만 지정 (채널을 늘려도 이 코드는 그대로)
*/
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Frameworks/Sound/AudioVolumeSubsystem.h"
#include "SoundVolumeRowWidget.generated.h"

class USlider;
class UButton;
class UTextBlock;

UCLASS()
class MYNICELITTLECAT_API USoundVolumeRowWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UFUNCTION() void HandleSliderChanged(float Value);
	UFUNCTION() void HandleSliderReleased();
	UFUNCTION() void HandleMuteClicked();

	// 서브시스템 값으로 슬라이더, 값 글자, 음소거 글자 갱신
	void Refresh();
	UAudioVolumeSubsystem* GetVolumeSubsystem() const;

protected:
	// 이 줄이 조절하는 채널: 배치한 인스턴스마다 지정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Settings")
	EAudioVolumeChannel Channel = EAudioVolumeChannel::BGM;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> VolumeSlider;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> MuteButton;

	// 볼륨 % 표시 (없어도 됨)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ValueText;

	// 음소거 버튼 글자 (없어도 됨)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MuteLabel;

	UPROPERTY(EditDefaultsOnly, Category = "Sound Settings")
	FText MuteText = NSLOCTEXT("SoundSettings", "Mute", "음소거");

	UPROPERTY(EditDefaultsOnly, Category = "Sound Settings")
	FText UnmuteText = NSLOCTEXT("SoundSettings", "Unmute", "음소거 해제");

	// 음소거 중일 때 % 대신 보여줄 글자
	UPROPERTY(EditDefaultsOnly, Category = "Sound Settings")
	FText MutedValueText = NSLOCTEXT("SoundSettings", "Muted", "꺼짐");
};
