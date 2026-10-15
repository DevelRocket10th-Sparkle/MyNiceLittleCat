// 작성자: 우진
// 작성일자: 2026-10-15
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Pet/PetTypes.h"
#include "PetModeDialWidget.generated.h"

// 펫 모드 선택 다이얼 위젯
UCLASS()
class MYNICELITTLECAT_API UPetModeDialWidget : public UUserWidget
{
	GENERATED_BODY()
public:
    // 다이얼 중심으로부터의 마우스 위치(인자로 들어오는 오프셋)로 선택 후보를 계산하는 함수
    UFUNCTION(BlueprintCallable, Category = "Pet|Dial")
    void UpdateSelectionFromOffset(const FVector2D& _pointerOffset);

    UFUNCTION(BlueprintCallable, Category = "Pet|Dial")
    void ResetSelection();

    // 후보가 있을 때만 true를 반환하는..
    UFUNCTION(BlueprintPure, Category = "Pet|Dial")
    bool TryGetSelectedMode(EPetMode& _outMode) const;

protected:
    // (언리얼이 이 위젯을 그릴 때 호출되는 함수, 렌더링 명령을 기록. 실제 렌더링은 Slate가 해요) 
    // 전체 배치와 그리기 순서 결졍
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    // 실질적으로 부채꼴을 그리는 함수
   static void DrawDialSector(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const FVector2D& Center,
        float InnerRadius, float OuterRadius, float StartDegrees, float EndDegrees, const FLinearColor& Color);

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pet|Dial")
    TObjectPtr<class UPetModeDialData> DialData;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Pet|Dial")
    int32 SelectedIndex = INDEX_NONE;
};
