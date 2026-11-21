// 난이도 선택 창 구현

#include "Widgets/Title/DifficultySelectWidget.h"
#include "Components/Button.h"

void UDifficultySelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetIsFocusable(true);

	EasyCard->OnClicked.AddDynamic(this, &UDifficultySelectWidget::HandleEasy);
	NormalCard->OnClicked.AddDynamic(this, &UDifficultySelectWidget::HandleNormal);
	BackButton->OnClicked.AddDynamic(this, &UDifficultySelectWidget::HandleBack);

	EasyCard->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	NormalCard->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
}

void UDifficultySelectWidget::FocusDefault()
{
	if (EasyCard)
	{
		EasyCard->SetKeyboardFocus();
	}
}

void UDifficultySelectWidget::UpdateCardScale(UButton* Card, float& Scale, float DeltaTime)
{
	if (!Card) return;
	const bool bHot = Card->IsHovered() || Card->HasKeyboardFocus();
	const float Target = Card->IsPressed() ? CardPressedScale : (bHot ? CardHoverScale : 1.f);
	const float NewScale = FMath::FInterpTo(Scale, Target, DeltaTime, 14.f);
	if (!FMath::IsNearlyEqual(NewScale, Scale, 0.0005f))
	{
		Scale = NewScale;
		Card->SetRenderScale(FVector2D(Scale));
	}
}

void UDifficultySelectWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateCardScale(EasyCard, EasyScale, InDeltaTime);
	UpdateCardScale(NormalCard, NormalScale, InDeltaTime);
}

FReply UDifficultySelectWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		HandleBack();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UDifficultySelectWidget::HandleEasy()
{
	OnChosen.ExecuteIfBound(true);
}

void UDifficultySelectWidget::HandleNormal()
{
	OnChosen.ExecuteIfBound(false);
}

void UDifficultySelectWidget::HandleBack()
{
	OnCancelled.ExecuteIfBound();
}
