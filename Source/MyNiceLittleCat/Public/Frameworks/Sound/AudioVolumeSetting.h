// Project Settings > Game > Audio Volume
// 옵션 메뉴의 BGM / SFX 볼륨 조절에 쓰는 SoundMix와 SoundClass를 지정합니다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
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

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
