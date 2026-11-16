// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.20.
* 수정 일자: 2026.11.13. (로그 전용 패턴 제거, 대상 판정을 EnemyTargetUtils로 이동, 미사용 BP 노출 제거)
* 잡몹, 보스 공통 공격 실행 및 관리
* - 패턴 GA 부여, 실행 조건 검사, 가중치 추첨, 요청 번호 발급
* - GA 종료 이벤트 -> 후딜 -> 공격 간격 -> 완료 (BT Task는 요청 번호로 결과만 확인)
* 
* 요청 흐름
*	TryStartPattern -> GA 활성화(GA가 GetActiveExecution으로 설정 수신) -> GA 종료
*   -> 후딜(RecoverySeconds, 이동 잠금) -> Completed + 다음 공격 허용 시각 추첨
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "GameplayAbilitySpec.h"
#include "Enemy/Data/EnemyPatternSetData.h"

#include "EnemyPatternComponent.generated.h"

class UAbilitySystemComponent;
struct FAbilityEndedData;

// 후딜 시작 알림: 이동 Task가 주기 갱신을 기다리지 않고 소유한 추적 경로를 즉시 정리
DECLARE_MULTICAST_DELEGATE(FEnemyRecoveryStarted);

UCLASS(ClassGroup=(Enemy), meta=(BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UEnemyPatternComponent : public UActorComponent
{
	GENERATED_BODY()

public:	

	// Tick 대신 GAS 종료 이벤트와 타이머로 실행 관리
	UEnemyPatternComponent();

	/* 초기화와 설정 */

	// 이전 실행 정리 후 패턴 설정 검사 및 필요한 GA 부여
	bool InitializePatterns(UAbilitySystemComponent* InASC, UEnemyPatternSetData* InPatternSet);
	
	// 현재 페이즈에서 사용할 패턴 ID만 교체
	void SetAvailablePatterns(const TArray<FName>& PatternIds);

	// Setter: 새 공격 요청 차단 여부 (실행 중 공격은 별도 취소)
	void SetRequestsBlocked(bool bBlocked) { bRequestsBlocked = bBlocked; }

	// 공통 공격 간격 범위 설정: 0초면 추가 대기 없음
	bool ConfigureAttackInterval(float InMinInterval, float InMaxInterval);

	// 다음 공격 허용 시각 초기화 (페이즈 전환, 전투 시작)
	void ResetAttackInterval();

	/* 실행 조건 조회 */

	// 거리와 시야를 제외한 새 요청의 준비 상태 (실행 중인 요청에는 사용하지 않음)
	UFUNCTION(BlueprintPure, Category = "Pattern|Attack Interval")
	bool CanStartAttack() const;

	// 다음 공격까지 남은 시간
	float GetAttackIntervalRemaining() const;

	// 거리, 높이만 검사: 쿨다운과 시야 때문에 접근 여부가 바뀌지 않도록 분리
	bool HasPatternInRange(AActor* Target) const;

	// 실행 가능한 자동 선택 후보가 있는지 검사 (추첨X)
	bool HasExecutableAttack(AActor* Target) const;

	// 지정한 패턴을 지금 시작할 수 있는지 검사
	bool CanStartPattern(FName PatternId, AActor* Target) const;

	// 실행 가능한 자동 선택 후보 중 하나를 추첨 (GA는 실행하지 않음)
	bool PickAttackPattern(AActor* Target, FName& OutPatternId) const;

	/* 실행 요청 */
	
	// 조건을 만족하는 후보 중 가중치로 하나를 골라 실행
	bool TryStartAttack(AActor* Target, int64& OutRequestId);

	// 지정한 패턴을 실행하고 요청 번호 반환: true는 시작 성공
	bool TryStartPattern(FName PatternId, AActor* Target, int64& OutRequestId);

	// 요청 번호가 일치하는 공격만 취소하고 Cancelled 결과 유지
	void CancelRequest(int64 RequestId);
	
	// 현재 요청 번호로 실행 중인 공격 취소
	void CancelActiveRequest();

	// 실행 중이면 먼저 취소하고 요청 번호, 결과 및 타깃 해제
	void ReleaseRequest(int64 RequestId);

	// 요청을 먼저 무효화한 뒤 타이머, 구독 및 능력 실행 정리
	void StopAndInvalidate();

	/* 요청 상태 조회 (BT Task, GA용) */

	// 요청 번호의 실행 결과: 해제되거나 다른 요청이면 Invalid
	EEnemyPatternResult GetRequestResult(int64 RequestId) const;

	// 지연된 판정이 현재 유효한 요청인지 번호와 실행 상태로 확인
	bool IsRequestRunning(int64 RequestId) const;

	// 공격 GA가 활성화 직후 호출: 현재 요청의 설정, 타깃, 번호 복사
	bool GetActiveExecution(FEnemyPatternDefinition& OutPattern, AActor*& OutTarget, int64& OutRequestId) const;
	
	// GA 실행 중에는 패턴의 이동 정책 반환
	// - 후딜 중에도 요청은 유효하지만 정책은 FollowLocomotion 반환 (실제 정지는 이동 잠금이 담당)
	bool GetActiveMovement(FEnemyPatternMovementSettings& OutMovement, AActor*& OutTarget, int64& OutRequestId) const;

	// 관리 중인 공격 GA가 실제로 모두 종료됐는지 확인
	bool IsIdle() const;

	// Getter: 후딜 대기 중인지
	bool IsRecovering() const { return bRecovering; }

	// Getter: 후딜용 이동 잠금을 이 컴포넌트가 가지고 있는지
	bool OwnsRecoveryMovementLock() const { return bOwnsRecoveryMovementLock; }

	/* 패턴 정보 조회 */
	
	// 초기화 과정에서 해당 패턴 ID가 등록됐는지 확인
	bool HasPattern(FName PatternId) const;

	// 등록된 패턴이 있는지 확인
	bool HasPatterns() const { return !bStopped && PatternSet && !PatternSet->Patterns.IsEmpty(); }

	// 지정한 패턴의 설정 복사
	bool GetPatternDefinition(FName PatternId, FEnemyPatternDefinition& OutDefinition) const;

	// 이동 모드 결정용 패턴 ID: 실행 중(후딜 포함) -> 선택만 된 패턴 -> None
	FName GetActivePatternId() const;

	// 선택 후 실행 전(방향 정렬 중) 패턴 기록
	void SetReservedPattern(FName PatternId) { ReservedPatternId = PatternId; }

	// 현재 예약 ID가 호출자가 정리하려는 ID와 일치할 때만 해제
	void ClearReservedPattern(FName PatternId);

public :

	// 후딜 시작 알림
	FEnemyRecoveryStarted OnRecoveryStarted;

protected :
	
	// 플레이 종료 시 실행, 구독 및 직접 소유한 자원 정리 후 부모 호출
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private :

	// DataAsset에서 ID가 일치하는 설정의 주소 조회
	const FEnemyPatternDefinition* FindPattern(FName PatternId) const;
	
	// 실행 주체, 허용 목록, 타깃 생존 및  선택적인 거리 조건 확인
	bool MatchesGeometry(const FEnemyPatternDefinition& Pattern, AActor* Target) const;

	// 거리 조건에 행동 가능 여부, 시야 및 GAS 활성화 조건을 추가 검사
	bool CanExecutePattern(const FEnemyPatternDefinition& Pattern, AActor* Target) const;

	// 현재 능력의 종료 이벤트만 받아 완료 또는 취소 기록
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);

	// 후딜 시작/종료: 정상 종료 후 후딜까지 끝나야 Completed
	void StartRecovery(float Duration);
	void HandleRecoveryFinished();
	void ReleaseRecoveryMovementLock();

	// 정상 완료 시 다음 공격 허용 시각 추첨
	void RollNextAttackTime();

	// 종료 이벤트 누락을 실패로 기록하고 해당 능력 취소
	void HandleExecutionTimeout();

	// 현재 요청의 ASC 종료 이벤트 구독만 해제
	void UnbindAbilityEnded();

	// 이 컴포넌트가 부여한 GA를 중복 없이 서버에서 제거
	void ClearGrantedAbilities();

private :

	/* 설정 */

	// 캐릭터별 공유 패턴 설정: 실행 중 원본을 변경하지 않음
	UPROPERTY(Transient)
	TObjectPtr<UEnemyPatternSetData> PatternSet;

	// 능력을 부여하고 실행할 ASC
	TWeakObjectPtr<UAbilitySystemComponent> ASC;

	// 패턴 ID와 부여한 GA 핸들 (같은 GA 클래스는 핸들 공유)
	TMap<FName, FGameplayAbilitySpecHandle> PatternHandles;

	// 현재 페이즈에서 실행을 허용할 패턴 ID
	TSet<FName> AvailablePatterns;

	// 공통 공격 간격 범위
	float MinAttackInterval = 0.f;
	float MaxAttackInterval = 0.f;

	/* 현재 요청 */
	
	// 실행 중인 패턴 설정 복사본
	UPROPERTY(Transient)
	FEnemyPatternDefinition ActivePattern;

	// 실행 시작 시 타깃
	TWeakObjectPtr<AActor> ActiveTarget;

	// 실행 중인 GA 핸들
	FGameplayAbilitySpecHandle ActiveAbilityHandle;

	// GA 종료 이벤트 구독
	FDelegateHandle AbilityEndedDelegateHandle;

	// GA 종료 누락 감지 타이머 후딜 타이머
	FTimerHandle ExecutionTimeoutHandle;

	// GA 후딜 타이머
	FTimerHandle RecoveryTimerHandle;

	// 새 요청 번호 발급용: 이전 번호를 재사용하지 않음
	int64 NextRequestId = 0;

	// 현재 요청 번호: 0은 유효한 요청 없음
	int64 ActiveRequestId = 0;

	// 현재 요청의 실행 결과
	EEnemyPatternResult ActiveResult = EEnemyPatternResult::Invalid;

	// 선택 후 실행 전(방향 정렬 중) 패턴 ID
	FName ReservedPatternId;

	/* 상태 */

	// 초기화 전 또는 전체 정지 이후
	bool bStopped = true;

	// 새 공격 요청만 임시 차단
	bool bRequestsBlocked = true;

	// 후딜 대기 중
	bool bRecovering = false;

	// 후딜용 이동 잠금 소유
	bool bOwnsRecoveryMovementLock = false;

	// 다음 공격 허용 월드 시각: 일시 정지와 시간 배율을 따르며, BT 재진입 시 재추첨하지 않음
	double NextAttackAllowedTime = 0.0;

};
