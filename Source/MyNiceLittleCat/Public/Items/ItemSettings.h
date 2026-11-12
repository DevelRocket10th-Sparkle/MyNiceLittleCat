/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 아이템 공통 설정 (프로젝트 설정 > 게임 > Item, DefaultGame.ini에 저장)
/// 획득 UI 위젯과 표시 시간을 한 곳에서 정합니다 (보물 상자·드랍 등 획득 경로와 상관없이 같은 UI).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ItemSettings.generated.h"

class UItemAcquireWidget;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Item"))
class MYNICELITTLECAT_API UItemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// 획득 UI 위젯 (비면 UI 없이 로그만)
	UPROPERTY(Config, EditAnywhere, Category = "AcquireUI")
	TSoftClassPtr<UItemAcquireWidget> AcquireWidgetClass;

	// 획득 UI 표시 시간
	UPROPERTY(Config, EditAnywhere, Category = "AcquireUI", meta = (ClampMin = "0.5", Units = "s"))
	float AcquireDisplayTime = 2.5f;

	UPROPERTY(Config, EditAnywhere, Category = "AcquireUI")
	int32 AcquireWidgetZOrder = 50;

	// 획득 UI가 뜰 때 기본 효과음 (아이템 AcquireSound가 비었을 때). 일시정지 중에도 들리는 2D UI 소리로 재생
	UPROPERTY(Config, EditAnywhere, Category = "AcquireUI", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	FSoftObjectPath DefaultAcquireSound;

	UPROPERTY(Config, EditAnywhere, Category = "AcquireUI", meta = (ClampMin = "0.0"))
	float AcquireSoundVolume = 1.f;

	// 테스트용: 능력 아이템 없이도 모든 능력이 열린 것으로 (UInventorySubsystem::HasAbility가 항상 true). 실제 게임에서는 끔
	UPROPERTY(Config, EditAnywhere, Category = "Test")
	bool bTestUnlockAllAbilities = false;
};
