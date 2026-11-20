/*
* 작성자: 전선현
* 작성 일자: 2026.11.19.
* 수정 일자: 2026.11.21. (채널별 줄을 USoundVolumeRowWidget으로 분리, 닫을 때 저장만 담당)
* 일시정지 메뉴의 소리 설정 위젯 (WBP_SoundSettings의 부모)
* - 채널마다 WBP_SoundVolumeRow를 배치하고 Channel을 지정: 채널을 늘려도 이 코드는 그대로
* - 화면을 닫을 때 저장하지 않은 값이 있으면 저장
*/
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoundSettingsWidget.generated.h"

UCLASS()
class MYNICELITTLECAT_API USoundSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeDestruct() override;
};
