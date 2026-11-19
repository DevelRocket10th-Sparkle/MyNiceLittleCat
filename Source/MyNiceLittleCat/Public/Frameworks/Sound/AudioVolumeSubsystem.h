/*
* 작성자: 전선현
* 작성 일자: 2026.11.19.
* BGM, 효과음 볼륨과 음소거를 저장하고 적용하는 게임 인스턴스 서브시스템
* - 값은 GameUserSettings에 저장, 월드가 바뀔 때마다 사운드 믹스에 다시 적용
*/
// BGM / SFX 사용자 볼륨 (옵션 메뉴용)
// 값은 GameUserSettings.ini에 저장되고, 월드가 바뀔 때마다 SoundMix로 다시 적용합니다.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AudioVolumeSubsystem.generated.h"

UENUM(BlueprintType)
enum class EAudioVolumeChannel : uint8
{
	BGM,
	SFX,
};

UCLASS(Config = GameUserSettings)
class MYNICELITTLECAT_API UAudioVolumeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Audio Volume")
	void SetVolume(EAudioVolumeChannel Channel, float InVolume);

	UFUNCTION(BlueprintPure, Category = "Audio Volume")
	float GetVolume(EAudioVolumeChannel Channel) const;

	UFUNCTION(BlueprintCallable, Category = "Audio Volume")
	void SetMuted(EAudioVolumeChannel Channel, bool bInMuted);

	UFUNCTION(BlueprintPure, Category = "Audio Volume")
	bool IsMuted(EAudioVolumeChannel Channel) const;

private:
	void HandleWorldActorsInitialized(const FActorsInitializedParams& Params);
	void ApplyAll();
	void Apply(EAudioVolumeChannel Channel);

	UPROPERTY(Config) float BGMVolume = 1.f;
	UPROPERTY(Config) float SFXVolume = 1.f;
	UPROPERTY(Config) bool bBGMMuted = false;
	UPROPERTY(Config) bool bSFXMuted = false;

	FDelegateHandle ActorsInitializedHandle;
};
