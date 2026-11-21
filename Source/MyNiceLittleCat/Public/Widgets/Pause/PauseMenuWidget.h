// 일시정지 메뉴 (Esc / 패드 Start). 동구 요청으로 추가
// 열리면 게임 월드를 멈추고(SetGamePaused) UI 입력만 받습니다. 닫으면 원래대로.
// WBP에 ResumeButton / QuitButton (UButton 또는 CozyMenuButton) 이 있어야 합니다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UButton;

UCLASS()
class MYNICELITTLECAT_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 화면에 띄우고 게임 정지
	void Open();

	// 닫고 게임 재개
	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void Close();

	bool IsOpen() const { return bOpen; }

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION() void HandleResume();
	UFUNCTION() void HandleQuit();

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ResumeButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> QuitButton;

	// 비어 있으면 게임 종료, 레벨 이름을 넣으면 그 레벨로 이동 (예: Lv_TitleScreen)
	UPROPERTY(EditDefaultsOnly, Category = "Pause Menu")
	FName QuitToLevel;

	// 메뉴를 닫는 키
	UPROPERTY(EditDefaultsOnly, Category = "Pause Menu")
	TArray<FKey> CloseKeys = { EKeys::Escape, EKeys::Gamepad_Special_Right };

private:
	bool bOpen = false;
};
