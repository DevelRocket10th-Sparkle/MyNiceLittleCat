#include "Widgets/Pet/PetModeBadgeWidget.h"
#include "Widgets/Pet/PetUIDraw.h"
#include "Pet/Data/PetModeDialData.h"
#include "Pet/PetCharacter.h"
#include "Characters/PlayerMainController.h"

#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"

namespace
{
    // 시안(지름 92 배지 + 오른쪽 아래 키캡, 전체 100 상자) 기준 치수
    // 실제로는 위젯 크기에 비례해서 커지고 작아져요
    constexpr float DesignBoxSize = 100.f;
    constexpr float DesignOutlineRadius = 44.f;   // 갈색 바깥 테두리
    constexpr float DesignRingRadius = 40.f;      // 크림색 고리
    constexpr float DesignStrokeOuter = 33.25f;   // 모드 색 원의 갈색 테두리 바깥
    constexpr float DesignColorRadius = 30.75f;   // 모드 색 원
    constexpr float DesignHighlightInner = 28.f;  // 위쪽 흰 하이라이트 띠
    constexpr float DesignHighlightOuter = 32.f;
    constexpr float DesignIconSize = 30.f;
    constexpr float DesignLabelFont = 12.f;
    constexpr float DesignKeyFont = 11.f;
}

using namespace PetUIDraw;

void UPetModeBadgeWidget::NativeConstruct()
{
    Super::NativeConstruct();

    RebuildIconBrushes();
    TryBindToPet();
}

void UPetModeBadgeWidget::NativeDestruct()
{
    UnbindFromPet();
    Super::NativeDestruct();
}

void UPetModeBadgeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // 펫은 플레이어보다 늦게 연결되거나 리스폰으로 바뀔 수 있어서, 연결이 끊겨 있으면 계속 다시 찾아요
    if (false == BoundPet.IsValid())
        TryBindToPet();

    // 통 튀는 연출 진행
    if (PopElapsed >= 0.f)
    {
        PopElapsed += InDeltaTime;
        if (PopElapsed >= PopDuration)
            PopElapsed = -1.f; // 연출 끝

        InvalidateLayoutAndVolatility(); // 크기가 바뀌니 다시 그리기
    }
}

void UPetModeBadgeWidget::TryBindToPet()
{
    APlayerMainController* PC = Cast<APlayerMainController>(GetOwningPlayer());
    APetCharacter* Pet = IsValid(PC) ? PC->GetPartnerPet() : nullptr;
    if (false == IsValid(Pet)) return;

    BoundPet = Pet;
    Pet->OnPetModeChanged.AddUniqueDynamic(this, &UPetModeBadgeWidget::HandlePetModeChanged);

    // 연결하는 순간의 모드로 맞춤 (연출 없이)
    SetDisplayedMode(Pet->GetCurrentMode(), false);
}

void UPetModeBadgeWidget::UnbindFromPet()
{
    if (APetCharacter* Pet = BoundPet.Get())
        Pet->OnPetModeChanged.RemoveDynamic(this, &UPetModeBadgeWidget::HandlePetModeChanged);

    BoundPet.Reset();
}

void UPetModeBadgeWidget::HandlePetModeChanged(EPetMode PreviousMode, EPetMode NewMode)
{
    SetDisplayedMode(NewMode, true);
}

void UPetModeBadgeWidget::SetDisplayedMode(EPetMode _mode, bool _bPlayPop)
{
    const bool bChanged = (DisplayedMode != _mode);
    DisplayedMode = _mode;

    // 실제로 모드가 바뀌었을 때만 통 튀어요
    if (true == _bPlayPop && true == bChanged && PopDuration > 0.f)
        PopElapsed = 0.f;

    InvalidateLayoutAndVolatility();
}

