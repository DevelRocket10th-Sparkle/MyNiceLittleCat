/// 작성자: 최요환
/// 작성일자: 2026-10-18
///
/// 자성 기믹 공용 타입입니다.
/// 극성(EMagneticPolarity), 자기장 범위 모양, 거리 감쇠 방식과
/// 자력원(펫 자석 능력 등)이 UMagneticSubsystem에 넘기는 정보 구조체(FMagneticSourceInfo)를 정의합니다.
/// 자력원 쪽(펫)과 자력을 받는 쪽(UMagneticComponent)이 이 헤더를 함께 사용합니다.

#pragma once

#include "CoreMinimal.h"
#include "MagneticTypes.generated.h"

// 극성 (None은 자력에 반응하지 않음)
UENUM(BlueprintType)
enum class EMagneticPolarity : uint8
{
	None	UMETA(DisplayName = "None"),
	N		UMETA(DisplayName = "N"),
	S		UMETA(DisplayName = "S"),
};

// 자기장 범위 모양
UENUM(BlueprintType)
enum class EMagneticFieldShape : uint8
{
	// 조준 방향 일직선 (반경 LineRadius인 빔)
	Line	UMETA(DisplayName = "Line"),
	// 조준 방향 기준 부채꼴 (반각 ConeHalfAngle)
	Cone	UMETA(DisplayName = "Cone"),
};

// 거리에 따른 힘 감쇠
// TODO(합의): 거리 감쇠 기획 미정 (기본값 None)
UENUM(BlueprintType)
enum class EMagneticFalloff : uint8
{
	// 범위 안에서는 거리와 관계없이 같은 세기
	None	UMETA(DisplayName = "None"),
	// 자력원에서 멀수록 선형으로 약해짐 (사거리 끝에서 0)
	Linear	UMETA(DisplayName = "Linear"),
};

// 자력원 정보. 자력원이 능력을 쓰는 동안 매 틱 UMagneticSubsystem::UpdateSource로 갱신
// TODO(합의): 펫 담당과 전달 항목 확정 필요
USTRUCT(BlueprintType)
struct FMagneticSourceInfo
{
	GENERATED_BODY()

	// 자력원 위치 (펫 위치)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnetic")
	FVector Location = FVector::ZeroVector;

	// 조준 방향 (펫 → 마우스 커서). 정규화하지 않아도 됨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnetic")
	FVector AimDirection = FVector::ForwardVector;

	// 자력원 극성. 같은 극성은 밀어내고 다른 극성은 끌어당김 (None이면 작용 안 함)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnetic")
	EMagneticPolarity Polarity = EMagneticPolarity::None;

	// 힘 세기. 받는 쪽 bIgnoreMass가 true면 가속도(cm/s², 중력 980)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnetic", meta = (ClampMin = "0.0"))
	float Strength = 2000.f;

	// 사거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnetic", meta = (ClampMin = "0.0"))
	float Range = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnetic")
	EMagneticFieldShape Shape = EMagneticFieldShape::Line;

	// Line: 빔 반경 (조준 축에서 이 거리 안이면 범위 안)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnetic", meta = (ClampMin = "0.0", EditCondition = "Shape == EMagneticFieldShape::Line"))
	float LineRadius = 100.f;

	// Cone: 부채꼴 반각 (도)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnetic", meta = (ClampMin = "0.0", ClampMax = "180.0", EditCondition = "Shape == EMagneticFieldShape::Cone"))
	float ConeHalfAngle = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnetic")
	EMagneticFalloff Falloff = EMagneticFalloff::None;

	// true면 자력원과 대상 사이에 벽(LineOfSightObjectTypes)이 있을 때 작용 안 함
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnetic")
	bool bRequireLineOfSight = true;

	// true면 높이(Z)를 무시하고 XY 평면 기준으로 범위 판정·힘 방향 계산 (쿼터뷰에서 물체가 위로 뜨지 않게)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnetic")
	bool bPlanar = true;

	// true면 자력원 높이 기준 위아래 HeightLimit 밖의 대상은 범위 밖 (Line·Cone 공통)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnetic")
	bool bLimitHeight = true;

	// 자력원(캐릭터 중심) Z에서 위아래로 허용하는 높이 차이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnetic", meta = (ClampMin = "0.0", EditCondition = "bLimitHeight"))
	float HeightLimit = 100.f;
};
