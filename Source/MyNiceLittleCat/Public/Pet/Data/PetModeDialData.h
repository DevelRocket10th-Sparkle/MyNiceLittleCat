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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    TObjectPtr<class UTexture2D> Icon = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FLinearColor Tint = FLinearColor::White;
};


//펫 모드 선택 다이얼UI 전체 설정 데이터
UCLASS()
class MYNICELITTLECAT_API UPetModeDialData : public UDataAsset
{
	GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dial", meta = (TitleProperty = "DisplayName"))
    TArray<FPetModeDialEntry> Entries; // 다이얼에 들어갈 항목들

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "1.0"))
    float Diameter = 280.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.0"))
    float DeadZoneRadius = 40.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PanelOpacity = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SelectedPanelOpacity = 0.8f;
};
