// 작성자: 강진구
// 작성일자: 2026-10-22
//
// 메인 HUD: MainWidget 생성 및 현재 조종 중인 플레이어 연결
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MainHUD.generated.h"

class UMainWidget;

class ABossCharacter;

UCLASS()
class MYNICELITTLECAT_API AMainHUD : public AHUD
{
	GENERATED_BODY()

public :

	void ShowBossHealthBar(ABossCharacter* Boss);
	void HideBossHealthBar();

protected:
	virtual void BeginPlay() override;

	// 화면에 띄울 메인 위젯 (WBP_MainWidget)
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSubclassOf<UMainWidget> MainWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMainWidget> MainWidget;

private:
	// 리스폰 등으로 Pawn이 바뀌면 위젯을 새 플레이어에 다시 연결
	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);
};
