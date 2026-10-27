// 작성자: 강진구
// 작성일자: 2026-10-27
// 
// 타이틀 화면용 게임모드 입니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TitleGameMode.generated.h"

class UTitleScreenWidget;
/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API ATitleGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ATitleGameMode();

protected:
	virtual void BeginPlay() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Title")
	TSubclassOf<UTitleScreenWidget> TitleWidgetClass;
};
