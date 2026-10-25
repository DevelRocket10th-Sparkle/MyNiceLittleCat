/// 작성자: 최요환
/// 작성일자: 2026-10-23
///
/// 보스가 위쪽(눈·가슴 높이의 레이저 컴포넌트)에서 플레이어가 서 있는 바닥 지점으로 레이저를 내리쏘는 패턴 GA입니다.
/// 보스 PatternSet에 패턴으로 등록하면 기존 BT 공격 Task(UEnemyPatternComponent)가 근접 패턴처럼 실행합니다.
/// 흐름: GetActiveExecution으로 설정(UBossLaserPatternSettings)·타깃 받기 → CommitAbility → 이동 정지·이동/회전 잠금
///       1. 추적(AimTrackTime): 공격 지점이 플레이어 발밑을 따라가고 원형 경고도 따라감, 보스는 지점 쪽으로 회전
///       2. 확정(LockDelay): 지점 고정, 경고가 차오름
///       3. 발사(FireDuration): 보스의 ULaserBeamComponent로 지점을 향해 발사. bTrackDuringFire면 지점이 플레이어 쪽으로 이동
///       → 레이저가 꺼지면(OnLaserStopped) ShotInterval 뒤 1단계부터 다시, ShotCount번 쏘면 정상 종료
/// (옵션) bMoveToGridBeamPoint면 시작 전에 격자 액터의 BossBeamPoint(격자 안쪽)로 이동한 뒤 잠금
/// 레이저 교대 페이즈에서는 단일 레이저 차례일 때만 실행되고(CanActivateAbility), 정상 종료 시 순번을 격자로 넘깁니다.
/// 취소(페이즈 전환·사망·리셋)와 정상 종료 모두 EndAbility에서 레이저·경고·잠금을 정리합니다.
/// 피해는 레이저 컴포넌트가 처리합니다 (플레이어는 ApplyPlayerHit → 플레이어 피해 GE). 레이저 컴포넌트는 bPlanarAim을 꺼야 합니다.
/// bUseBossAttackPower면 패턴 동안 레이저 피해량 = 보스 AttackPower × DamageMultiplier (근접 패턴과 같은 공식), 종료 시 원래 값으로 복구합니다.
/// MaxExecutionSeconds는 이동 + ShotCount번 전체 시간보다 길게 잡아야 합니다.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Enemy/Datas/EnemyPatternSetData.h"
#include "GA_BossLaserBeam.generated.h"

class AEnemyCharacter;
class UEnemyPatternComponent;
class ULaserBeamComponent;
class UBossLaserPatternSettings;
class AAttackWarningDecal;

UENUM()
enum class EBossLaserBeamStage : uint8
{
	None,
	Tracking,
	Locked,
	Firing
};

UCLASS()
class MYNICELITTLECAT_API UGA_BossLaserBeam : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BossLaserBeam();

	// 레이저 교대 페이즈에서는 단일 레이저 차례일 때만 실행
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 격자 안쪽 지점 도착 (이동하지 않으면 바로): 잠금 후 첫 발사
	UFUNCTION()
	void HandleArrived();

	// 한 발사의 1단계(추적) 시작
	UFUNCTION()
	void StartShot();

	// 격자 액터의 BossBeamPoint (못 찾으면 false)
	bool FindGridBeamPoint(FTransform& OutTransform) const;

	// 1단계 끝: 공격 지점 확정
	UFUNCTION()
	void HandleAimFinished();

	// 2단계 끝: 확정 지점으로 발사
	UFUNCTION()
	void HandleLockFinished();

	// 레이저가 발사 시간을 다 채워 꺼지면 정상 종료
	UFUNCTION()
	void HandleLaserStopped(ULaserBeamComponent* InLaser);

	// 추적·발사 중 매 갱신: 공격 지점 이동, 경고·보스 회전·레이저 목표 갱신
	void UpdateAim();

	// 공격 지점을 목표 쪽으로 이동 (Speed 0이면 바로)
	void MoveAimPointToward(const FVector& Desired, float Speed, float DeltaTime);

	// 타깃 발밑 바닥 지점 (바닥을 못 찾으면 캡슐 아래 끝)
	bool FindTargetFloorPoint(FVector& OutPoint) const;

	// 보스에서 사용할 레이저 컴포넌트 찾기 (이름 지정 시 그 컴포넌트)
	ULaserBeamComponent* FindLaserComponent() const;

	void FaceAimPoint();

	// 공격 지점에 원형 경고 표시 (Duration 0이면 진행도 수동)
	void ShowAimWarning(float Duration);

	void HideAimWarning();

	bool IsExecutionValid() const;

	void EndWithResult(bool bWasCancelled);

protected:
	// 현재 실행용으로 복사한 패턴 설정
	UPROPERTY(BlueprintReadOnly, Category = "Pattern")
	FEnemyPatternDefinition ExecutionPattern;

	// 실행 시작 시 타깃 (Blackboard 변경과 무관하게 유지)
	UPROPERTY(BlueprintReadOnly, Category = "Pattern")
	TObjectPtr<AActor> ExecutionTarget;

	UPROPERTY(Transient)
	TObjectPtr<UBossLaserPatternSettings> LaserSettings;

	UPROPERTY(Transient)
	TObjectPtr<ULaserBeamComponent> Laser;

	UPROPERTY(Transient)
	TObjectPtr<AAttackWarningDecal> Warning;

	TWeakObjectPtr<AEnemyCharacter> Enemy;

	TWeakObjectPtr<UEnemyPatternComponent> PatternComponent;

	EBossLaserBeamStage Stage = EBossLaserBeamStage::None;

	// 현재 공격 지점 (바닥, 월드)
	FVector AimPoint = FVector::ZeroVector;

	FTimerHandle UpdateTimerHandle;

	// 이번 GA가 실행하는 요청 번호
	int64 RequestId = 0;

	// 이번 실행에서 끝난 발사 수
	int32 ShotsFired = 0;

	// 보스 공격력으로 바꾸기 전 레이저 컴포넌트 피해량 (종료 시 복구)
	float OriginalDamagePerHit = 0.f;

	bool bOverrodeDamage = false;

	bool bOwnsMovementLock = false;

	bool bOwnsRotationLock = false;
};
