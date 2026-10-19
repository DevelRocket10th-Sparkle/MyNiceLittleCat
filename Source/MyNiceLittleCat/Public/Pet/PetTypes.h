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
    Follow UMETA(DisplayName = "Follow"),
    MoveTo UMETA(DisplayName = "MoveTo"),
};

// 플레이어가 선택한 펫 능력 모드
UENUM(BlueprintType)
enum class EPetMode : uint8
{
    Basic   UMETA(DisplayName = "Basic"),
    Magnet  UMETA(DisplayName = "Magnet"),
    Reflect UMETA(DisplayName = "Reflect"),
};

// 펫 자성 모드에 쓸 펫의 극
UENUM(BlueprintType)
enum class EPetMagnetPolarity : uint8
{
    N UMETA(DisplayName = "N"),
    S UMETA(DisplayName = "S")
};

// 펫 모드의 사용 입력에 대한
UENUM(BlueprintType)
enum class EPetModeUseInputPhase : uint8
{
    Pressed,
    Released,
    Canceled
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