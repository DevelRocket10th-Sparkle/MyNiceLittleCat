/// 작성자: 최요환
/// 작성일자: 2026-10-23
///
/// 보스 단일 레이저 패턴(UGA_BossLaserBeam) 전용 실행 설정입니다.
/// PatternSet의 패턴 항목 ExecutionSettings에서 이 클래스를 골라 편집합니다 (근접의 UEnemyMeleePatternSettings와 같은 방식).
/// 레이저 굵기·사거리·피해량·발사 위치(눈·가슴 높이)는 보스 BP에 붙인 ULaserBeamComponent 값을 그대로 쓰고,
/// 여기서는 공격 지점 추적·확정·발사 시간과 발사 중 추적 여부만 정합니다.
/// 보스 BP의 레이저 컴포넌트는 bPlanarAim을 꺼야 위에서 바닥으로 내리쏩니다.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/Datas/EnemyPatternExecutionSettings.h"
#include "BossLaserPatternSettings.generated.h"

class UAnimMontage;
class AAttackWarningDecal;

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "Boss Laser Pattern Settings"))
class MYNICELITTLECAT_API UBossLaserPatternSettings : public UEnemyPatternExecutionSettings
{
	GENERATED_BODY()

public:
	// 보스에 레이저 컴포넌트가 여러 개일 때 사용할 컴포넌트 이름 (None이면 처음 찾은 ULaserBeamComponent)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser")
	FName LaserComponentName;

	// 1단계: 공격 지점이 플레이어 발밑을 따라가는 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Aim", meta = (ClampMin = "0.0", Units = "s"))
	float AimTrackTime = 1.5f;

	// 1단계 공격 지점 이동 속도 (cm/s, 0이면 플레이어 발밑에 바로 붙음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Aim", meta = (ClampMin = "0.0", Units = "cm/s"))
	float AimFollowSpeed = 0.f;

	// 2단계: 공격 지점 확정 후 발사까지 시간 (초). 이 동안 경고가 차오름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Aim", meta = (ClampMin = "0.0", Units = "s"))
	float LockDelay = 1.f;

	// 3단계: 발사 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Fire", meta = (ClampMin = "0.05", Units = "s"))
	float FireDuration = 1.5f;

	// true면 발사 중 공격 지점이 플레이어 발밑 쪽으로 FireTrackSpeed로 이동 (false면 확정 지점 고정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Fire")
	bool bTrackDuringFire = false;

	// 발사 중 공격 지점 이동 속도 (cm/s)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Fire", meta = (ClampMin = "0.0", Units = "cm/s", EditCondition = "bTrackDuringFire"))
	float FireTrackSpeed = 250.f;

	// 공격 지점 경고 데칼 (원형 머티리얼 BP). 비어 있으면 경고 없이 진행
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Warning")
	TSubclassOf<AAttackWarningDecal> WarningDecalClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Warning")
	FLinearColor WarningColor = FLinearColor(1.f, 0.05f, 0.f);

	// 경고 반경 = 빔 반지름 + 이 값 (플레이어 캡슐 반지름 34 기준 = 실제로 맞는 범위)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Warning", meta = (ClampMin = "0.0", Units = "cm"))
	float WarningRadiusPadding = 35.f;

	// 추적·확정 동안 재생할 충전 몽타주 (선택, 끝나도 패턴은 계속)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Animation")
	TObjectPtr<UAnimMontage> ChargeMontage;
};
