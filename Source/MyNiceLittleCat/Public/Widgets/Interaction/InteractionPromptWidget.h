/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 상호작용 안내 UI(프롬프트) 위젯 베이스입니다.
/// 플레이어의 InteractionComponent가 포커스 대상 위에 이 위젯 하나를 띄우고 내용을 갱신합니다.
/// WBP에서 이름이 KeyText·ActionText인 TextBlock을 두면 자동으로 채워집니다 (둘 다 선택).
/// 모양 변화·애니메이션은 On Prompt Updated / On Prompt Shown / On Prompt Hidden / On Prompt Pressed 이벤트에서 처리합니다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractionPromptWidget.generated.h"

class UTextBlock;

UCLASS(Abstract)
class MYNICELITTLECAT_API UInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 표시 내용 갱신 (키 이름, 동작 텍스트). 동작 텍스트가 비면 ActionText를 숨김
	UFUNCTION(BlueprintCallable, Category = "Interaction Prompt")
	void SetPrompt(const FText& InKeyText, const FText& InActionText);

	// 포커스 대상에 표시 시작 / 숨김 직전 / 상호작용 입력 시
	void NotifyShown();
	void NotifyHidden();
	void NotifyPressed();

	UFUNCTION(BlueprintPure, Category = "Interaction Prompt")
	FText GetKeyDisplayText() const { return CurrentKeyText; }

	UFUNCTION(BlueprintPure, Category = "Interaction Prompt")
	FText GetActionDisplayText() const { return CurrentActionText; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction Prompt", meta = (DisplayName = "On Prompt Updated"))
	void K2_OnPromptUpdated(const FText& InKeyText, const FText& InActionText);

	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction Prompt", meta = (DisplayName = "On Prompt Shown"))
	void K2_OnPromptShown();

	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction Prompt", meta = (DisplayName = "On Prompt Hidden"))
	void K2_OnPromptHidden();

	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction Prompt", meta = (DisplayName = "On Prompt Pressed"))
	void K2_OnPromptPressed();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Interaction Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KeyText;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActionText;

private:
	FText CurrentKeyText;
	FText CurrentActionText;
};
