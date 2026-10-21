/// 작성자: 최요환
/// 작성일자: 2026-10-21
///
/// 바닥 광역 레이저 패턴(ALaserGridPattern)과 공격 순서 데이터(ULaserGridSequenceData)가 함께 쓰는 타입입니다.
/// FLaserGridStep 하나 = 줄 하나 공격(어느 축·어느 줄·어느 쪽에서 쏠지 + 다음 공격까지 시간)

#pragma once

#include "CoreMinimal.h"
#include "LaserGridTypes.generated.h"

UENUM(BlueprintType)
enum class ELaserGridAxis : uint8
{
	// 레이저가 X 방향으로 지나감 (줄은 Y 위치로 구분)
	X	UMETA(DisplayName = "X"),
	// 레이저가 Y 방향으로 지나감 (줄은 X 위치로 구분)
	Y	UMETA(DisplayName = "Y"),
};

UENUM(BlueprintType)
enum class ELaserGridTargetMode : uint8
{
	// 플레이어가 서 있는 줄 (CrossChance 확률로 가로·세로 동시)
	TargetPlayer	UMETA(DisplayName = "Target Player"),
	// 무작위 축·줄
	Random			UMETA(DisplayName = "Random"),
	// 한쪽 끝 줄부터 차례로, 끝까지 가면 축을 바꿈
	Sweep			UMETA(DisplayName = "Sweep"),
	// 플레이어 위치에 항상 가로·세로 동시
	Cross			UMETA(DisplayName = "Cross"),
	// Sequences(공격 순서 데이터)대로 실행
	Sequence		UMETA(DisplayName = "Sequence"),
};

// 공격 단계에서 줄을 고르는 방법
UENUM(BlueprintType)
enum class ELaserGridLaneMode : uint8
{
	// Lane 번호 그대로 (음수면 끝에서부터: -1 = 마지막 줄)
	Fixed			UMETA(DisplayName = "Fixed"),
	// 플레이어가 서 있는 줄
	PlayerLane		UMETA(DisplayName = "Player Lane"),
	// 플레이어 줄 + Lane 칸 (영역 밖이면 이 단계는 건너뜀)
	PlayerOffset	UMETA(DisplayName = "Player Offset"),
	// 무작위 줄
	Random			UMETA(DisplayName = "Random"),
};

// 레이저가 나오는 쪽
UENUM(BlueprintType)
enum class ELaserGridFireSide : uint8
{
	// 축의 - 쪽에서 + 쪽으로
	Negative	UMETA(DisplayName = "Negative"),
	// 축의 + 쪽에서 - 쪽으로
	Positive	UMETA(DisplayName = "Positive"),
	Random		UMETA(DisplayName = "Random"),
};

// 공격 순서의 한 단계 (줄 하나)
USTRUCT(BlueprintType)
struct FLaserGridStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserGrid")
	ELaserGridAxis Axis = ELaserGridAxis::X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserGrid")
	ELaserGridLaneMode LaneMode = ELaserGridLaneMode::PlayerLane;

	// Fixed: 줄 번호(음수면 끝에서부터), PlayerOffset: 플레이어 줄에서 떨어진 칸 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserGrid")
	int32 Lane = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserGrid")
	ELaserGridFireSide FireSide = ELaserGridFireSide::Negative;

	// 이 단계를 시작한 뒤 다음 단계까지 시간 (초). 0이면 다음 단계와 동시
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserGrid", meta = (ClampMin = "0.0", Units = "s"))
	float DelayToNext = 0.5f;

	// 이 단계만 다른 예고 시간 (0 미만이면 패턴 기본값)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserGrid", meta = (Units = "s"))
	float WarningTimeOverride = -1.f;

	// 이 단계만 다른 발사 시간 (0 미만이면 패턴 기본값)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserGrid", meta = (Units = "s"))
	float FireDurationOverride = -1.f;
};
