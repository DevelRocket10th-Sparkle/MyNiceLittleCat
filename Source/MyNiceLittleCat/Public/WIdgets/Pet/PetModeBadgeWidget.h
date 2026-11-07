// 작성자: 우진
// 작성일자: 2026-11-07
//
// 메인 HUD 왼쪽 아래에 펫의 현재 모드를 보여주는 동그란 배지
// - 다이얼과 같은 데이터(DA_PetModeDial)의 색·아이콘·이름을 써서 두 UI가 항상 같은 느낌을 유지
// - 펫의 OnPetModeChanged를 받아 자동으로 바뀌고, 바뀌는 순간 통 튀는 연출
// - 오른쪽 아래에 모드 변경 키(LCtrl) 키캡을 겹쳐 표시
// 위젯 크기는 UMG에서 정해요 (권장 100 x 100, 정사각형)
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Pet/PetTypes.h"
#include "PetModeBadgeWidget.generated.h"

UCLASS()
class MYNICELITTLECAT_API UPetModeBadgeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
    // 표시할 모드를 직접 바꿔요 (보통은 펫에 자동 연결되므로 직접 부를 일은 없어요)
    UFUNCTION(BlueprintCallable, Category = "Pet|Badge")
    void SetDisplayedMode(EPetMode _mode, bool _bPlayPop = true);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    // 플레이어 컨트롤러의 펫을 찾아 모드 변경 이벤트에 연결 (펫이 늦게 생기면 Tick에서 다시 시도)
    void TryBindToPet();
    void UnbindFromPet();

    UFUNCTION()
    void HandlePetModeChanged(EPetMode PreviousMode, EPetMode NewMode);

    void RebuildIconBrushes();

    // 지금 표시할 모드의 다이얼 항목 번호 (없으면 INDEX_NONE)
    int32 FindEntryIndex(EPetMode _mode) const;

    // 모드가 바뀐 순간의 통 튀는 크기 (0.8 → 살짝 넘쳤다가 → 1)
    float GetPopScale() const;

protected:
    // 다이얼과 같은 데이터 에셋 (모드별 색·아이콘·이름, 공통 색)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Badge")
    TObjectPtr<class UPetModeDialData> DialData;

    // 오른쪽 아래 키캡에 적을 키 이름
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Badge")
    FText KeyHintText = NSLOCTEXT("PetModeBadge", "KeyHint", "LCtrl");

    // 키캡 글자 색
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Badge")
    FLinearColor KeyTextColor = FLinearColor(FColor(107, 70, 40));

    // 모드가 바뀔 때 통 튀는 연출 시간
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Badge", meta = (ClampMin = "0.0", Units = "s"))
    float PopDuration = 0.35f;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Pet|Badge")
    EPetMode DisplayedMode = EPetMode::Basic;

private:
    TWeakObjectPtr<class APetCharacter> BoundPet;

    // DialData->Entries와 같은 순서의 아이콘 브러시
    TArray<FSlateBrush> IconBrushes;

    // 통 튀는 연출 경과 시간 (음수면 연출 안 하는 중)
    float PopElapsed = -1.f;
};
