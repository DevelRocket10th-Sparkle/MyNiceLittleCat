#include "Widgets/Pet/PetModeDialWidget.h"
#include "Pet/Data/PetModeDialData.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"

void UPetModeDialWidget::UpdateSelectionFromOffset(const FVector2D& _pointerOffset)
{
    InvalidateLayoutAndVolatility(); // 렌더링 다시 진행

	SelectedIndex = INDEX_NONE;

	if (false == IsValid(DialData.Get()) || _pointerOffset.ContainsNaN()) return;

	const int32 EntrySize = DialData->Entries.Num();
	if (0 == EntrySize) return;

	// 마우스가 중앙 영역 안인지 계산
	const float DeadZoneRadius = FMath::Max(0.f, DialData->DeadZoneRadius);
	if (FMath::Square(DeadZoneRadius) >= _pointerOffset.SizeSquared()) return;

	// 중앙에서는 모드 선택을 안 하기 위함
	if (_pointerOffset.SizeSquared() <= FMath::Square(DeadZoneRadius)) return;

	// 마우스 방향을 각도로 변경하는 과정
	// Atan2를 이용해 마우스 방향의 각도를 구해요
	const double AngleRadians = FMath::Atan2(-_pointerOffset.X, -_pointerOffset.Y);

	//(위쪽이 0도, 반시계 방향으로 각도 계산 시작)
	const double AngleDegrees = FMath::Fmod(FMath::RadiansToDegrees(AngleRadians) + 360.0, 360.0);

	// 모드 하나가 차지할 각도 계산
	const double SectorAngle = 360.0 / EntrySize;

	// 첫 항목의 중심이 위쪽을 향하도록 반 구역만큼 보정 (좀 왼쪽으로 돌려주는 것...)
	const double AdjustedAngle = FMath::Fmod(AngleDegrees + SectorAngle * 0.5, 360.0);

	SelectedIndex = FMath::FloorToInt(AdjustedAngle / SectorAngle) % EntrySize;
}

void UPetModeDialWidget::ResetSelection()
{
	SelectedIndex = INDEX_NONE;
    InvalidateLayoutAndVolatility();  // 렌더링 다시 진행
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
    return (true == IsValid(DialData.Get())) ? FMath::Max(1.f, DialData->Diameter) : 280.f; //! 280은 그냥 기본값
}

int32 UPetModeDialWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, 
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    // BaseLayer : 기존 위젯
    // BaseLayer + 1 : 부채꼴 패널
    // BaseLayer + 2 : 글자
    const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    if (false == IsValid(DialData.Get()) || true == DialData->Entries.IsEmpty())
        return BaseLayer;

    const FVector2D Size = AllottedGeometry.GetLocalSize();
    const FVector2D Center = Size * 0.5; // 중심

    // 도넛의 반지름들 구하기
    const float Diameter = FMath::Min(DialData->Diameter, static_cast<float>(FMath::Min(Size.X, Size.Y)));
    const float OuterRadius = Diameter * 0.5f;
    if (0.f >= OuterRadius) return BaseLayer;
    const float InnerRadius = FMath::Clamp(DialData->DeadZoneRadius, 0.f, OuterRadius); // 안쪽 구녕

    const int32 EntrySize = DialData->Entries.Num();
    const float SectorAngle = 360.f / EntrySize; // 부채꼴 각도

    // 부채꼴 사이에 있는 작은 간격을 위한
    const float GapAngle = FMath::Min(2.f, SectorAngle * 0.1f);

    // 모드 이름을 적을 텍스트 관련 세팅
    const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16);
    const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const FLinearColor TextColor(0.12f, 0.16f, 0.15f, 1.f);

    // 해당 위치에 글자를 그려요
    const auto DrawLabel = [&](const FText& Text, const FVector2D& Position)
    {
        const FVector2D TextSize = FontMeasure->Measure(Text, Font); // <- 글자의 실제 크기 측정
        FSlateDrawElement::MakeText(
            OutDrawElements, BaseLayer + 2,
            AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(Position - TextSize * 0.5)), // <- 인자로 들어온 포지션이 글자의 중간
            Text, Font, ESlateDrawEffect::None, TextColor * InWidgetStyle.GetColorAndOpacityTint());
    };

    for (int32 Index = 0; Index < EntrySize; ++Index)
    {
        const FPetModeDialEntry& Entry = DialData->Entries[Index];
        const bool bSelected = (SelectedIndex == Index);

        FLinearColor PanelColor = Entry.Tint; // 데이터에 든 색상 적용
        PanelColor.A *= FMath::Clamp(bSelected ? DialData->SelectedPanelOpacity : DialData->PanelOpacity, 0.f, 1.f);
        PanelColor *= InWidgetStyle.GetColorAndOpacityTint();

        const float MiddleAngle = Index * SectorAngle;

        // 모드 부채꼴을 그려요
        DrawDialSector(OutDrawElements, BaseLayer + 1, AllottedGeometry, Center, InnerRadius, OuterRadius,
            MiddleAngle - SectorAngle * 0.5f + GapAngle * 0.5f, MiddleAngle + SectorAngle * 0.5f - GapAngle * 0.5f, PanelColor);

        // 부채꼴 가운데에 모드의 이름을 그릴거예요
        const float Radians = FMath::DegreesToRadians(MiddleAngle);
        const float LabelRadius = (InnerRadius + OuterRadius) * 0.5f;

        const FVector2D LabelPosition = Center + FVector2D(-FMath::Sin(Radians), -FMath::Cos(Radians)) * LabelRadius;

        DrawLabel(Entry.DisplayName, LabelPosition);
    }

    DrawLabel(NSLOCTEXT("PetModeDial", "KeepMode", "유지"), Center);

    return BaseLayer + 2; // 엔진한테 해당 레이어까지 사용했다고 알려줘요
}

