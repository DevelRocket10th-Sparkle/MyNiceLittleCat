// 작성자: 우진
// 작성일자: 2026-10-27

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SpeechBubbleWidget.generated.h"

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API USpeechBubbleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMessage(const FText& _Message);
	void PlayPop();

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> MessageText;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<class UWidgetAnimation> PopAnim;
};
