/*
* 작성자: 전선현
* 수정 일자: 2026.11.17. (돌진 시작/반복/막힘 연출 슬롯 추가)
* 준비 동작 후 돌진 자세를 유지하며 이동하고, 돌진 동작과 공격을 차례로 재생하는 근접 공격
* - 몽타주 섹션: (시작) -> Rush -> Attack
* - 실행 전체에서 대상마다 피해는 한 번만 (근접 판정과 돌진 판정 공유)
*/

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Enemy/Combat/EnemyFeedback.h"
#include "Enemy/GAS/Abilities/GA_Enemy_MeleeAttack.h"
#include "GA_Enemy_RushAttack.generated.h"

UCLASS()
class MYNICELITTLECAT_API UGA_Enemy_RushAttack : public UGA_Enemy_MeleeAttack
{
	GENERATED_BODY()

protected:

	/* UEnemyPatternAbility / UGA_Enemy_MeleeAttack */

	// 근접 설정 검사 + 돌진 섹션 순서와 수치 검사
	virtual bool CanStartPattern() const override;

	// 루트 모션 무시, 회전 잠금, 돌진 섹션 연결, 돌진 갱신 타이머 시작
	virtual void OnMontageStarted() override;

	// 돌진 타이머, 일시 정지한 몽타주, 루트 모션 설정 복원
	virtual void StopPattern(bool bWasCancelled) override;

private:

	// 돌진 시작 대기 -> 이동 -> 타격 판정
	void UpdateRush();

	// 돌진 종료: 몽타주 재개하여 공격 섹션으로 진행
	void FinishRush();

	// 이동 경로 전체 Sweep 판정
	void SweepRushHits(const FVector& Start, const FVector& End);

protected:

	// 돌진 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rush", meta = (ClampMin = "1", Units = "cm/s"))
	float RushSpeed = 1500.f;

	// 돌진 자세 섹션과 공격 섹션 이름 (몽타주에 맞게 BP에서 설정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rush")
	FName RushSection = TEXT("Rush");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rush")
	FName AttackSection = TEXT("Attack");

	// 캐릭터 캡슐 중심 기준 로컬 오프셋 (X축이 전방)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rush|Hit")
	FVector RushHitOffset = FVector(100.f, 0.f, 0.f);

	// 돌진 판정 구 반경
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rush|Hit", meta = (ClampMin = "1", Units = "cm"))
	float RushHitRadius = 80.f;

	// 돌진 이동 갱신 간격 (작을수록 판정이 촘촘함)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rush", meta = (ClampMin = "0.005", Units = "s"))
	float RushUpdateInterval = 0.01f;

	/* 연출: 캐릭터마다 다르게 하려면 이 GA BP를 캐릭터별로 상속해서 값만 바꿈 */

	// 돌진을 시작하는 순간 발밑에서 재생 (박차고 나가는 먼지, 기합 소리)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rush|Feedback")
	FEnemyFeedbackCue RushStartCue;

	// 돌진하는 동안 몸(RushTrailSocket)에 붙어 반복 재생, 돌진이 끝나면 정지 (반복 이펙트/반복 소리 사용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rush|Feedback")
	FEnemyFeedbackCue RushTrailCue;

	// 돌진 반복 연출을 붙일 메시 소켓 (None이면 메시 원점 = 발밑)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rush|Feedback")
	FName RushTrailSocket;

	// 벽 등 장애물에 막혀 돌진이 멈췄을 때 충돌 지점에서 재생 (쿵 소리, 부스러기)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rush|Feedback")
	FEnemyFeedbackCue RushBlockedCue;

private :

	FTimerHandle RushTimer;

	// 돌진 중 재생 중인 반복 연출
	FEnemyFeedbackInstance RushTrail;

	// 돌진 시작 시 정한 방향과 남은 거리: 시작 후 타깃이 움직여도 목적지를 바꾸지 않음
	FVector RushDirection = FVector::ZeroVector;
	float DistanceRemaining = 0.f;

	// 몽타주에서 돌진 섹션이 시작되는 시각
	float RushStartTime = 0.f;

	// 이전 갱신 월드 시각
	double LastUpdateTime = 0.0;

	bool bRushStarted = false;
	bool bRushing = false;

	// 루트 모션 설정을 바꿨는지와 원래 값
	bool bOwnsRootMotionOverride = false;
	TEnumAsByte<ERootMotionMode::Type> PreviousRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;

};
