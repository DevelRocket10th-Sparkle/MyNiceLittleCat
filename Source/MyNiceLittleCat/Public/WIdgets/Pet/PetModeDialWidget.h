// 작성자: 우진
// 작성일자: 2026-10-15
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
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

    // 지금 펫이 쓰고 있는 모드 (해당 칸에 체크 배지를 그려요)
    UFUNCTION(BlueprintCallable, Category = "Pet|Dial")
    void SetCurrentMode(EPetMode _mode);

    // 아직 해금 안 된 모드들 (회색 "???" 칸 + 선택 불가). 다이얼을 열 때마다 받아요
    void SetLockedModes(const TSet<EPetMode>& _lockedModes);

    // 후보가 있을 때만 true를 반환하는..
    UFUNCTION(BlueprintPure, Category = "Pet|Dial")
    bool TryGetSelectedMode(EPetMode& _outMode) const;

    // 위젯 크기 반환 함수
    float GetDialDiameter() const;

    // 다이얼 열림 효과음 (컴포넌트가 열 때 불러요)
    void PlayOpenSound() const;

    // 다이얼 닫힘 효과음
    // 잠긴 칸 위에서 닫히면(= 잠긴 모드 선택 시도) 잠김음, 아니면 닫힘음
    void PlayCloseSound() const;

protected:
    // 아이콘 텍스처로 브러시를 미리 만들어 둬요 (Paint 중에는 만들 수 없어서)
    virtual void NativeConstruct() override;

    // (언리얼이 이 위젯을 그릴 때 호출되는 함수, 렌더링 명령을 기록. 실제 렌더링은 Slate가 해요)
    // 전체 배치와 그리기 순서 결졍
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    void RebuildIconBrushes();

    // 비어 있으면 무시하고 2D로 재생
    void PlayDialSound(class USoundBase* _sound) const;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pet|Dial")
    TObjectPtr<class UPetModeDialData> DialData;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Pet|Dial")
    int32 SelectedIndex = INDEX_NONE;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Pet|Dial")
    EPetMode CurrentMode = EPetMode::Basic;

private:
    // Entries와 같은 순서의 아이콘 브러시
    TArray<FSlateBrush> IconBrushes;

    // 잠긴 모드 목록
    TSet<EPetMode> LockedModes;

    // 마우스가 올라가 있는 칸 (잠긴 칸 포함, 가운데 영역이면 INDEX_NONE)
    // 의도: SelectedIndex는 잠긴 칸을 INDEX_NONE으로 지워버려서 "잠긴 칸 호버"를 알 수 없어서 따로 둬요
    int32 HoveredIndex = INDEX_NONE;
};
