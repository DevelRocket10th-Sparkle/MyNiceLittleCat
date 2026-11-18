// 작성자: 우진
// 작성일자: 2026-11-18
// Project Settings > Game > BGM 에서 레벨(맵)과 BGM 에셋을 짝지어 등록.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BGMSetting.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "BGM"))
class MYNICELITTLECAT_API UBGMSetting : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	// 레벨 -> 기본 BGM
	// 여기 없는 레벨은 기본곡 없음(무음 또는 Zone/이벤트로만 재생)
	UPROPERTY(Config, EditAnywhere, Category = "BGM")
	TMap<TSoftObjectPtr<UWorld>, TSoftObjectPtr<USoundBase>> LevelBGMs;

	// 곡 전환 시 기본 크로스페이드 시간(초)
	UPROPERTY(Config, EditAnywhere, Category = "BGM", meta = (ClampMin = "0.0"))
	float DefaultFadeTime = 1.5f;

	// BGM 전체 기본 볼륨 
	UPROPERTY(Config, EditAnywhere, Category = "BGM", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BGMVolume = 1.0f;

	// Project Settings에서 Game 카테고리 아래에 보이도록
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
