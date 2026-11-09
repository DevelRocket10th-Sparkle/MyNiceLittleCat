// 작성자: 우진
// 작성일자: 2026-11-07
//
// 펫 UI(모드 다이얼, 모드 배지)가 함께 쓰는 Slate 도형 그리기 도우미
// 의도: 원/호/둥근 부채꼴을 안티에일리어싱까지 포함해 같은 방식으로 그려서 두 UI의 느낌을 똑같이 맞추기 위함
#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"

namespace PetUIDraw
{
    // 각도(위쪽이 0도, 반시계 방향으로 증가)와 반지름으로 위젯 로컬 좌표를 구해요
    inline FVector2f MakeDialPoint(const FVector2f& Center, float Degrees, float Radius)
    {
        const float Radians = FMath::DegreesToRadians(Degrees);
        return FVector2f(Center.X - FMath::Sin(Radians) * Radius, Center.Y - FMath::Cos(Radians) * Radius);
    }

    // 다이얼 도형들의 정점/인덱스를 모았다가 한 번에 그리는 버퍼
    // 같은 버퍼 안에서는 나중에 추가한 도형이 위에 그려져요
    // 원/호를 몇 조각으로 나눌지 계산 (둘레 약 3px마다 한 조각 → 큰 원도 각져 보이지 않게)
    inline int32 CalcArcSegments(float Radius, float Degrees, int32 MinSegments)
    {
        const float ArcLength = FMath::Max(0.f, Radius) * FMath::DegreesToRadians(FMath::Abs(Degrees));
        return FMath::Clamp(FMath::CeilToInt(ArcLength / 3.f), MinSegments, 256);
    }

    // 같은 색의 완전 투명 버전 (가장자리 부드럽게 처리하는 띠의 바깥쪽 색)
    inline FColor MakeFaded(const FColor& Color)
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

        // 모서리가 둥근 사각형 (키캡 모양용)
        // 가운데 십자 모양 사각형 두 개 + 네 귀퉁이 원을 겹쳐서 둥근 모양을 만들어요
        void AddRoundedRect(const FVector2f& Min, const FVector2f& Max, float Radius, const FColor& Color)
        {
            const FVector2f Size = Max - Min;
            if (Size.X <= 0.f || Size.Y <= 0.f) return;

            const float R = FMath::Clamp(Radius, 0.f, FMath::Min(Size.X, Size.Y) * 0.5f);

            // 가로로 긴 사각형, 세로로 긴 사각형
            AddQuad(FVector2f(Min.X, Min.Y + R), FVector2f(Max.X, Min.Y + R), FVector2f(Max.X, Max.Y - R), FVector2f(Min.X, Max.Y - R), Color);
            AddQuad(FVector2f(Min.X + R, Min.Y), FVector2f(Max.X - R, Min.Y), FVector2f(Max.X - R, Max.Y), FVector2f(Min.X + R, Max.Y), Color);

            // 네 변 가장자리 부드럽게
            AddFeatherEdge(FVector2f(Min.X + R, Min.Y), FVector2f(Max.X - R, Min.Y), FVector2f(0.f, -1.f), Color);
            AddFeatherEdge(FVector2f(Min.X + R, Max.Y), FVector2f(Max.X - R, Max.Y), FVector2f(0.f, 1.f), Color);
            AddFeatherEdge(FVector2f(Min.X, Min.Y + R), FVector2f(Min.X, Max.Y - R), FVector2f(-1.f, 0.f), Color);
            AddFeatherEdge(FVector2f(Max.X, Min.Y + R), FVector2f(Max.X, Max.Y - R), FVector2f(1.f, 0.f), Color);

            // 네 귀퉁이 원
            if (R > 0.f)
            {
                AddEllipse(FVector2f(Min.X + R, Min.Y + R), R, R, Color);
                AddEllipse(FVector2f(Max.X - R, Min.Y + R), R, R, Color);
                AddEllipse(FVector2f(Min.X + R, Max.Y - R), R, R, Color);
                AddEllipse(FVector2f(Max.X - R, Max.Y - R), R, R, Color);
            }
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
