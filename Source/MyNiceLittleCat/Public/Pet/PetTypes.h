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
    AbilityMoveTo UMETA(DisplayName = "AbilityMoveTo"), // 능력이 요청하고 결과를 기다리는 이동..
    RecoverFollow UMETA(DisplayName = "Recover Follow")
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

// 애니메이션을 선택할 이동 상황
UENUM(BlueprintType)
enum class EPetMovementStyle : uint8
{
    Normal           UMETA(DisplayName = "Normal"),
    Magnet           UMETA(DisplayName = "Magnet"),
    ReflectPlacement UMETA(DisplayName = "Reflect Placement"),
    FollowRecovery   UMETA(DisplayName = "Follow Recovery")
    ReflectPlacement UMETA(DisplayName = "Reflect Placement"),

    Fly              UMETA(DisplayName = "Fly") // 튜토리얼 등에서 펫이 공중으로 날아갈 때
};