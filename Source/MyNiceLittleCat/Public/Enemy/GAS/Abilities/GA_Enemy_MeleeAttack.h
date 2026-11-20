/*
* 작성자: 전선현
* 수정 일자: 2026.11.20. (휘두르는 소리를 SwingCue 연출로 재생)
* 패턴 설정의 몽타주를 재생하고 Notify 구간 동안 소켓 판정으로 피해를 주는 근접 공격
* - 판정 구간: AnimNotifyState_EnemyMeleeTrace -> GameplayEvent(Begin/End) -> UEnemyMeleeTraceComponent
* - 원거리, 아레나 공격은 UEnemyPatternAbility를 직접 상속한 별도 GA로 구현
*/

#pragma once

#include "CoreMinimal.h"
#include "Enemy/GAS/Abilities/EnemyPatternAbility.h"
#include "Frameworks/Feedback/FeedbackCue.h"
#include "GA_Enemy_MeleeAttack.generated.h"

class UEnemyMeleePatternSettings;
class UEnemyMeleeTraceComponent;
struct FGameplayEventData;

UCLASS()
class MYNICELITTLECAT_API UGA_Enemy_MeleeAttack : public UEnemyPatternAbility
{
	GENERATED_BODY()

public :

	// 몽타주와 회전을 쓰므로 몸 점유 자원(Resource.Body) 기본 지정
	UGA_Enemy_MeleeAttack();

protected :
	
	/* UEnemyPatternAbility */

	// 근접 설정과 몽타주가 있는지 검사
	virtual bool CanStartPattern() const override;
	
	// 판정 준비, 이동/회전 잠금, 판정 이벤트 구독, 몽타주 재생
	virtual bool StartPattern() override;

	// 판정 종료와 구독 해제
	virtual void StopPattern(bool bWasCancelled) override;

	/* 파생 근접 GA 확장 지점 */

	// 몽타주 재생이 시작된 직후 호출 (돌진 등 추가 동작 시작)
	virtual void OnMontageStarted() {}

	// 판정에 맞은 대상 처리: 같은 실행에서 대상마다 한 번만 피해
	virtual void HandleTargetHit(const FHitResult& Hit);

	/* 몽타주 및 판정 이벤트 */

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageCancelled();

	UFUNCTION()
	void HandleWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void HandleWindowEnd(FGameplayEventData Payload);

protected :

	// 이 GA만 해석하는 근접 실행 설정
	UPROPERTY(Transient)
	TObjectPtr<UEnemyMeleePatternSettings> MeleeSettings;

	// 판정 구간 동안 붙여 재생하는 휘두르는 연출 (구간 종료, 패턴 정리 시 정지)
	FFeedbackInstance SwingFeedback;

	// 소켓 판정 컴포넌트
	TWeakObjectPtr<UEnemyMeleeTraceComponent> MeleeTraceComponent;

	// 판정 결과 구독 핸들
	FDelegateHandle TargetHitHandle;

	// 이번 실행에서 이미 피해를 준 대상: 모든 판정 경로와 구간에서 공유
	TSet<TWeakObjectPtr<AActor>> DamagedActors;

};
