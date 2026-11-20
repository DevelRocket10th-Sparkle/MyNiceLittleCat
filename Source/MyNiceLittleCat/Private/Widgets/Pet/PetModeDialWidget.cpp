#include "Widgets/Pet/PetModeDialWidget.h"
#include "Pet/Data/PetModeDialData.h"

#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include "Widgets/Pet/PetUIDraw.h"

// 도형 그리기 도우미(FDialMesh 등)는 모드 배지와 같이 쓰려고 PetUIDraw.h로 옮겼어요
using namespace PetUIDraw;

void UPetModeDialWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RebuildIconBrushes();
}

void UPetModeDialWidget::RebuildIconBrushes()
{
    IconBrushes.Reset();
    if (false == IsValid(DialData.Get())) return;

    // Entries와 같은 순서로 만들어서 인덱스로 바로 꺼내 쓸 수 있게 해요
    for (const FPetModeDialEntry& Entry : DialData->Entries)
    {
        FSlateBrush& Brush = IconBrushes.AddDefaulted_GetRef();
        if (IsValid(Entry.Icon.Get()))
            Brush.SetResourceObject(Entry.Icon.Get());
    }
}

void UPetModeDialWidget::UpdateSelectionFromOffset(const FVector2D& _pointerOffset)
{
    InvalidateLayoutAndVolatility(); // 렌더링 다시 진행

	SelectedIndex = INDEX_NONE;

	if (false == IsValid(DialData.Get()) || _pointerOffset.ContainsNaN()) return;

	const int32 EntrySize = DialData->Entries.Num();
	if (0 == EntrySize) return;

	// 중앙(유지) 영역에서는 모드 선택을 안 해요
	const float DeadZoneRadius = FMath::Max(0.f, DialData->DeadZoneRadius);
	if (FMath::Square(DeadZoneRadius) >= _pointerOffset.SizeSquared())
	{
		HoveredIndex = INDEX_NONE; // 가운데로 돌아오면 호버 해제 → 다시 같은 칸에 들어가도 소리가 나게
		return;
	}

	// 마우스 방향을 각도로 변경하는 과정
	// Atan2를 이용해 마우스 방향의 각도를 구해요
	const double AngleRadians = FMath::Atan2(-_pointerOffset.X, -_pointerOffset.Y);

	//(위쪽이 0도, 반시계 방향으로 각도 계산 시작)
	const double AngleDegrees = FMath::Fmod(FMath::RadiansToDegrees(AngleRadians) + 360.0, 360.0);

	// 모드 하나가 차지할 각도 계산
	const double SectorAngle = 360.0 / EntrySize;

	// 첫 항목의 중심이 위쪽을 향하도록 반 구역만큼 보정 (좀 왼쪽으로 돌려주는 것...)
	const double AdjustedAngle = FMath::Fmod(AngleDegrees + SectorAngle * 0.5, 360.0);

	const int32 Index = FMath::FloorToInt(AdjustedAngle / SectorAngle) % EntrySize;

	const bool bLocked = LockedModes.Contains(DialData->Entries[Index].Mode);

	// 칸이 바뀐 순간에만 소리 (매 프레임 호출되니까 같은 칸에서는 안 울리게)
	if (Index != HoveredIndex)
	{
		HoveredIndex = Index;
		PlayDialSound(bLocked ? DialData->LockedSound : DialData->HoverSound);
	}

	// 잠긴 칸은 선택 안 됨 → 그 위에서 놓아도 "유지"와 같은 처리
	SelectedIndex = bLocked ? INDEX_NONE : Index;
}

void UPetModeDialWidget::ResetSelection()
{
	SelectedIndex = INDEX_NONE;
	HoveredIndex = INDEX_NONE; // 다시 열었을 때 첫 칸 호버에도 소리가 나게
    InvalidateLayoutAndVolatility();  // 렌더링 다시 진행
}

void UPetModeDialWidget::SetCurrentMode(EPetMode _mode)
{
    CurrentMode = _mode;
    InvalidateLayoutAndVolatility();  // 체크 배지 위치가 바뀌니까 다시 그려요
}

void UPetModeDialWidget::SetLockedModes(const TSet<EPetMode>& _lockedModes)
{
    LockedModes = _lockedModes;
    InvalidateLayoutAndVolatility();  // 잠긴 칸 색이 바뀌니까 다시 그려요
}

bool UPetModeDialWidget::TryGetSelectedMode(EPetMode& _outMode) const
{
	_outMode = EPetMode::Basic;

	if (false == IsValid(DialData.Get()) || false == DialData->Entries.IsValidIndex(SelectedIndex))
		return false;

	_outMode = DialData->Entries[SelectedIndex].Mode;
	return true;
}

