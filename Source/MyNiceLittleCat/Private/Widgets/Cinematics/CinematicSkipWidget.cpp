#include "Widgets/Cinematics/CinematicSkipWidget.h"
#include "Characters/PlayerMainController.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

void UCinematicSkipWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetIsFocusable(false);
}

void UCinematicSkipWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const APlayerMainController* PC = Cast<APlayerMainController>(GetOwningPlayer());
	const bool bShow = PC && PC->IsCinematicSkipPromptVisible();
	Fade = FMath::FInterpConstantTo(Fade, bShow ? 1.f : 0.f, InDeltaTime, bShow ? 5.f : 7.f);
	const float NextProgress = bShow ? PC->GetCinematicSkipProgress() : 0.f;
	if (Progress > 0.85f && NextProgress == 0.f && !bShow) CompletionGlow = 1.f;
	Progress = NextProgress;
	CompletionGlow = FMath::Max(0.f, CompletionGlow - InDeltaTime * 5.f);
	InvalidateLayoutAndVolatility();
}

int32 UCinematicSkipWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
	const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId,
	const FWidgetStyle& Style, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, Geometry, CullingRect, Elements, LayerId, Style, bParentEnabled);
	if (Fade <= 0.f) return LayerId;
	const FText Hint = NSLOCTEXT("Cinematic", "HoldEToSkip", "E를 길게 눌러 넘어가기");
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 17);
	const FSlateFontInfo KeyFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 17);
	const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D TextSize = Measure->Measure(Hint, Font);
	const FVector2D Size(TextSize.X + 88.f, 58.f);
	const FVector2D Position(Geometry.GetLocalSize().X - Size.X - 32.f, 28.f - (1.f - Fade) * 6.f);
	const FLinearColor Tint = Style.GetColorAndOpacityTint();
	auto Color = [&](FLinearColor Value) { Value.A *= Fade; return Value * Tint; };
	static const FSlateRoundedBoxBrush Panel(FLinearColor::White, 14.f);
	FSlateDrawElement::MakeBox(Elements, ++LayerId, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)),
		&Panel, ESlateDrawEffect::None, Color(FLinearColor(0.025f, 0.03f, 0.045f, 0.78f)));
	FSlateDrawElement::MakeText(Elements, ++LayerId,
		Geometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(Position + FVector2D(16.f, (Size.Y - TextSize.Y) * 0.5f))),
		Hint, Font, ESlateDrawEffect::None, Color(FLinearColor(0.94f, 0.93f, 0.89f)));
	const FVector2D Center = Position + FVector2D(Size.X - 29.f, 29.f);
	auto Ring = [&](float Amount, float Radius, float Thickness, FLinearColor RingColor)
	{
		if (Amount <= 0.f) return;
		TArray<FVector2D> Points;
		const int32 Segments = FMath::Max(1, FMath::CeilToInt(Amount * 96.f));
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float Angle = -HALF_PI + 2.f * PI * Amount * Index / Segments;
			Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FSlateDrawElement::MakeLines(Elements, ++LayerId, Geometry.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, Color(RingColor), true, Thickness);
	};
	Ring(1.f, 19.f, 2.f, FLinearColor(1.f, 1.f, 1.f, 0.18f));
	const float DisplayProgress = CompletionGlow > 0.f ? 1.f : Progress;
	if (DisplayProgress > 0.f)
	{
		Ring(DisplayProgress, 19.f, 6.f, FLinearColor(1.f, 0.78f, 0.38f, 0.12f));
		Ring(DisplayProgress, 19.f, 2.5f, FLinearColor(1.f, 0.83f, 0.48f));
	}
	const FText Key = FText::FromString(TEXT("E"));
	const FVector2D KeySize = Measure->Measure(Key, KeyFont);
	FSlateDrawElement::MakeText(Elements, ++LayerId,
		Geometry.ToPaintGeometry(KeySize, FSlateLayoutTransform(Center - KeySize * 0.5f)),
		Key, KeyFont, ESlateDrawEffect::None, Color(FLinearColor::White));
	return LayerId;
}
