#include "Widgets/Pet/PetModeDialWidget.h"
#include "Pet/Data/PetModeDialData.h"

#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"

namespace
{
    // 각도(위쪽이 0도, 반시계 방향으로 증가)와 반지름으로 위젯 로컬 좌표를 구해요
    FVector2f MakeDialPoint(const FVector2f& Center, float Degrees, float Radius)
    {
        const float Radians = FMath::DegreesToRadians(Degrees);
        return FVector2f(Center.X - FMath::Sin(Radians) * Radius, Center.Y - FMath::Cos(Radians) * Radius);
    }

    // 다이얼 도형들의 정점/인덱스를 모았다가 한 번에 그리는 버퍼
    // 같은 버퍼 안에서는 나중에 추가한 도형이 위에 그려져요
    // 원/호를 몇 조각으로 나눌지 계산 (둘레 약 3px마다 한 조각 → 큰 원도 각져 보이지 않게)
    int32 CalcArcSegments(float Radius, float Degrees, int32 MinSegments)
    {
        const float ArcLength = FMath::Max(0.f, Radius) * FMath::DegreesToRadians(FMath::Abs(Degrees));
        return FMath::Clamp(FMath::CeilToInt(ArcLength / 3.f), MinSegments, 256);
    }

    // 같은 색의 완전 투명 버전 (가장자리 부드럽게 처리하는 띠의 바깥쪽 색)
    FColor MakeFaded(const FColor& Color)
    {
        FColor Faded = Color;
        Faded.A = 0;
        return Faded;
    }

    // 다이얼 도형들의 정점/인덱스를 모았다가 한 번에 그리는 버퍼
    // 같은 버퍼 안에서는 나중에 추가한 도형이 위에 그려져요
    //
    // MakeCustomVerts로 그린 삼각형은 안티에일리어싱이 안 돼서 가장자리가 계단처럼 보여요
    // 그래서 모든 도형의 바깥 가장자리에 Feather 폭의 띠를 붙여서
    // 안쪽은 원래 색, 바깥쪽은 투명으로 그라데이션되게 해요 (직접 하는 안티에일리어싱)
    struct FDialMesh
    {
        FSlateRenderTransform Transform;
        float Feather = 1.f;            // 가장자리 띠 폭 (로컬 단위, 화면 기준 약 1px)
        TArray<FSlateVertex> Vertices;  // 유사 VertexBuffer
        TArray<SlateIndex> Indices;     // 유사 IndexBuffer

        explicit FDialMesh(const FGeometry& Geometry)
            : Transform(Geometry.GetAccumulatedRenderTransform())
            , Feather(1.f / FMath::Max(0.01f, Geometry.Scale)) {} // UI 배율이 달라도 화면 기준 1px 유지

