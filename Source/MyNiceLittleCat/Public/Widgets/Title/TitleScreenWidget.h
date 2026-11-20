// 작성자: 강진구
// 작성일자: 2026-10-27
// 
// 타이틀 화면위젯입니다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TitleScreenWidget.generated.h"

class UButton;
class UWidget;
class UDifficultySelectWidget;
/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UTitleScreenWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UFUNCTION() void HandleNewGame();
	UFUNCTION() void HandleContinue();
	UFUNCTION() void HandleExit();
	UFUNCTION() void HandleConfirmYes();
	UFUNCTION() void HandleConfirmNo();

	void ShowDifficultySelect();
	void CloseDifficultySelect();
	void StartNewGame(bool bEasyMode);

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> NewGameButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ContinueButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ExitButton;

	// "세이브를 덮어 씌우시겠습니까?" 확인 창
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> ConfirmPanel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ConfirmYesButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ConfirmNoButton;

	// 새 게임 시작 레벨
	UPROPERTY(EditDefaultsOnly, Category = "Title")
	FName NewGameLevel = TEXT("Lv_Tutorial");

	// 새 게임 난이도 선택 창 (WBP_DifficultySelect)
	UPROPERTY(EditDefaultsOnly, Category = "Title")
	TSubclassOf<UDifficultySelectWidget> DifficultySelectClass;

	UPROPERTY(Transient)
	TObjectPtr<UDifficultySelectWidget> DifficultySelect;
};
