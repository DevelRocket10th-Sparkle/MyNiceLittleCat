// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.20.
* 수정 일자: 2026.11.17. (동시 실행 슬롯, 상황별 가중치 재정의, 패턴별 GA Spec)
* 잡몹, 보스 공통 공격 실행 및 관리
* - 패턴 GA 부여, 실행 조건 검사, 가중치 추첨, 요청 번호 발급
* - 동시 실행: 주 패턴 1개(BT가 고르고 결과를 기다림) + 보조 패턴(자원이 겹치지 않는 독립 패턴)
*
* 요청 흐름
*   주 패턴: PickAttackPattern → (BT 방향 정렬) → TryStartPattern → GA 실행 → 후딜 → Completed
*   보조 패턴: 주 패턴이 시작된 직후 빈 자리만큼 하나씩 추첨 → (시작 지연이면 예약) → GA 실행 → 끝나면 자리 반환
*
* 슬롯 규칙
* - 동시 실행 수 = 예약, 실행, 후딜 중인 슬롯 수 (결과만 남은 주 패턴과 남은 생성물은 세지 않음)
* - 자원 태그(Enemy.Pattern.Resource.*): 슬롯이 살아 있는 동안 점유
* - 위험 태그(Enemy.Pattern.Hazard.*)와 위험도: 슬롯 + GA가 등록한 생성물이 남아 있는 동안 점유
* - 몸(Resource.Body)을 쓰는 패턴은 주 패턴으로만 실행: BT가 방향 정렬과 이동을 맡기 때문
* - 다음 공격 간격은 주 패턴이 끝날 때만 추첨 (보조 패턴은 간격에 영향 없음)
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "Enemy/Data/EnemyPatternSetData.h"

#include "EnemyPatternComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
struct FAbilityEndedData;

// 후딜 시작 알림: 이동 Task가 주기 갱신을 기다리지 않고 소유한 추격 경로를 즉시 정리
DECLARE_MULTICAST_DELEGATE(FEnemyRecoveryStarted);

// 실행 슬롯 하나: 예약 → 실행 → (후딜) → 결과
USTRUCT()
struct FEnemyPatternSlot
{
	GENERATED_BODY()

	// 요청 번호: 0은 빈 슬롯
	int64 RequestId = 0;

	// 실행 중인 패턴 설정 복사본
	UPROPERTY(Transient)
	FEnemyPatternDefinition Pattern;

	// 실행 시작 시 타깃
	TWeakObjectPtr<AActor> Target;

	// 이 패턴 전용 GA Spec
	FGameplayAbilitySpecHandle AbilityHandle;

	// 점유한 자원과 위험 태그, 위험도 (초기화 때 계산한 유효 값)
	FGameplayTagContainer Resources;
	int32 Threat = 0;

	// 실행 결과: Running인 동안 동시 실행 수와 자원을 점유
	EEnemyPatternResult Result = EEnemyPatternResult::Running;

	// BT가 기다리는 주 패턴인지 (아니면 보조 패턴)
	bool bPrimary = false;

	// GA를 활성화했는지 (false면 시작 지연 예약 중)
	bool bStarted = false;

	// 후딜 중인지
	bool bRecovering = false;

	// 시작 지연, 종료 누락 감시, 후딜 타이머
	FTimerHandle StartDelayTimer;
	FTimerHandle TimeoutTimer;
	FTimerHandle RecoveryTimer;
};

// GA가 끝난 뒤에도 남아 있는 위험 생성물 (폭탄, 장판): 사라질 때까지 위험 태그와 위험도 점유
struct FEnemyPatternLingeringHazard
{
	// 만든 요청: 같은 요청의 생성물 여러 개는 위험도를 한 번만 셈
	int64 RequestId = 0;
	TWeakObjectPtr<AActor> Actor;
	FGameplayTagContainer Hazards;
	int32 Threat = 0;
};

