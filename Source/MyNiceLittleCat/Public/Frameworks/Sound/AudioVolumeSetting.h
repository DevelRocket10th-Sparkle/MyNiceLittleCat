/*
* 작성자: 전선현
* 작성 일자: 2026.11.19.
* 수정 일자: 2026.11.21. (채널별 사운드 클래스 조회 GetSoundClass)
* 소리 설정 기본값 (프로젝트 설정 → Game → Audio Volume, DefaultGame.ini)
* - BGM, 효과음 사운드 클래스와 사운드 믹스 지정
*/
// Project Settings > Game > Audio Volume
// 옵션 메뉴의 BGM / SFX 볼륨 조절에 쓰는 SoundMix와 SoundClass를 지정합니다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Frameworks/Sound/AudioVolumeSubsystem.h"
#include "AudioVolumeSetting.generated.h"

class USoundMix;
class USoundClass;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Audio Volume"))
class MYNICELITTLECAT_API UAudioVolumeSetting : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// 볼륨 조절용 SoundMix (클래스 오버라이드만 사용하므로 빈 SoundMix면 충분)
	UPROPERTY(Config, EditAnywhere, Category = "Audio Volume")
	TSoftObjectPtr<USoundMix> VolumeMix;

	// BGMSubSystem이 재생하는 곡에 SoundClassOverride로 붙는 클래스
	UPROPERTY(Config, EditAnywhere, Category = "Audio Volume")
	TSoftObjectPtr<USoundClass> BGMSoundClass;

	// 효과음 클래스. 자식 클래스까지 같이 조절됩니다.
	// BGMSoundClass는 이 클래스의 자식이면 안 됩니다 (SFX 볼륨이 BGM에도 걸림)
	UPROPERTY(Config, EditAnywhere, Category = "Audio Volume")
	TSoftObjectPtr<USoundClass> SFXSoundClass;

	// 채널별 사운드 클래스: 채널을 늘리면 여기에 하나 추가
	TSoftObjectPtr<USoundClass> GetSoundClass(EAudioVolumeChannel Channel) const
	{
		return Channel == EAudioVolumeChannel::BGM ? BGMSoundClass : SFXSoundClass;
	}

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