float UPetModeDialWidget::GetDialDiameter() const
{
    return (true == IsValid(DialData.Get())) ? FMath::Max(1.f, DialData->Diameter) : 300.f; //! 300은 그냥 기본값
}

void UPetModeDialWidget::PlayOpenSound() const
{
    if (true == IsValid(DialData.Get()))
        PlayDialSound(DialData->OpenSound);
}

void UPetModeDialWidget::PlayCloseSound() const
{
    if (false == IsValid(DialData.Get())) return;

    // 닫히는 순간 잠긴 칸 위였다면 = 잠긴 모드를 고르려다 실패한 것
    const bool bOnLockedEntry = DialData->Entries.IsValidIndex(HoveredIndex)
        && LockedModes.Contains(DialData->Entries[HoveredIndex].Mode);

    PlayDialSound(bOnLockedEntry ? DialData->LockedSound : DialData->CloseSound);
}

void UPetModeDialWidget::PlayDialSound(USoundBase* _sound) const
{
    // 슬롯이 비어 있으면 조용히 무시
    if (false == IsValid(_sound)) return;

    UGameplayStatics::PlaySound2D(this, _sound);
}

int32 UPetModeDialWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    // BaseLayer     : 기존 위젯
    // BaseLayer + 1 : 크림색 고리
    // BaseLayer + 2 : 선택 안 된 칸들
    // BaseLayer + 3 : 선택된 칸 그림자
    // BaseLayer + 4 : 선택된 칸 (갈색 테두리 + 진한 색)
    // BaseLayer + 5 : 젤리 발바닥, 체크 배지
    // BaseLayer + 6 : 아이콘, 글자
    const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    if (false == IsValid(DialData.Get()) || true == DialData->Entries.IsEmpty())
        return BaseLayer;

    const UPetModeDialData& Data = *DialData;

    const FVector2D Size = AllottedGeometry.GetLocalSize();
    const FVector2f Center(Size * 0.5); // 중심

    // ===== 반지름 계산 =====
    // 위젯 크기 안에 "튀어나온 칸 + 그림자"까지 들어가도록 평소 칸 크기를 잡아요
    const float MaxRadius = FMath::Min(Data.Diameter, static_cast<float>(FMath::Min(Size.X, Size.Y))) * 0.5f;
    const float SectorOuter = MaxRadius - Data.SelectedPopOut - Data.ShadowOffset;
    const float SectorInner = Data.DeadZoneRadius + Data.OutlineThickness; // 가운데 영역과 살짝 띄워요
    if (SectorOuter <= SectorInner) return BaseLayer;

    const float RingRadius = SectorOuter + Data.RingPadding;

    // 위젯 투명도(페이드 인/아웃 등)를 모든 색에 반영해요
    const FLinearColor WidgetTint = InWidgetStyle.GetColorAndOpacityTint();
    const auto ToVertexColor = [&](const FLinearColor& Color) { return (Color * WidgetTint).ToFColor(true); };

    const FColor OutlineColor = ToVertexColor(Data.OutlineColor);
    const FColor HighlightColor = ToVertexColor(Data.HighlightContentColor);

    FDialMesh RingMesh(AllottedGeometry);
    FDialMesh ShadowMesh(AllottedGeometry);
    FDialMesh SectorMesh(AllottedGeometry);
    FDialMesh SelectedMesh(AllottedGeometry);
    FDialMesh DecoMesh(AllottedGeometry);

    // ===== 1. 크림색 고리 (갈색 원 위에 크림색 원을 올려 외곽선처럼 보이게) =====
    RingMesh.AddEllipse(Center, RingRadius + Data.OutlineThickness, RingRadius + Data.OutlineThickness, OutlineColor);
    RingMesh.AddEllipse(Center, RingRadius, RingRadius, ToVertexColor(Data.RingColor));

    // ===== 글자 세팅 =====
    const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Data.LabelFontSize);
    const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

    // 인자로 들어온 위치(글자의 중심)에 글자를 그려요
    const auto DrawLabel = [&](const FText& Text, const FVector2D& Position, const FLinearColor& Color, float Scale)
    {
        FSlateFontInfo ScaledFont = Font;
        ScaledFont.Size = FMath::RoundToFloat(Font.Size * Scale);

        const FVector2D TextSize = FontMeasure->Measure(Text, ScaledFont); // <- 글자의 실제 크기 측정
        FSlateDrawElement::MakeText(
            OutDrawElements, BaseLayer + 6,
            AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(Position - TextSize * 0.5)),
            Text, ScaledFont, ESlateDrawEffect::None, Color * WidgetTint);
    };

    // ===== 2. 칸들 =====
    const int32 EntrySize = Data.Entries.Num();
    const float SectorAngle = 360.f / EntrySize; // 부채꼴 각도
    const float HalfGap = FMath::Min(Data.SectorGapDegrees, SectorAngle * 0.3f) * 0.5f;

    for (int32 Index = 0; Index < EntrySize; ++Index)
    {
        const FPetModeDialEntry& Entry = Data.Entries[Index];
        const bool bSelected = (SelectedIndex == Index);
        const bool bCurrent = (CurrentMode == Entry.Mode);
        const bool bLocked = LockedModes.Contains(Entry.Mode); // 아직 해금 안 된 칸? (선택될 일 없음)

        const float MiddleAngle = Index * SectorAngle;
        const float StartAngle = MiddleAngle - SectorAngle * 0.5f + HalfGap;
        const float EndAngle = MiddleAngle + SectorAngle * 0.5f - HalfGap;

        // 선택된 칸은 바깥으로 튀어나와요
        const float Outer = bSelected ? SectorOuter + Data.SelectedPopOut : SectorOuter;

        if (true == bSelected)
        {
            // 칸을 모든 방향으로 OutlineThickness만큼 키운 모양 (갈색 테두리용)
            // 각도는 (안쪽 반지름 + 모서리) 기준으로 넓혀야 둥근 모서리 중심이 원래 칸과 같아져요
            const float Border = Data.OutlineThickness;
            const float BorderAngle = FMath::RadiansToDegrees(Border / (SectorInner + Data.CornerRadius));
            const float BorderInner = SectorInner - Border;
            const float BorderOuter = Outer + Border;
            const float BorderStart = StartAngle - BorderAngle;
            const float BorderEnd = EndAngle + BorderAngle;
            const float BorderCorner = Data.CornerRadius + Border;

            // 갈색 그림자를 살짝 아래로 깔아서 떠 보이게 해요
            // 테두리와 같은 색이라 아래쪽 테두리만 두꺼워 보일 수 있어서, ShadowOffset이 0이면 그리지 않아요
            if (Data.ShadowOffset > 0.f)
            {
                ShadowMesh.AddRoundedSector(Center + FVector2f(0.f, Data.ShadowOffset), BorderInner, BorderOuter,
                    BorderStart, BorderEnd, BorderCorner, OutlineColor);
            }

            // 갈색 테두리 → 그 위에 진한 모드 색 (옆 칸보다 위 레이어라 테두리가 가려지지 않아요)
            SelectedMesh.AddRoundedSector(Center, BorderInner, BorderOuter, BorderStart, BorderEnd, BorderCorner, OutlineColor);
            SelectedMesh.AddRoundedSector(Center, SectorInner, Outer, StartAngle, EndAngle, Data.CornerRadius, ToVertexColor(Entry.Tint));
        }
        else
        {
            SectorMesh.AddRoundedSector(Center, SectorInner, Outer, StartAngle, EndAngle, Data.CornerRadius,
                ToVertexColor(bLocked ? Data.LockedColor : Entry.IdleColor)); // 잠긴 칸은 회색
        }

        // 현재 모드 칸 바깥 크림색 고리 위에 흰 동그라미 + 갈색 체크 (칸 안의 이름과 겹치지 않게)
        if (true == bCurrent)
        {
            const FVector2f BadgeCenter = MakeDialPoint(Center, MiddleAngle, SectorOuter + Data.RingPadding * 0.5f);
            DecoMesh.AddEllipse(BadgeCenter, 11.5f, 11.5f, OutlineColor);
            DecoMesh.AddEllipse(BadgeCenter, 9.f, 9.f, HighlightColor);
            DecoMesh.AddThickLine(BadgeCenter + FVector2f(-4.5f, 0.f), BadgeCenter + FVector2f(-1.f, 3.5f), 2.2f, OutlineColor);
            DecoMesh.AddThickLine(BadgeCenter + FVector2f(-1.8f, 3.5f), BadgeCenter + FVector2f(4.5f, -3.5f), 2.2f, OutlineColor);
        }

        // ===== 아이콘 + 이름 (위아래로 쌓아서 칸 가운데에) =====
        const float ContentScale = bSelected ? Data.SelectedContentScale : 1.f;
        const FLinearColor ContentColor = bSelected ? Data.HighlightContentColor
                                        : (bLocked ? Data.LockedLabelColor : Entry.LabelColor);

        const FVector2D ContentCenter(MakeDialPoint(Center, MiddleAngle, (SectorInner + Outer) * 0.5f));
        // 잠긴 칸은 아이콘을 숨겨서 어떤 능력인지 모르게 해요
        const bool bHasIcon = (false == bLocked) && IconBrushes.IsValidIndex(Index) && nullptr != IconBrushes[Index].GetResourceObject();
        const float IconDrawSize = bHasIcon ? Data.IconSize * ContentScale : 0.f; // 아이콘이 없으면 이름만 가운데에
        const float LabelHeight = Data.LabelFontSize * ContentScale * 1.4f; // 대략적인 글자 줄 높이
        const float StackTop = ContentCenter.Y - (IconDrawSize + LabelHeight) * 0.5f;

        if (true == bHasIcon)
        {
            const FVector2D IconTopLeft(ContentCenter.X - IconDrawSize * 0.5f, StackTop);
            FSlateDrawElement::MakeBox(OutDrawElements, BaseLayer + 6,
                AllottedGeometry.ToPaintGeometry(FVector2D(IconDrawSize), FSlateLayoutTransform(IconTopLeft)),
                &IconBrushes[Index], ESlateDrawEffect::None, ContentColor * WidgetTint);
        }

        const FText Label = bLocked ? NSLOCTEXT("PetModeDial", "LockedMode", "???") : Entry.DisplayName; // 아직 모르는 능력
        DrawLabel(Label, FVector2D(ContentCenter.X, StackTop + IconDrawSize + LabelHeight * 0.5f), ContentColor, ContentScale);
    }

    // ===== 3. 가운데 젤리 발바닥 =====
    // 시안이 DeadZoneRadius 62 기준이라, 그 비율로 크기를 맞춰요
    const float PawScale = Data.DeadZoneRadius / 62.f;
    const auto PawPoint = [&](float X, float Y) { return Center + FVector2f(X, Y) * PawScale; };

    const bool bHasSelection = Data.Entries.IsValidIndex(SelectedIndex);
    const FColor PadColor = ToVertexColor(bHasSelection ? Data.Entries[SelectedIndex].Tint : Data.CenterPadColor);
    const float Outline = Data.OutlineThickness;

    // 흰 바탕 (갈색 외곽선)
    DecoMesh.AddEllipse(PawPoint(0.f, 8.f), 50.f * PawScale + Outline, 42.f * PawScale + Outline, OutlineColor);
    DecoMesh.AddEllipse(PawPoint(0.f, 8.f), 50.f * PawScale, 42.f * PawScale, HighlightColor);

    // 큰 젤리
    DecoMesh.AddEllipse(PawPoint(0.f, 14.f), 27.f * PawScale, 16.f * PawScale, PadColor);

    // 발가락 젤리 3개 (갈색 외곽선)
    const FVector2f ToeOffsets[] = { FVector2f(-24.f, -30.f), FVector2f(0.f, -38.f), FVector2f(24.f, -30.f) };
    for (const FVector2f& Offset : ToeOffsets)
    {
        DecoMesh.AddEllipse(PawPoint(Offset.X, Offset.Y), 8.f * PawScale + Outline * 0.75f, 9.f * PawScale + Outline * 0.75f, OutlineColor);
        DecoMesh.AddEllipse(PawPoint(Offset.X, Offset.Y), 8.f * PawScale, 9.f * PawScale, PadColor);
    }

    // 큰 젤리 위 글자: 선택 중이면 모드 이름, 아니면 "유지"
    const FText CenterText = bHasSelection ? Data.Entries[SelectedIndex].DisplayName : NSLOCTEXT("PetModeDial", "KeepMode", "유지");
    const FLinearColor CenterTextColor = bHasSelection ? Data.HighlightContentColor : Data.CenterTextColor;
    DrawLabel(CenterText, FVector2D(PawPoint(0.f, 14.f)), CenterTextColor, 1.f);

    // ===== 실제 그리기 요청 (레이어 순서대로) =====
    RingMesh.Draw(OutDrawElements, BaseLayer + 1);
    SectorMesh.Draw(OutDrawElements, BaseLayer + 2);
    ShadowMesh.Draw(OutDrawElements, BaseLayer + 3);
    SelectedMesh.Draw(OutDrawElements, BaseLayer + 4);
    DecoMesh.Draw(OutDrawElements, BaseLayer + 5);

    return BaseLayer + 6; // 엔진한테 해당 레이어까지 사용했다고 알려줘요
}
