// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.20.
* 잡몹, 보스 공통 공격 실행 및 관리
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "GameplayAbilitySpec.h"
#include "Enemy/Datas/EnemyPatternSetData.h"

#include "EnemyPatternComponent.generated.h"

class UAbilitySystemComponent;
struct FAbilityEndedData;

DECLARE_MULTICAST_DELEGATE(FEnemyRecoveryStarted);

UCLASS(ClassGroup=(Enemy), meta=(BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UEnemyPatternComponent : public UActorComponent
{
	GENERATED_BODY()

public:	

	// 컴포넌트의 기본 실행 상태를 설정한다.
	// 생성자: Tick 대신 GAS 종료 이벤트와 타이머로 실행 관리
	UEnemyPatternComponent();

	// BT가 주기 갱신을 기다리지 않고 소유한 추격 경로를 즉시 정리하는 알림이다.
	FEnemyRecoveryStarted OnRecoveryStarted;

	// 공격 실행이 끝난 뒤 후딜 대기 중인지 반환
	inline bool IsRecovering() const { return bRecovering; }

	// 이 컴포넌트가 후딜용 이동 잠금을 소유하고 있는지 반환
	inline bool OwnsRecoveryMovementLock() const { return bOwnsRecoveryMovementLock; }

	// 등록된 패턴 초기화
	// 이전 실행 정리 후 패턴 설정 검사 및 필요한 GA 부여
	bool InitializePatterns(
		UAbilitySystemComponent* InASC,
		UEnemyPatternSetData* InPatternSet
	);

	// 사용 가능 패턴 설정
	// 현재 페이즈에서 사용할 패턴 ID만 교체
	void SetAvailablePatterns(const TArray<FName>& PatternIds);
	
	// Setter: bRequestsBlocked
	// Setter: 새 공격 요청 차단 여부; 실행 중 공격은 별도 취소
	inline void SetRequestsBlocked(bool bBlocked) { bRequestsBlocked = bBlocked; }

	// 공통 적 초기화에서 EnemyConfigData의 범위를 전달한다. 기본 0초면 추가 대기를 사용하지 않는다.
	bool ConfigureAttackInterval(float InMinInterval, float InMaxInterval);
	void ResetAttackInterval();

	// 거리와 시야 조건을 제외한 새 요청의 준비 상태다. 실행 중인 요청에는 사용하지 않는다.
	UFUNCTION(BlueprintPure, Category="Pattern|Attack Interval")
	bool CanStartAttack() const;

	UFUNCTION(BlueprintPure, Category="Pattern|Attack Interval")
	float GetAttackIntervalRemaining() const;

	UFUNCTION(BlueprintPure, Category="Pattern|Attack Interval")
	double GetNextAttackAllowedTime() const { return NextAttackAllowedTime; }

	// 요청된 공격 시도
	// 조건을 만족하는 후보 중 확률 가중치로 공격 하나 선택
	bool TryStartAttack(AActor* Target, int64& OutRequestId);

	// 요청된 패턴 시도
	// 지정한 패턴을 실행하고 요청 번호 반환; true는 시작 성공
	bool TryStartPattern(FName PatternId, AActor* Target, int64& OutRequestId);

	// 요청 취소
	// 요청 번호가 일치하는 공격만 취소하고 Cancelled 결과 유지
	void CancelRequest(int64 RequestId);

	// 활성화된 요청 취소
	// 현재 요청 번호로 실행 중인 공격 취소
	void CancelActiveRequest();

	// 요청 해제
	// 실행 중이면 먼저 취소하고 요청 번호, 결과 및 타깃 해제
	void ReleaseRequest(int64 RequestId);

	// 요청 번호의 실행 결과 조회; 해제되거나 다른 요청이면 Invalid
	EEnemyPatternResult GetRequestResult(int64 RequestId) const;

	// 초기화 과정에서 해당 패턴 ID가 등록됐는지 확인
	bool HasPattern(FName PatternId) const;

	// 새 패턴 경로가 초기화돼 등록된 Ability가 있는지 확인
	// Getter: 새 패턴 경로에 등록한 능력이 있는지 확인
	bool HasPatterns() const { return !bStopped && PatternSet && !PatternSet->Patterns.IsEmpty(); }

	// 관리 중인 공격 GA가 실제로 모두 종료됐는지 확인
	bool IsIdle() const;

	// 거리, 높이만 검사
	// 쿨다운과 시야 때문에 접근 여부가 바뀌지 않도록 분리
	// 쿨다운과 시야를 제외한 현재 패턴의 타깃 및 거리 조건 검사
	bool HasPatternInRange(AActor* Target) const;
	
	// 공격 GA에서 활성화 직후 호출하여 실행 데이터 복사
	// 현재 요청의 설정, 타깃 포인터 및 번호를 GA의 출력 인자로 전달
	UFUNCTION(BlueprintPure, Category = "Pattern")
	bool GetActiveExecution(
		FEnemyPatternDefinition& OutPattern,
		AActor*& OutTarget,
		int64& OutRequestId
	) const;
	
	// 지연된 판정이 현재 유효한 요청인지 번호와 실행 상태로 확인
	UFUNCTION(BlueprintPure, Category = "Pattern")
	bool IsRequestRunning(int64 RequestId) const;

	// 공격 중에는 패턴 정책을, 후딜 중에는 항상 Stop을 반환한다.
	UFUNCTION(BlueprintPure, Category="Pattern")
	bool GetActiveMovement(FEnemyPatternMovementSettings& OutMovement, AActor*& OutTarget, int64& OutRequestId) const;

	// 요청을 먼저 무효화한 뒤 타이머, 구독 및 능력 실행 정리
	void StopAndInvalidate();

	// 실행 가능한 자동 선택 후보 중 하나를 추첨 (GA는 실행하지 않음)
	bool PickAttackPattern(AActor* Target, FName& OutPatternId) const;

	// 지정한 패턴의 설정 복사
	bool GetPatternDefinition(FName PatternId, FEnemyPatternDefinition& OutDefinition) const;

	// 지정한 패턴을 지금 시작할 수 있는지 검사
	bool CanStartPattern(FName PatternId, AActor* Target) const;

	// 실행 가능한 자동 선택 후보가 있는지 검사 (추첨하지 않음)
	bool HasExecutableAttack(AActor* Target) const;
	
protected:

	// 플레이 종료 시 실행, 구독 및 직접 소유한 자원 정리 후 부모 호출
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private :
	
	// DataAsset에서 ID가 일치하는 설정의 주소 조회
	const FEnemyPatternDefinition* FindPattern(FName PatternId) const;

	// 실행 주체, 허용 목록, 타깃 생존 및 선택적인 거리 조건 확인
	bool MatchesGeometry(const FEnemyPatternDefinition& Pattern, AActor* Target) const;

	// 거리 조건에 행동 가능 여부, 시야 및 GAS 활성화 조건을 추가 검사
	bool CanExecutePattern(const FEnemyPatternDefinition& Pattern, AActor* Target) const;
	
	// 현재 능력의 종료 이벤트만 받아 완료 또는 취소 기록
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);

	// 정상 종료 후 후딜까지 끝나야 BT에 완료를 알린다.
	void HandleRecoveryFinished();
	void StartRecovery(float Duration);
	void ReleaseRecoveryMovementLock();

	// 종료 이벤트 누락을 실패로 기록하고 해당 능력 취소
	void HandleExecutionTimeout();
	
	// 현재 요청의 ASC 종료 이벤트 구독만 해제
	void UnbindAbilityEnded();

	// 이 컴포넌트가 부여한 GA를 중복 없이 서버에서 제거
	void ClearGrantedAbilities();

private :

	// 캐릭터별 공유 패턴 설정; 실행 중 원본을 변경하지 않음
	UPROPERTY(Transient)
	TObjectPtr<UEnemyPatternSetData> PatternSet;
	
	// 능력을 부여하고 실행할 ASC의 약한 참조
	TWeakObjectPtr<UAbilitySystemComponent> ASC;

	// 패턴 ID와 부여한 GA 핸들 연결
	TMap<FName, FGameplayAbilitySpecHandle> PatternHandles;

	// 현재 페이즈에서 실행을 허용한 패턴 ID 집합
	TSet<FName> AvailablePatterns;

	// 로그 전용 패턴의 반복 출력 방지: 초기화할 때 비움
	TSet<FName> LoggedPlaceholderPatterns;

	UPROPERTY(Transient)
	FEnemyPatternDefinition ActivePattern;				// 활성화된 패턴

	TWeakObjectPtr<AActor> ActiveTarget;				// 현재 타깃

	FGameplayAbilitySpecHandle ActiveAbilityHandle;		// GA 스펙 관리

	FDelegateHandle AbilityEndedDelegateHandle;			// GA 종료 델리게이트 관리

	// 능력 종료 누락을 감지하는 제한 시간 타이머
	FTimerHandle ExecutionTimeoutHandle;

	FTimerHandle RecoveryTimerHandle;

	// 새 요청 번호 발급용; 이전 번호를 재사용하지 않음
	int64 NextRequestId = 0;
	// 현재 요청 번호; 0은 유효한 요청 없음
	int64 ActiveRequestId = 0;

	// 현재 요청의 실행 결과
	EEnemyPatternResult ActiveResult = EEnemyPatternResult::Invalid;

	// 초기화 전 또는 전체 정지 이후 상태
	bool bStopped = true;
	// 새 공격 요청만 임시 차단하는 상태
	bool bRequestsBlocked = true;
	bool bRecovering = false;
	bool bOwnsRecoveryMovementLock = false;

	float MinAttackInterval = 0.f;
	float MaxAttackInterval = 0.f;
	// 월드 게임 시간을 사용하므로 일시 정지와 시간 배율을 따르며, BT 재진입 시 재추첨하지 않는다.
	double NextAttackAllowedTime = 0.0;

};
