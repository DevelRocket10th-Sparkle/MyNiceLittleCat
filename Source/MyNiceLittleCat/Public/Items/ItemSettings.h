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
};