        int32 AddVertex(const FVector2f& Position, const FColor& Color)
        {
            Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Position, FVector2f(0.5f, 0.5f), Color));
            return Vertices.Num() - 1;
        }

        void AddTriangle(int32 A, int32 B, int32 C)
        {
            Indices.Add(static_cast<SlateIndex>(A));
            Indices.Add(static_cast<SlateIndex>(B));
            Indices.Add(static_cast<SlateIndex>(C));
        }

        // 사각형 (둘레를 따라 A → B → C → D 순서)
        void AddQuad(const FVector2f& A, const FVector2f& B, const FVector2f& C, const FVector2f& D, const FColor& Color)
        {
            const int32 I0 = AddVertex(A, Color);
            const int32 I1 = AddVertex(B, Color);
            const int32 I2 = AddVertex(C, Color);
            const int32 I3 = AddVertex(D, Color);
            AddTriangle(I0, I1, I2);
            AddTriangle(I0, I2, I3);
        }

        // 한 변(P0-P1)의 바깥쪽(Normal 방향)에 Feather 폭의 투명해지는 띠를 붙여요
        void AddFeatherEdge(const FVector2f& P0, const FVector2f& P1, const FVector2f& Normal, const FColor& Color)
        {
            const FColor Faded = MakeFaded(Color);
            const int32 I0 = AddVertex(P0, Color);
            const int32 I1 = AddVertex(P1, Color);
            const int32 I2 = AddVertex(P1 + Normal * Feather, Faded);
            const int32 I3 = AddVertex(P0 + Normal * Feather, Faded);
            AddTriangle(I0, I1, I2);
            AddTriangle(I0, I2, I3);
        }

        // 타원 (X, Y 반지름이 같으면 원), 가장자리 부드럽게 처리 포함
        void AddEllipse(const FVector2f& Center, float RadiusX, float RadiusY, const FColor& Color)
        {
            if (RadiusX <= 0.f || RadiusY <= 0.f) return;

            const int32 Segments = CalcArcSegments(FMath::Max(RadiusX, RadiusY), 360.f, 16);
            const FColor Faded = MakeFaded(Color);
            const int32 CenterIndex = AddVertex(Center, Color);

            for (int32 Step = 0; Step <= Segments; ++Step)
            {
                const float Radians = 2.f * PI * Step / Segments;
                const FVector2f Direction(FMath::Cos(Radians), FMath::Sin(Radians));

                // 실제 가장자리 점(원래 색) + 그 바깥 Feather만큼의 점(투명)
                const int32 EdgeIndex = AddVertex(Center + Direction * FVector2f(RadiusX, RadiusY), Color);
                const int32 FadeIndex = AddVertex(Center + Direction * FVector2f(RadiusX + Feather, RadiusY + Feather), Faded);

                if (Step > 0)
                {
                    const int32 PrevEdge = EdgeIndex - 2;
                    const int32 PrevFade = FadeIndex - 2;

                    AddTriangle(CenterIndex, PrevEdge, EdgeIndex); // 몸통
                    AddTriangle(PrevEdge, PrevFade, FadeIndex);    // 가장자리 띠
                    AddTriangle(PrevEdge, FadeIndex, EdgeIndex);
                }
            }
        }

        // 모서리가 각진 도넛 조각, 안쪽/바깥 호에 가장자리 부드럽게 처리 포함
        void AddArcBand(const FVector2f& Center, float Inner, float Outer, float Start, float End, const FColor& Color)
        {
            if (Outer <= Inner || End <= Start) return;

            // 바깥 호 기준 약 3px마다 한 조각
            const int32 Steps = CalcArcSegments(Outer, End - Start, 1);
            const FColor Faded = MakeFaded(Color);

            // 한 각도마다 점 4개: 안쪽 띠(투명) / 안쪽 / 바깥 / 바깥 띠(투명)
            const float Radii[4] = { FMath::Max(0.f, Inner - Feather), Inner, Outer, Outer + Feather };
            const FColor Colors[4] = { Faded, Color, Color, Faded };

            for (int32 Step = 0; Step <= Steps; ++Step)
            {
                const float Degrees = FMath::Lerp(Start, End, static_cast<float>(Step) / Steps);

                int32 Current[4];
                for (int32 Ring = 0; Ring < 4; ++Ring)
                    Current[Ring] = AddVertex(MakeDialPoint(Center, Degrees, Radii[Ring]), Colors[Ring]);

                if (Step > 0)
                {
                    // 이전 각도의 점 4개와 현재 각도의 점 4개 사이를 사각형 3개(안쪽 띠, 몸통, 바깥 띠)로 채워요
                    for (int32 Ring = 0; Ring < 3; ++Ring)
                    {
                        const int32 Prev0 = Current[Ring] - 4;
                        const int32 Prev1 = Current[Ring + 1] - 4;
                        AddTriangle(Prev0, Prev1, Current[Ring + 1]);
                        AddTriangle(Prev0, Current[Ring + 1], Current[Ring]);
                    }
                }
            }
        }

        // 모서리가 둥근 도넛 조각
        // 각진 몸통 + 양옆 직사각형 + 네 귀퉁이 원을 겹쳐서 둥근 모양을 만들어요
        void AddRoundedSector(const FVector2f& Center, float Inner, float Outer, float Start, float End, float CornerRadius, const FColor& Color)
        {
            const float Corner = FMath::Clamp(CornerRadius, 0.f, (Outer - Inner) * 0.5f);

            // 둥글게 만든 만큼 몸통 각도를 안쪽으로 줄여서 원래 칸 크기를 유지해요
            const float AngleInset = (Corner > 0.f) ? FMath::RadiansToDegrees(Corner / (Inner + Corner)) : 0.f;
            const float CoreStart = Start + AngleInset;
            const float CoreEnd = End - AngleInset;

            if (Corner <= 0.f || CoreStart >= CoreEnd)
            {
                AddArcBand(Center, Inner, Outer, Start, End, Color);
                return;
            }

            // 1. 몸통
            AddArcBand(Center, Inner, Outer, CoreStart, CoreEnd, Color);

            // 2, 3. 옆면 직사각형과 귀퉁이 원
            const auto AddSide = [&](float Degrees, float Sign)
            {
                const float Radians = FMath::DegreesToRadians(Degrees);

                // 각도가 커지는 방향(원의 접선)으로 Corner만큼 밀어낸 직사각형
                const FVector2f Tangent(-FMath::Cos(Radians), FMath::Sin(Radians));
                const FVector2f Offset = Tangent * (Corner * Sign);

                const FVector2f InnerPoint = MakeDialPoint(Center, Degrees, Inner + Corner);
                const FVector2f OuterPoint = MakeDialPoint(Center, Degrees, Outer - Corner);

                AddQuad(InnerPoint, OuterPoint, OuterPoint + Offset, InnerPoint + Offset, Color);
                AddFeatherEdge(InnerPoint + Offset, OuterPoint + Offset, Tangent * Sign, Color); // 옆면 가장자리 부드럽게
                AddEllipse(InnerPoint, Corner, Corner, Color);
                AddEllipse(OuterPoint, Corner, Corner, Color);
            };

            AddSide(CoreStart, -1.f);
            AddSide(CoreEnd, 1.f);
        }

        // 두께 있는 선분 (체크 표시용)
        void AddThickLine(const FVector2f& A, const FVector2f& B, float Thickness, const FColor& Color)
        {
            const FVector2f Direction = (B - A).GetSafeNormal();
            const FVector2f Side(-Direction.Y, Direction.X);
            const FVector2f Normal = Side * (Thickness * 0.5f);
            AddQuad(A + Normal, B + Normal, B - Normal, A - Normal, Color);

            // 긴 두 변 가장자리 부드럽게
            AddFeatherEdge(A + Normal, B + Normal, Side, Color);
            AddFeatherEdge(A - Normal, B - Normal, -Side, Color);
        }

        // 모아둔 도형을 진짜 그려달라고 요청
        void Draw(FSlateWindowElementList& Elements, int32 Layer) const
        {
            if (Indices.IsEmpty()) return;

            const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
            const FSlateResourceHandle Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush);
            FSlateDrawElement::MakeCustomVerts(Elements, Layer, Resource, Vertices, Indices, nullptr, 0, 0);
        }
    };
}

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
	if (FMath::Square(DeadZoneRadius) >= _pointerOffset.SizeSquared()) return;

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

void UPetModeDialWidget::SetCurrentMode(EPetMode _mode)
{
    CurrentMode = _mode;
    InvalidateLayoutAndVolatility();  // 체크 배지 위치가 바뀌니까 다시 그려요
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
            SectorMesh.AddRoundedSector(Center, SectorInner, Outer, StartAngle, EndAngle, Data.CornerRadius, ToVertexColor(Entry.IdleColor));
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
        const FLinearColor ContentColor = bSelected ? Data.HighlightContentColor : Entry.LabelColor;

        const FVector2D ContentCenter(MakeDialPoint(Center, MiddleAngle, (SectorInner + Outer) * 0.5f));
        const bool bHasIcon = IconBrushes.IsValidIndex(Index) && nullptr != IconBrushes[Index].GetResourceObject();
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

        DrawLabel(Entry.DisplayName, FVector2D(ContentCenter.X, StackTop + IconDrawSize + LabelHeight * 0.5f), ContentColor, ContentScale);
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
