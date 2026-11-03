/// 작성자: 최요환
/// 작성일자: 2026-10-25
///
/// 보스 격자 레이저 패턴(UGA_BossLaserGrid) 전용 실행 설정입니다.
/// PatternSet의 패턴 항목 ExecutionSettings에서 이 클래스를 골라 편집합니다 (단일 레이저의 UBossLaserPatternSettings와 같은 방식).
/// 경기장 크기·줄 폭·예고·발사 시간·경고 데칼은 레벨에 배치한 ALaserGridPattern 값을 그대로 쓰고,
/// 여기서는 어느 격자 액터를 쓸지, 어떤 발사 모양을 몇 번 쏠지, 보스를 어디에 세울지만 정합니다.
/// 격자 액터는 보스전 매니저의 PhaseGimmickActors에 등록해야 보스의 UBossGimmickComponent로 찾을 수 있습니다.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/Datas/EnemyPatternExecutionSettings.h"
#include "BossLaserGridPatternSettings.generated.h"

class UAnimMontage;
class ULaserGridSequenceData;

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "Boss Laser Grid Pattern Settings"))
class MYNICELITTLECAT_API UBossLaserGridPatternSettings : public UEnemyPatternExecutionSettings
{
	GENERATED_BODY()

public:
	// 매니저 PhaseGimmickActors에서 격자 액터를 찾을 키 (None이면 현재 페이즈 ID)
	// TODO(합의): 2페이즈 PhaseId 이름은 보스 담당과 확정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaserGrid")
	FName GridActorKey;

	// 1회 발사 모양 후보 (시퀀스 DA 하나 = 1회, 단계의 DelayToNext를 0으로 두면 여러 줄이 동시에 나감)
	// 비어 있으면 격자 액터의 Sequences 목록에서 고름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaserGrid")
	TArray<TObjectPtr<ULaserGridSequenceData>> Sequences;

	// true면 후보를 무작위로 고름 (같은 시퀀스 연속은 피함), false면 목록 순서대로
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaserGrid")
	bool bRandomSequenceOrder = true;

	// 패턴 한 번에 발사하는 횟수 (매번 후보에서 하나씩 골라 실행)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaserGrid", meta = (ClampMin = "1"))
	int32 VolleyCount = 3;

	// 한 발사의 레이저가 모두 끝난 뒤 다음 발사까지 간격 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaserGrid", meta = (ClampMin = "0.0", Units = "s"))
	float VolleyInterval = 0.5f;

	// true면 격자 레이저 한 번 맞을 때 피해량 = 보스 AttackPower × DamageMultiplier (근접 패턴과 같은 공식, 패턴 동안만 적용)
	// false면 격자 액터 레이저의 기본 DamagePerHit 그대로
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaserGrid|Damage")
	bool bUseBossAttackPower = true;


	// true면 패턴 동안 보스가 이동하지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaserGrid")
	bool bLockMovement = true;

	// true면 발사 전에 격자 액터의 BossStandPoint(격자 바깥)로 이동
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaserGrid|Move")
	bool bMoveToStandPoint = true;

	// 지점 도착으로 볼 거리 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaserGrid|Move", meta = (ClampMin = "1.0", Units = "cm", EditCondition = "bMoveToStandPoint"))
	float MoveAcceptanceRadius = 50.f;

	// 이 시간 안에 도착하지 못하면 순간이동 (0이면 제한 없음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaserGrid|Move", meta = (ClampMin = "0.0", Units = "s", EditCondition = "bMoveToStandPoint"))
	float MoveTimeLimit = 3.f;

	// 패턴 시작 시 재생할 몽타주 (선택, 끝나도 패턴은 계속)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaserGrid|Animation")
	TObjectPtr<UAnimMontage> StartMontage;
};