void UPetModeDialWidget::DrawDialSector(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const FVector2D& Center, float InnerRadius, float OuterRadius, float StartDegrees, float EndDegrees, const FLinearColor& Color)
{
    const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    const FSlateResourceHandle Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush);

    TArray<FSlateVertex> Vertices; // 유사 VertextBuffer
    TArray<SlateIndex> Indices; // 유사 IndexBuffer

    // 조각을 4도 간격으로 나눠 원처럼 보이게 해요
    const int32 Steps = FMath::Max(1, FMath::CeilToInt((EndDegrees - StartDegrees) / 4.f));

    const FColor VertexColor = Color.ToFColor(true);

    // 각도와 반지름을 점의 좌표로 바꾸는 람다
    // 중심에서 특정 방향으로 반지름만큼 떨어진 점을 구해요
    const auto MakePoint = [&](float Degrees, float Radius)
    {
        const float Radians = FMath::DegreesToRadians(Degrees); 
        return FVector2f(Center.X - FMath::Sin(Radians) * Radius, Center.Y - FMath::Cos(Radians) * Radius);
    };

    // VertexBuffer에 유사 Vertex를 추가하는 함수
    const auto AddVertex = [&](const FVector2f& Position)
    {
        Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(), // <-- 좌표 변환
                Position, FVector2f(0.5f, 0.5f), VertexColor));
    };

    // 부채꼴을 그릴 Vertex, Index Buffer를 채워요
    for (int32 Step = 0; Step < Steps; ++Step)
    {
        const float A = FMath::Lerp(StartDegrees, EndDegrees, static_cast<float>(Step) / Steps);
        const float B = FMath::Lerp(StartDegrees, EndDegrees, static_cast<float>(Step + 1) / Steps);

        const SlateIndex Base = static_cast<SlateIndex>(Vertices.Num());

        // Vertex 추가
        AddVertex(MakePoint(A, InnerRadius));
        AddVertex(MakePoint(A, OuterRadius));
        AddVertex(MakePoint(B, OuterRadius));
        AddVertex(MakePoint(B, InnerRadius));

        // 
        Indices.Add(Base);
        Indices.Add(Base + 1);
        Indices.Add(Base + 2);

        Indices.Add(Base);
        Indices.Add(Base + 2);
        Indices.Add(Base + 3);
    }

    // 진짜 그려달라고 요청
    FSlateDrawElement::MakeCustomVerts(Elements, Layer, Resource, Vertices, Indices, nullptr, 0, 0);
}
