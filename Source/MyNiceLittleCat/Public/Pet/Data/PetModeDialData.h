// 작성자: 우진
// 작성일자: 2026-10-15
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Pet/PetTypes.h"
#include "PetModeDialData.generated.h"


// 다이얼의 항목 하나에 대한
USTRUCT(BlueprintType)
struct MYNICELITTLECAT_API FPetModeDialEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode")
    EPetMode Mode = EPetMode::Basic;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FText DisplayName;

    // 흰색 실루엣 아이콘을 권장해요 (코드에서 색을 입혀요)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    TObjectPtr<class UTexture2D> Icon = nullptr;

    // 선택했을 때 칸을 채우는 진한 색 (가운데 젤리 색도 이 색으로 바뀌어요)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FLinearColor Tint = FLinearColor(FColor(245, 169, 74));

    // 평소(선택 안 됐을 때) 칸을 채우는 파스텔 색
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FLinearColor IdleColor = FLinearColor(FColor(255, 227, 191));

    // 평소 아이콘과 이름 색 (파스텔 위에서 잘 읽히는 같은 계열의 진한 색)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FLinearColor LabelColor = FLinearColor(FColor(122, 74, 18));
};


//펫 모드 선택 다이얼UI 전체 설정 데이터
UCLASS()
class MYNICELITTLECAT_API UPetModeDialData : public UDataAsset
{
	GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dial", meta = (TitleProperty = "DisplayName"))
    TArray<FPetModeDialEntry> Entries; // 다이얼에 들어갈 항목들

    // ===== 배치 =====
    // 위젯 전체 지름 (선택 시 튀어나오는 칸과 그림자까지 포함한 크기)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "1.0"))
    float Diameter = 300.f;

    // 가운데 "유지" 영역 반지름 (젤리 발바닥 크기도 이 값에 맞춰 커지고 작아져요)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.0"))
    float DeadZoneRadius = 62.f;

    // 칸 사이 틈 (도)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.0", ClampMax = "30.0"))
    float SectorGapDegrees = 6.f;

    // 칸 모서리를 둥글게 하는 반지름
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.0"))
    float CornerRadius = 8.f;

    // 칸 바깥 끝과 크림색 고리 끝 사이의 여백
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.0"))
    float RingPadding = 12.f;

    // 갈색 외곽선 두께
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.0"))
    float OutlineThickness = 4.f;

    // ===== 선택 연출 =====
    // 선택된 칸이 바깥으로 튀어나오는 거리
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "0.0"))
    float SelectedPopOut = 16.f;

    // 선택된 칸 아래 깔리는 그림자의 아래쪽 오프셋
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "0.0"))
    float ShadowOffset = 4.f;

    // 선택된 칸의 아이콘/글자 확대 비율
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "1.0"))
    float SelectedContentScale = 1.15f;

    // ===== 아이콘 / 글자 =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content", meta = (ClampMin = "1.0"))
    float IconSize = 28.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content", meta = (ClampMin = "1"))
    int32 LabelFontSize = 14;

    // ===== 공통 색 =====
    // 크림색 고리 바탕
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
    FLinearColor RingColor = FLinearColor(FColor(255, 250, 240));

    // 갈색 외곽선 / 그림자 / 체크 표시
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
    FLinearColor OutlineColor = FLinearColor(FColor(138, 98, 64));

    // 선택된 칸 위 아이콘/글자 색, 젤리 바탕, 체크 배지 바탕
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
    FLinearColor HighlightContentColor = FLinearColor::White;

    // 아무것도 선택 안 했을 때 젤리 색
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
    FLinearColor CenterPadColor = FLinearColor(FColor(255, 179, 190));

    // 아무것도 선택 안 했을 때 가운데 "유지" 글자 색
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
    FLinearColor CenterTextColor = FLinearColor(FColor(138, 58, 74));

    // 아직 해금 안 된 모드 칸 색 (회색빛 베이지)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
    FLinearColor LockedColor = FLinearColor(FColor(214, 208, 200));

    // 아직 해금 안 된 모드 칸의 "???" 글자 색
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
    FLinearColor LockedLabelColor = FLinearColor(FColor(150, 140, 130));
};