UCLASS(ClassGroup = (Enemy), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UEnemyPatternComponent : public UActorComponent
{
	GENERATED_BODY()

public :

	// Tick 대신 GAS 종료 이벤트와 타이머로 실행 관리
	UEnemyPatternComponent();

	/* 초기화와 설정 */

	// 이전 실행 정리 후 패턴 설정 검사, 패턴마다 GA Spec 부여, 유효 자원 계산
	bool InitializePatterns(UAbilitySystemComponent* InASC, UEnemyPatternSetData* InPatternSet);

	// 사용할 패턴 ID만 교체 (가중치는 패턴 기본값, 동시 실행은 그대로)
	void SetAvailablePatterns(const TArray<FName>& PatternIds);

	// 사용할 패턴, 가중치 재정의, 동시 실행 설정을 한 번에 교체 (보스 페이즈 진입)
	void SetPatternSelection(const TArray<FEnemyPatternSelectionEntry>& Entries, const FEnemyPatternConcurrency& InConcurrency);

	// Setter: 새 공격 요청 차단 여부 (실행 중 공격은 별도 취소)
	void SetRequestsBlocked(bool bBlocked) { bRequestsBlocked = bBlocked; }

	// 공통 공격 간격 범위 설정: 0초면 추가 대기 없음
	bool ConfigureAttackInterval(float InMinInterval, float InMaxInterval);

	// 다음 공격 허용 시각 초기화 (페이즈 전환, 전투 시작)
	void ResetAttackInterval();

	/* 실행 조건 조회 */

	// 새 주 패턴 요청의 준비 상태: 주 패턴 없음, 몸 비어 있음, 빈 자리, 공격 간격 (거리와 시야 제외)
	UFUNCTION(BlueprintPure, Category = "Pattern|Attack Interval")
	bool CanStartAttack() const;

	// 다음 공격까지 남은 시간
	float GetAttackIntervalRemaining() const;

	// 거리, 높이만 검사: 쿨다운과 시야 때문에 접근 여부가 바뀌지 않도록 분리
	bool HasPatternInRange(AActor* Target) const;

	// 자동 선택 주 패턴 후보가 있는지 검사 (추첨하지 않음)
	bool HasExecutableAttack(AActor* Target) const;

	// 지정한 패턴을 지금 주 패턴으로 시작할 수 있는지 검사
	bool CanStartPattern(FName PatternId, AActor* Target) const;

	// 자동 선택 주 패턴 하나를 추첨 (GA는 실행하지 않음)
	bool PickAttackPattern(AActor* Target, FName& OutPatternId) const;

	/* 실행 요청 */

	// 주 패턴을 추첨해 실행 (보조 패턴도 함께 채움)
	bool TryStartAttack(AActor* Target, int64& OutRequestId);

	// 지정한 패턴을 주 패턴으로 실행하고 요청 번호 반환, 이어서 빈 자리에 보조 패턴 추첨
	// - 직접 지정은 가중치 0인 패턴도 실행 (가중치는 자동 선택 빈도만 조절)
	bool TryStartPattern(FName PatternId, AActor* Target, int64& OutRequestId);

	// 요청 번호가 일치하는 실행만 취소 (주 패턴은 Cancelled 결과 유지, 보조 패턴은 자리 반환)
	void CancelRequest(int64 RequestId);

	// 주 패턴만 취소 (보조 패턴은 계속 실행)
	void CancelActiveRequest();

	// 주 패턴, 보조 패턴, 시작 지연 예약을 모두 취소 (페이즈 전환)
	void CancelAllRequests();

	// 주 패턴 요청 해제: 실행 중이면 먼저 취소하고 결과와 타깃 해제
	void ReleaseRequest(int64 RequestId);

	// 모든 요청을 무효화한 뒤 타이머, 남은 생성물 기록, 능력 실행 정리
	void StopAndInvalidate();

	/* 요청 상태 조회 (BT Task, GA용) */

	// 요청 번호의 실행 결과: 해제되었거나 모르는 요청이면 Invalid
	EEnemyPatternResult GetRequestResult(int64 RequestId) const;

	// 요청이 아직 실행 중(예약, 실행, 후딜)인지
	bool IsRequestRunning(int64 RequestId) const;

	// 공격 GA가 활성화 직후 호출: 자기 Spec Handle로 자기 실행의 설정, 타깃, 번호를 받음
	// - 패턴마다 Spec이 따로라 동시에 여러 GA가 실행돼도 서로의 설정을 받지 않음
	bool GetExecutionForAbility(const FGameplayAbilitySpecHandle& AbilityHandle,
		FEnemyPatternDefinition& OutPattern, AActor*& OutTarget, int64& OutRequestId) const;

	// 주 패턴 실행 중에는 그 패턴의 이동 정책 반환 (보조 패턴은 몸을 쓰지 않으므로 이동과 무관)
	// - 후딜 중에도 요청은 유효하지만 정책은 FollowLocomotion 반환 (실제 정지는 이동 잠금이 담당)
	bool GetActiveMovement(FEnemyPatternMovementSettings& OutMovement, AActor*& OutTarget, int64& OutRequestId) const;

	// 관리 중인 GA와 예약이 모두 끝났는지 (페이즈 전환 대기용)
	bool IsIdle() const;

	// 몸을 쓰는 패턴이 실행 중인지 (후딜 제외): 밀림, 회전 판단용
	bool IsBodyBusy() const;

	// Getter: 주 패턴이 후딜 대기 중인지
	bool IsRecovering() const;

	// Getter: 후딜용 이동 잠금을 이 컴포넌트가 가지고 있는지
	bool OwnsRecoveryMovementLock() const { return bOwnsRecoveryMovementLock; }

	// 지금 동시 실행 수에 들어가는 슬롯 수 (디버그 표시용)
	int32 GetRunningSlotCount() const;

	/* 남은 위험 생성물 (GA용) */

	// 이 요청이 만든 액터가 살아 있는 동안 위험 태그와 위험도를 계속 점유
	// - 액터가 사라지면 다음 추첨에서 자동으로 빠짐 (별도 해제 호출 없음)
	void RegisterLingeringHazard(int64 RequestId, AActor* SpawnedActor);

	/* 패턴 정보 조회 */

	// 초기화 과정에서 해당 패턴 ID가 등록됐는지 확인
	bool HasPattern(FName PatternId) const;

	// 등록된 패턴이 있는지 확인
	bool HasPatterns() const { return !bStopped && PatternSet && !PatternSet->Patterns.IsEmpty(); }

	// 지정한 패턴의 설정 복사
	bool GetPatternDefinition(FName PatternId, FEnemyPatternDefinition& OutDefinition) const;

	// 현재 선택 상황의 유효 가중치 (재정의가 없으면 패턴 기본값)
	float GetSelectionWeight(const FEnemyPatternDefinition& Pattern) const;

	// 이동 모드 결정용 패턴 ID: 주 패턴 실행 중(후딜 포함) → 선택만 된 패턴 → None
	FName GetActivePatternId() const;

	// 선택 후 실행 전(방향 정렬 중) 패턴 기록
	void SetReservedPattern(FName PatternId) { ReservedPatternId = PatternId; }

	// 현재 예약 ID가 호출자가 정리하려는 ID와 일치할 때만 해제
	void ClearReservedPattern(FName PatternId);

public :

	// 주 패턴 후딜 시작 알림
	FEnemyRecoveryStarted OnRecoveryStarted;

protected :

	// 플레이 종료 시 실행, 구독 및 직접 소유한 자원 정리 후 부모 호출
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private :

	/* 후보 검사 */

	// DataAsset에서 ID가 일치하는 설정의 주소 조회
	const FEnemyPatternDefinition* FindPattern(FName PatternId) const;

	// 실행 주체, 허용 목록, 타깃 생존 및 선택적인 거리 조건 확인
	bool MatchesGeometry(const FEnemyPatternDefinition& Pattern, AActor* Target) const;

	// 거리 조건에 행동 가능 여부, 시야 및 GAS 활성화 조건을 추가 검사
	bool CanExecutePattern(const FEnemyPatternDefinition& Pattern, AActor* Target) const;

	// 살아 있는 슬롯과 남은 생성물이 점유한 자원·위험 태그와 위험도 합
	void CollectOccupied(FGameplayTagContainer& OutResources, int32& OutThreat) const;

	// 자동 선택 후보 수집: 가중치 > 0, 실행 조건, 자원·위험도 통과 (보조 후보는 몸을 쓰지 않는 패턴만)
	void CollectCandidates(AActor* Target, bool bCompanion, const TSet<FName>& Excluded,
		TArray<const FEnemyPatternDefinition*>& OutCandidates) const;

	// 점유 중인 자원·위험도와 겹치지 않고 함께 실행할 수 있는지
	bool CanJoin(const FEnemyPatternDefinition& Pattern, const FGameplayTagContainer& Occupied, int32 OccupiedThreat) const;

	// 패턴이 몸(Resource.Body)을 쓰는지
	bool UsesBody(FName PatternId) const;

	// 가중치 추첨: 후보 중 하나 (후보가 없으면 nullptr)
	const FEnemyPatternDefinition* PickWeighted(TConstArrayView<const FEnemyPatternDefinition*> Candidates) const;

	// 패턴의 유효 자원 계산: GA 기본 자원 + 패턴 추가 자원 + 몸 사용 자동 판정
	FGameplayTagContainer ResolvePatternResources(const FEnemyPatternDefinition& Pattern) const;

	/* 슬롯 관리 */

	// 슬롯 생성: 자원과 동시 실행 수를 이 시점부터 점유
	FEnemyPatternSlot& AddSlot(const FEnemyPatternDefinition& Pattern, AActor* Target, bool bPrimary);

	// 요청 번호로 슬롯 검색
	FEnemyPatternSlot* FindSlot(int64 RequestId);
	const FEnemyPatternSlot* FindSlot(int64 RequestId) const;

	// 슬롯의 GA 활성화: 실패하면 슬롯을 실패 처리하고 false
	bool ActivateSlot(int64 RequestId);

	// 주 패턴 시작 직후 빈 자리에 보조 패턴을 하나씩 추첨해 예약 또는 실행
	void FillCompanions(AActor* Target);

	// 슬롯 종료: 주 패턴은 결과를 남기고, 보조 패턴은 슬롯을 지움
	void FinishSlot(int64 RequestId, EEnemyPatternResult Result);

	// 슬롯 삭제 (타이머 포함)
	void RemoveSlot(int64 RequestId);

	// 슬롯 타이머 정리
	void ClearSlotTimers(FEnemyPatternSlot& Slot);

	// 사라진 생성물 기록 정리
	void PruneLingeringHazards();

	/* GAS와 타이머 콜백 */

	// 모든 패턴 GA의 종료 이벤트: Spec Handle로 슬롯을 찾아 처리
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);

	// 시작 지연 종료, 종료 누락, 후딜 종료
	void HandleStartDelayFinished(int64 RequestId);
	void HandleExecutionTimeout(int64 RequestId);
	void HandleRecoveryFinished(int64 RequestId);

	// 주 패턴 후딜 시작 (이동 잠금 포함)
	void StartPrimaryRecovery(FEnemyPatternSlot& Slot);
	void ReleaseRecoveryMovementLock();

	// 주 패턴 정상 완료 시 다음 공격 허용 시각 추첨
	void RollNextAttackTime();

	// 이 컴포넌트가 부여한 GA 제거와 종료 이벤트 구독 해제
	void ClearGrantedAbilities();

private :

	/* 설정 */

	// 캐릭터별 공유 패턴 설정: 실행 중 원본을 변경하지 않음
	UPROPERTY(Transient)
	TObjectPtr<UEnemyPatternSetData> PatternSet;

	// 능력을 부여하고 실행할 ASC
	TWeakObjectPtr<UAbilitySystemComponent> ASC;

	// 패턴 ID와 이 패턴 전용 GA Spec (같은 GA 클래스여도 패턴마다 따로 부여)
	TMap<FName, FGameplayAbilitySpecHandle> PatternHandles;

	// 패턴 ID와 유효 자원·위험 태그 (초기화 때 계산)
	TMap<FName, FGameplayTagContainer> PatternResources;

	// 현재 상황에서 실행을 허용한 패턴 ID와 가중치 재정의
	TSet<FName> AvailablePatterns;
	TMap<FName, float> WeightOverrides;

	// 동시 실행 설정 (기본값 = 한 번에 하나)
	FEnemyPatternConcurrency Concurrency;

	// 공통 공격 간격 범위
	float MinAttackInterval = 0.f;
	float MaxAttackInterval = 0.f;

	// 모든 패턴 GA의 종료 이벤트 구독
	FDelegateHandle AbilityEndedDelegateHandle;

	/* 실행 상태 */

	// 실행 슬롯: 주 패턴은 BT가 해제할 때까지, 보조 패턴은 끝날 때까지 남음
	UPROPERTY(Transient)
	TArray<FEnemyPatternSlot> Slots;

	// GA가 끝난 뒤에도 남은 위험 생성물
	TArray<FEnemyPatternLingeringHazard> LingeringHazards;

	// 새 요청 번호 발급용: 이전 번호를 재사용하지 않음
	int64 NextRequestId = 0;

	// BT가 기다리는 주 패턴 요청 번호: 0은 없음
	int64 PrimaryRequestId = 0;

	// 선택 후 실행 전(방향 정렬 중) 패턴 ID
	FName ReservedPatternId;

	/* 상태 */

	// 초기화 전 또는 전체 정지 이후
	bool bStopped = true;

	// 새 공격 요청만 임시 차단
	bool bRequestsBlocked = true;

	// 후딜용 이동 잠금 소유
	bool bOwnsRecoveryMovementLock = false;

	// 다음 공격 허용 월드 시각: 일시 정지와 시간 배율을 따르며, BT 재진입 시 재추첨하지 않음
	double NextAttackAllowedTime = 0.0;
};
