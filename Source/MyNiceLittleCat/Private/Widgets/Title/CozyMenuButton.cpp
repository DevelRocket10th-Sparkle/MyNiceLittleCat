// 메뉴용 버튼 구현

#include "Widgets/Title/CozyMenuButton.h"
#include "Components/TextBlock.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

namespace
{
	FLinearColor SRGB(uint8 R, uint8 G, uint8 B, uint8 A = 255)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B, A));
	}

	FSlateBrush MakePillBrush(const FLinearColor& Fill, const FLinearColor& Outline, float Width)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = Width;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		return Brush;
	}
}

UCozyMenuButton::UCozyMenuButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FillColor         = SRGB(255, 240, 214);	// 크림
	HoverFillColor    = SRGB(255, 205, 140);	// 살구
	PressedFillColor  = SRGB(240, 170, 110);
	DisabledFillColor = SRGB(205, 198, 188, 170);
	OutlineColor      = SRGB(110, 70, 45);		// 초코
	HoverOutlineColor = SRGB(215, 100, 50);		// 주황
	TextColor         = SRGB(85, 50, 30);
	HoverTextColor    = SRGB(150, 60, 25);
	DisabledTextColor = SRGB(140, 130, 120);

	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	ApplyCozyStyle();
}

#if WITH_EDITOR
const FText UCozyMenuButton::GetPaletteCategory()
{
	return NSLOCTEXT("CozyMenuButton", "Palette", "Common");
}
#endif

void UCozyMenuButton::ApplyCozyStyle()
{
	FButtonStyle Style;
	Style.SetNormal(MakePillBrush(FillColor, OutlineColor, OutlineWidth));
	Style.SetHovered(MakePillBrush(HoverFillColor, HoverOutlineColor, OutlineWidth + 1.f));
	Style.SetPressed(MakePillBrush(PressedFillColor, HoverOutlineColor, OutlineWidth + 1.f));
	Style.SetDisabled(MakePillBrush(DisabledFillColor, DisabledFillColor * 0.8f, OutlineWidth));
	Style.SetNormalPadding(ContentPadding);
	Style.SetPressedPadding(FMargin(ContentPadding.Left, ContentPadding.Top + PressedSink, ContentPadding.Right, ContentPadding.Bottom - PressedSink));
	Style.NormalForeground = FSlateColor(TextColor);
	Style.HoveredForeground = FSlateColor(HoverTextColor);
	Style.PressedForeground = FSlateColor(HoverTextColor);
	Style.DisabledForeground = FSlateColor(DisabledTextColor);

	SetStyle(Style);
	SetBackgroundColor(FLinearColor::White);
	SetColorAndOpacity(FLinearColor::White);
}

void UCozyMenuButton::ApplyLabelStyle(bool bHot, bool bDisabled)
{
	if (!bStyleLabel) return;

	const int8 State = bDisabled ? 2 : (bHot ? 1 : 0);
	UTextBlock* Label = Cast<UTextBlock>(GetContent());
	if (!Label || State == LastLabelState) return;
	LastLabelState = State;

	Label->SetColorAndOpacity(FSlateColor(bDisabled ? DisabledTextColor : (bHot ? HoverTextColor : TextColor)));
}

void UCozyMenuButton::SynchronizeProperties()
{
	ApplyCozyStyle();
	Super::SynchronizeProperties();

	if (bStyleLabel)
	{
		if (UTextBlock* Label = Cast<UTextBlock>(GetContent()))
		{
			FSlateFontInfo Font = Label->GetFont();
			Font.Size = FontSize;
			Font.TypefaceFontName = TEXT("Bold");
			Label->SetFont(Font);
			Label->SetShadowOffset(FVector2D(0.f, 2.f));
			Label->SetShadowColorAndOpacity(SRGB(255, 255, 255, 140));
			Label->SetJustification(ETextJustify::Center);
		}
		LastLabelState = -1;
		ApplyLabelStyle(false, !GetIsEnabled());
	}
}

TSharedRef<SWidget> UCozyMenuButton::RebuildWidget()
{
	TSharedRef<SWidget> Result = Super::RebuildWidget();

	if (!TickHandle.IsValid())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCozyMenuButton::TickAnim));
	}
	return Result;
}

void UCozyMenuButton::ReleaseSlateResources(bool bReleaseChildren)
{
	if (TickHandle.IsValid())
	{
		FTSTicker::RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	Super::ReleaseSlateResources(bReleaseChildren);
}

bool UCozyMenuButton::TickAnim(float DeltaTime)
{
	if (!MyButton.IsValid())
	{
		return true;
	}

	const bool bDisabled = !GetIsEnabled();
	const bool bPressed = !bDisabled && IsPressed();
	const bool bHot = !bDisabled && (IsHovered() || HasKeyboardFocus());

	// 크기: 올리면 커지고, 누르면 작아짐
	const float TargetScale = bPressed ? PressedScale : (bHot ? HoverScale : 1.f);
	const float NewScale = FMath::FInterpTo(CurrentScale, TargetScale, DeltaTime, ScaleSpeed);

	// 흔들흔들
	float TargetAngle = 0.f;
	if (bHot && !bPressed && WobbleAngle > 0.f)
	{
		WobbleTime += DeltaTime;
		TargetAngle = FMath::Sin(WobbleTime * WobbleSpeed) * WobbleAngle;
	}
	else
	{
		WobbleTime = 0.f;
	}
	const float NewAngle = FMath::FInterpTo(CurrentAngle, TargetAngle, DeltaTime, ScaleSpeed);

	if (!FMath::IsNearlyEqual(NewScale, CurrentScale, 0.0005f) || !FMath::IsNearlyEqual(NewAngle, CurrentAngle, 0.01f))
	{
		CurrentScale = NewScale;
		CurrentAngle = NewAngle;
		FWidgetTransform Transform = GetRenderTransform();
		Transform.Scale = FVector2D(CurrentScale);
		Transform.Angle = CurrentAngle;
		SetRenderTransform(Transform);
	}

	ApplyLabelStyle(bHot || bPressed, bDisabled);
	return true;
}