void UPetModeBadgeWidget::RebuildIconBrushes()
{
    IconBrushes.Reset();
    if (false == IsValid(DialData.Get())) return;

    // Entries와 같은 순서로 만들어서 인덱스로 바로 꺼내 쓸 수 있게 해요 (Paint 중에는 만들 수 없어서 미리)
    for (const FPetModeDialEntry& Entry : DialData->Entries)
    {
        FSlateBrush& Brush = IconBrushes.AddDefaulted_GetRef();
        if (IsValid(Entry.Icon.Get()))
            Brush.SetResourceObject(Entry.Icon.Get());
    }
}

int32 UPetModeBadgeWidget::FindEntryIndex(EPetMode _mode) const
{
    if (false == IsValid(DialData.Get())) return INDEX_NONE;

    return DialData->Entries.IndexOfByPredicate([_mode](const FPetModeDialEntry& Entry) { return Entry.Mode == _mode; });
}

float UPetModeBadgeWidget::GetPopScale() const
{
    if (PopElapsed < 0.f || PopDuration <= 0.f) return 1.f;

    // easeOutBack: 0.8에서 시작해 1을 살짝 넘겼다가 1로 돌아와요 (말랑하게 통 튀는 느낌)
    const float T = FMath::Clamp(PopElapsed / PopDuration, 0.f, 1.f);
    constexpr float Overshoot = 2.5f;
    const float Back = 1.f + (Overshoot + 1.f) * FMath::Pow(T - 1.f, 3.f) + Overshoot * FMath::Square(T - 1.f);

    return 0.8f + 0.2f * Back;
}

