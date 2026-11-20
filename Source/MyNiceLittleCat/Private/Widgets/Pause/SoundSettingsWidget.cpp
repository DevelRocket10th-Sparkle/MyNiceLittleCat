#include "Widgets/Pause/SoundSettingsWidget.h"
#include "Frameworks/Sound/AudioVolumeSubsystem.h"
#include "Kismet/GameplayStatics.h"

void USoundSettingsWidget::NativeDestruct()
{
	// 슬라이더를 놓지 않은 채 닫는 경우 등: 남은 변경 저장
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	if (UAudioVolumeSubsystem* Volume = GI ? GI->GetSubsystem<UAudioVolumeSubsystem>() : nullptr)
	{
		Volume->SaveIfDirty();
	}

	Super::NativeDestruct();
}
