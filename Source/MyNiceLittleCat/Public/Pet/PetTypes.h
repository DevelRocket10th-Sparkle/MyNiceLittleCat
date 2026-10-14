#pragma once
// 작성자: 우진
// 작성일자: 2026-10-14

#include "CoreMinimal.h"
#include "PetTypes.generated.h"

 // 펫이 수행할 행동 ENUM (not State)
UENUM(BlueprintType)
enum class EPetBehavior : uint8
{
    Idle   UMETA(DisplayName = "Idle"),
    Follow UMETA(DisplayName = "Follow")
};

// 외부에서 펫에게 전달할 펫 초기화 정보
USTRUCT(BlueprintType)
struct MYNICELITTLECAT_API FPetInitializationData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pet")
    TObjectPtr<class APawn> PartnerPlayer = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pet")
    EPetBehavior InitialBehavior = EPetBehavior::Idle;
};