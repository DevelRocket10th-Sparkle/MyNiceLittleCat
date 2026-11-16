/// 작성자: 최요환
/// 작성일자: 2026-10-25
///
/// 레벨에 배치한 격자 레이저(ALaserGridPattern)로 VolleyCount번 발사하는 보스 패턴 GA입니다.
/// 보스 PatternSet에 패턴으로 등록하면 기존 BT 공격 Task(UEnemyPatternComponent)가 근접·단일 레이저 패턴처럼 실행합니다.
/// 흐름: GetActiveExecution으로 설정(UBossLaserGridPatternSettings) 받기 → 보스의 UBossGimmickComponent에서 격자 액터 찾기
///       → CommitAbility → (옵션) 이동/회전 잠금 (보스를 지점으로 옮기지 않음, 이동은 보스 쪽 이동 패턴 담당)
///       → 발사 모양(시퀀스 DA)을 골라 PlaySequenceOnce → 끝나면 VolleyInterval 뒤 다음 발사 … VolleyCount번 → 정상 종료
/// 레이저 교대 페이즈에서는 격자 차례일 때만 실행되고(CanActivateAbility), 정상 종료 시 순번을 단일 레이저로 넘깁니다.
/// 취소(페이즈 전환·사망·리셋)되면 StopPattern에서 격자 패턴을 멈춰 남은 경고·레이저를 정리하고, 잠금은 부모(UEnemyPatternAbility)가 해제합니다.
/// 피해는 격자 액터의 레이저 컴포넌트가 처리합니다 (플레이어는 ApplyPlayerHit → 플레이어 피해 GE).
/// bUseBossAttackPower면 피해량 = 보스 AttackPower × DamageMultiplier (근접 패턴과 같은 공식)를 이번 실행에만 넘깁니다.
/// MaxExecutionSeconds는 전체 발사 시간보다 길게 잡아야 합니다.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/GAS/Abilities/EnemyPatternAbility.h"
#include "GA_BossLaserGrid.generated.h"

class ALaserGridPattern;
class ULaserGridSequenceData;
class UBossLaserGridPatternSettings;

UCLASS()
class MYNICELITTLECAT_API UGA_BossLaserGrid : public UEnemyPatternAbility
{
	GENERATED_BODY()

public:
	// 레이저 교대 페이즈에서는 격자 차례일 때만 실행
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	// 설정·격자 액터 검사, 격자가 이미 돌고 있으면 실패 (CommitAbility 전, 실패하면 부모가 취소 종료)
	virtual bool CanStartPattern() const override;

	// 이동·회전 처리, 시작 몽타주, 완료 구독 후 첫 발사 (기존 ActivateAbility의 CommitAbility 이후 부분)
	virtual bool StartPattern() override;

	// 완료 구독 해제, 이 GA가 시작한 격자 시퀀스 정지 (잠금 해제와 요청 정리는 부모가 처리)
	virtual void StopPattern(bool bWasCancelled) override;

	// 다음 발사 모양을 골라 실행
	UFUNCTION()
	void StartNextVolley();

	// 한 발사의 마지막 레이저까지 끝남
	UFUNCTION()
	void HandleSequenceFinished(ULaserGridSequenceData* Sequence);

	// 보스의 UBossGimmickComponent에서 격자 액터 찾기 (키가 None이면 현재 페이즈 ID)
	ALaserGridPattern* FindGridPattern() const;

	// 설정의 발사 모양 후보에서 이번에 실행할 것 고르기 (비어 있으면 nullptr = 격자 액터가 고름)
	ULaserGridSequenceData* PickSequence();

	// bUseBossAttackPower면 보스 AttackPower × DamageMultiplier, 아니면 -1 (격자 레이저 기본값)
	float GetDamagePerHitOverride() const;

	void FaceTarget();

	bool IsExecutionValid() const;

	void EndWithResult(bool bWasCancelled);

protected:
	// ExecutionPattern, ExecutionTarget(없어도 실행, 시작할 때 바라보기에만 사용), Enemy, PatternComponent, RequestId는 부모 멤버 사용
	UPROPERTY(Transient)
	TObjectPtr<UBossLaserGridPatternSettings> GridSettings;

	UPROPERTY(Transient)
	TObjectPtr<ALaserGridPattern> Grid;

	// 이번 실행에서 끝난 발사 수
	int32 VolleysFired = 0;

	// 순서대로 고를 때 마지막으로 고른 후보 번호 (무작위일 때는 연속 방지용)
	int32 LastSequenceIndex = INDEX_NONE;

	// 격자 시퀀스가 진행 중인지 (취소 시 이 GA가 시작한 것만 멈춤)
	bool bStartedGrid = false;
};