int32 UPetModeBadgeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    // BaseLayer + 1 : 배지 원들
    // BaseLayer + 2 : 아이콘, 모드 이름
    // BaseLayer + 3 : 키캡 도형
    // BaseLayer + 4 : 키캡 글자
    const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    if (false == IsValid(DialData.Get())) return BaseLayer;

    const UPetModeDialData& Data = *DialData;
    const int32 EntryIndex = FindEntryIndex(DisplayedMode);
    if (false == Data.Entries.IsValidIndex(EntryIndex)) return BaseLayer;

    const FPetModeDialEntry& Entry = Data.Entries[EntryIndex];

    // 위젯 크기에 맞춰 시안 치수를 늘이고 줄여요
    const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
    const float Unit = static_cast<float>(FMath::Min(LocalSize.X, LocalSize.Y)) / DesignBoxSize;
    if (Unit <= 0.f) return BaseLayer;

    const float Pop = GetPopScale();

    // 배지는 왼쪽 위, 키캡은 오른쪽 아래 (키캡이 배지 가장자리에 겹쳐요)
    const FVector2f Center(DesignOutlineRadius * Unit, DesignOutlineRadius * Unit);

    const FLinearColor WidgetTint = InWidgetStyle.GetColorAndOpacityTint();
    const auto ToVertexColor = [&](const FLinearColor& Color) { return (Color * WidgetTint).ToFColor(true); };

    const FColor OutlineColor = ToVertexColor(Data.OutlineColor);

    // ===== 1. 배지 원들 (통 튀는 크기 적용) =====
    FDialMesh BadgeMesh(AllottedGeometry);
    const float R = Unit * Pop;

    BadgeMesh.AddEllipse(Center, DesignOutlineRadius * R, DesignOutlineRadius * R, OutlineColor);                       // 갈색 바깥 테두리
    BadgeMesh.AddEllipse(Center, DesignRingRadius * R, DesignRingRadius * R, ToVertexColor(Data.RingColor));             // 크림색 고리
    BadgeMesh.AddEllipse(Center, DesignStrokeOuter * R, DesignStrokeOuter * R, OutlineColor);                           // 모드 색 원의 갈색 테두리
    BadgeMesh.AddEllipse(Center, DesignColorRadius * R, DesignColorRadius * R, ToVertexColor(Entry.Tint));               // 모드 색 원

    // 위쪽 반원 흰 하이라이트 (동글동글한 버튼 느낌), 오른쪽(-90도)에서 위를 지나 왼쪽(90도)까지
    BadgeMesh.AddArcBand(Center, DesignHighlightInner * R, DesignHighlightOuter * R, -90.f, 90.f,
        ToVertexColor(FLinearColor(1.f, 1.f, 1.f, 0.25f)));

    BadgeMesh.Draw(OutDrawElements, BaseLayer + 1);

    // ===== 2. 아이콘 + 모드 이름 =====
    const FLinearColor ContentColor = Data.HighlightContentColor * WidgetTint;
    const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

    const float IconDrawSize = DesignIconSize * R;
    const bool bHasIcon = IconBrushes.IsValidIndex(EntryIndex) && nullptr != IconBrushes[EntryIndex].GetResourceObject();

    if (true == bHasIcon)
    {
        // 아이콘은 가운데보다 살짝 위, 이름은 그 아래
        const FVector2D IconTopLeft(Center.X - IconDrawSize * 0.5f, Center.Y - IconDrawSize * 0.5f - 6.f * R);
        FSlateDrawElement::MakeBox(OutDrawElements, BaseLayer + 2,
            AllottedGeometry.ToPaintGeometry(FVector2D(IconDrawSize), FSlateLayoutTransform(IconTopLeft)),
            &IconBrushes[EntryIndex], ESlateDrawEffect::None, ContentColor);
    }

    {
        const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FMath::Max(1, FMath::RoundToInt(DesignLabelFont * R)));
        const FVector2D LabelSize = FontMeasure->Measure(Entry.DisplayName, LabelFont);

        // 아이콘이 없으면 이름을 원 한가운데에
        const float LabelCenterY = Center.Y + (true == bHasIcon ? 14.f * R : 0.f);
        const FVector2D LabelTopLeft(Center.X - LabelSize.X * 0.5, LabelCenterY - LabelSize.Y * 0.5);

        FSlateDrawElement::MakeText(OutDrawElements, BaseLayer + 2,
            AllottedGeometry.ToPaintGeometry(LabelSize, FSlateLayoutTransform(LabelTopLeft)),
            Entry.DisplayName, LabelFont, ESlateDrawEffect::None, ContentColor);
    }

    // ===== 3. 오른쪽 아래 키캡 (통 튀지 않고 고정) =====
    if (false == KeyHintText.IsEmpty())
    {
        const FSlateFontInfo KeyFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FMath::Max(1, FMath::RoundToInt(DesignKeyFont * Unit)));
        const FVector2D KeyTextSize = FontMeasure->Measure(KeyHintText, KeyFont);

        // 테두리 2, 아래 테두리만 4로 두꺼워서 눌리는 키처럼 보여요
        const float Border = 2.f * Unit;
        const float BottomBorder = 4.f * Unit;
        const float PadX = 7.f * Unit;
        const float PadY = 1.f * Unit;

        const FVector2f CapSize(
            static_cast<float>(KeyTextSize.X) + (PadX + Border) * 2.f,
            static_cast<float>(KeyTextSize.Y) + PadY * 2.f + Border + BottomBorder);

        const FVector2f CapMax(static_cast<float>(LocalSize.X), static_cast<float>(LocalSize.Y));
        const FVector2f CapMin = CapMax - CapSize;

        FDialMesh KeyMesh(AllottedGeometry);
        KeyMesh.AddRoundedRect(CapMin, CapMax, 7.f * Unit, OutlineColor);                                     // 갈색 테두리
        KeyMesh.AddRoundedRect(CapMin + FVector2f(Border, Border), CapMax - FVector2f(Border, BottomBorder),  // 크림 바탕
            5.f * Unit, ToVertexColor(Data.RingColor));
        KeyMesh.Draw(OutDrawElements, BaseLayer + 3);

        const FVector2D KeyTextTopLeft(CapMin.X + Border + PadX, CapMin.Y + Border + PadY);
        FSlateDrawElement::MakeText(OutDrawElements, BaseLayer + 4,
            AllottedGeometry.ToPaintGeometry(KeyTextSize, FSlateLayoutTransform(KeyTextTopLeft)),
            KeyHintText, KeyFont, ESlateDrawEffect::None, KeyTextColor * WidgetTint);
    }

    return BaseLayer + 4; // 엔진한테 해당 레이어까지 사용했다고 알려줘요
}
