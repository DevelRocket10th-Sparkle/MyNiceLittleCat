// 작성자: 강진구
// 작성일자: 2026-11-16
// 새 게임 시작 시 난이도(쉬움/보통)를 고르는 창입니다.
// 레이아웃은 WBP_DifficultySelect에서 편집하고, 여기서는 버튼 연결·호버 연출·뒤로가기만 처리합니다.
// UTitleScreenWidget이 DifficultySelectClass로 생성합니다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DifficultySelectWidget.generated.h"

class UButton;

DECLARE_DELEGATE_OneParam(FOnDifficultyChosen, bool /*bEasyMode*/);

UCLASS()
class MYNICELITTLECAT_API UDifficultySelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FOnDifficultyChosen OnChosen;
	FSimpleDelegate OnCancelled;

	void FocusDefault();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION() void HandleEasy();
	UFUNCTION() void HandleNormal();
	UFUNCTION() void HandleBack();

	void UpdateCardScale(UButton* Card, float& Scale, float DeltaTime);

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> EasyCard;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> NormalCard;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> BackButton;

	// 카드에 마우스를 올리거나 포커스가 오면 커지는 배율
	UPROPERTY(EditAnywhere, Category = "Difficulty", meta = (ClampMin = "1"))
	float CardHoverScale = 1.05f;

	UPROPERTY(EditAnywhere, Category = "Difficulty", meta = (ClampMin = "0.5", ClampMax = "1"))
	float CardPressedScale = 0.97f;

private:
	float EasyScale = 1.f;
	float NormalScale = 1.f;
};
