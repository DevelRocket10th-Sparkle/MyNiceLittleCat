/*
* 작성자: 전선현
* 작성 일자: 2026.11.19.
* 수정 일자: 2026.11.21. (채널별 값 접근을 한 곳으로, 저장은 조작이 끝날 때만, 사운드 믹스 Push는 월드당 한 번)
* BGM, 효과음 볼륨과 음소거를 저장하고 적용하는 게임 인스턴스 서브시스템
* - 값은 GameUserSettings에 저장, 월드가 바뀔 때마다 사운드 믹스에 다시 적용
* - 채널 추가: EAudioVolumeChannel 값 + Config 값 2개 + VolumeOf/MutedOf + UAudioVolumeSetting::GetSoundClass, 위젯은 행(USoundVolumeRowWidget)을 하나 더 배치
*/
#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumRange.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AudioVolumeSubsystem.generated.h"

UENUM(BlueprintType)
enum class EAudioVolumeChannel : uint8
{
	BGM,
	SFX,

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EAudioVolumeChannel, EAudioVolumeChannel::Count);

UCLASS(Config = GameUserSettings)
class MYNICELITTLECAT_API UAudioVolumeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 볼륨 변경과 즉시 적용 (저장은 SaveIfDirty: 슬라이더를 끄는 동안 매번 ini를 쓰지 않음)
	UFUNCTION(BlueprintCallable, Category = "Audio Volume")
	void SetVolume(EAudioVolumeChannel Channel, float InVolume);

	UFUNCTION(BlueprintPure, Category = "Audio Volume")
	float GetVolume(EAudioVolumeChannel Channel) const;

	// 음소거 변경과 즉시 적용 (저장은 SaveIfDirty)
	UFUNCTION(BlueprintCallable, Category = "Audio Volume")
	void SetMuted(EAudioVolumeChannel Channel, bool bInMuted);

	UFUNCTION(BlueprintPure, Category = "Audio Volume")
	bool IsMuted(EAudioVolumeChannel Channel) const;

	// 바뀐 값이 있으면 GameUserSettings.ini에 저장: 슬라이더를 놓을 때, 음소거를 누를 때, 메뉴를 닫을 때
	UFUNCTION(BlueprintCallable, Category = "Audio Volume")
	void SaveIfDirty();

private:
	void HandleWorldActorsInitialized(const FActorsInitializedParams& Params);
	void ApplyAll();
	void Apply(EAudioVolumeChannel Channel);

	// 채널별 저장 값: 채널을 늘릴 때 여기만 고침
	float& VolumeOf(EAudioVolumeChannel Channel);
	bool& MutedOf(EAudioVolumeChannel Channel);
	float VolumeOf(EAudioVolumeChannel Channel) const { return const_cast<UAudioVolumeSubsystem*>(this)->VolumeOf(Channel); }
	bool MutedOf(EAudioVolumeChannel Channel) const { return const_cast<UAudioVolumeSubsystem*>(this)->MutedOf(Channel); }

	// 저장 키는 그대로 (이전 GameUserSettings.ini 값 유지)
	UPROPERTY(Config) float BGMVolume = 1.f;
	UPROPERTY(Config) float SFXVolume = 1.f;
	UPROPERTY(Config) bool bBGMMuted = false;
	UPROPERTY(Config) bool bSFXMuted = false;

	FDelegateHandle ActorsInitializedHandle;

	// 저장하지 않은 변경이 있음
	bool bDirty = false;

	// 사운드 믹스를 Push한 월드: 같은 월드에서는 클래스 값만 바꿈 (Push를 반복하면 믹스 참조 횟수만 늘어남)
	TWeakObjectPtr<UWorld> MixPushedWorld;
};
