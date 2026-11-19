// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.20.
* 수정 일자: 2026.11.19. (전투 실패·페이즈 전환의 공격 정리를 HaltCombatActions로 통일)
* 보스 전용 기능 구현을 위한 EnemyCharacter 상속 클래스
* - 보스방 전투 세션 (시작, 정지, 실패 예약, 세대 번호)
* - 페이즈 진행 연결 (UBossPhaseComponent), 페이즈별 공격 목록과 기믹 적용
* - 다른 담당자의 기믹 컴포넌트는 IBossPhaseGimmickInterface로만 연결
*/

#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyCharacter.h"

#include "Enemy/Data/BossMovementTypes.h"
#include "Enemy/Data/BossPhaseConfigData.h"

#include "BossCharacter.generated.h"

class ABossAIController;
class ABossCharacter;
class UBossPhaseComponent;

// 새 페이즈가 활성화됨 (레벨 BP에서 구독)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBossPhaseStartedSignal, ABossCharacter*, Boss, FName, StartedPhaseId);

// 보스방 전투 이벤트 (처치, 사망 연출 종료, 실패)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBossEncounterSignal, ABossCharacter*, Boss);

// 페이즈 전환 종료 (BT 전환 Task가 구독)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBossPhaseTransitionResult, int64 /*TransitionId*/, bool /*bSucceeded*/);

UCLASS()
class MYNICELITTLECAT_API ABossCharacter : public AEnemyCharacter
{
	GENERATED_BODY()

public :

	// 보스 전용 AI 컨트롤러와 페이즈 컴포넌트 생성
	ABossCharacter(const FObjectInitializer& ObjectInitializer);

	/* AEnemyCharacter */

	// 초기화와 생존에 보스방 전투 진행 여부를 추가로 확인
	virtual bool CanRunEnemyAI() const override;

	// Health 변경 전에 전투 상태와 페이즈 체력 하한을 반영
	virtual float CalculateHealthAfterDamage(float CurrentHealth, float MaxHealth, float Damage) const override;

	// 사망 연출 종료 후 매니저에 알림
	virtual void FinishDeath() override;

	/* 보스방 전투 (BossEncounterManager가 호출) */

	// 매니저가 FinishSpawning 전에 레벨 기믹 참조 저장: 기믹 컴포넌트 연결 시 전달
	void SetLevelGimmickReferences(const TMap<FName, TObjectPtr<AActor>>& PhaseActors, AActor* AreaCenter);

	// 매니저가 전달한 이동 경로를 페이즈 컴포넌트에 전달
	void SetLevelMovementRoutes(const TMap<FName, FBossMovementRoute>& InRoutes);

	// 레벨의 전환 연출 담당자 설정 (없으면 nullptr → 페이즈 설정의 기본 연출 사용)
	void SetPhaseTransitionHandler(UObject* Handler);

	// 준비된 보스의 첫 페이즈와 AI 시작
	bool StartEncounter();

	// 전투 상태를 먼저 무효화한 뒤 공격, 전환, 기믹, AI 정리
	void StopEncounter();

	// Task 콜백 도중 보스를 파괴하지 않도록 다음 Tick에 실패 통지 예약
	void ScheduleEncounterFailure();

	// Getter: 보스방 전투가 진행 중인지 확인
	bool IsEncounterActive() const { return bEncounterActive && !bFailurePending; }

	// Getter: 사망 연출까지 끝났는지 확인
	bool HasFinishedDeath() const { return LifeState == EEnemyLifeState::Dead; }

	/* 페이즈 전환 (BT 전환 Task와 레벨 연출 담당자가 호출) */

	// 대기 중인 전환 시작: 결과는 OnPhaseTransitionResult로 통지
	bool BeginPhaseTransition();

	// 레벨 연출 담당자의 전환 연출 종료 통지
	bool NotifyPhaseTransitionPresentationFinished(int64 TransitionId);

	// Getter: 대기 또는 진행 중인 전환 번호 (0이면 없음)
	int64 GetPendingTransitionId() const;

	/* 기믹 (다른 담당자의 기믹 컴포넌트가 호출) */

	// 현재 연결한 기믹 컴포넌트의 완료만 페이즈 진행에 전달
	UFUNCTION(BlueprintCallable, Category = "Boss|Gimmick")
	bool NotifyPhaseGimmickCompleted(UActorComponent* SourceComponent, FName PhaseId, int64 PhaseExecutionId);

	/* 상태 조회 (AI 컨트롤러, 기믹, 레이저 GA 등에서 사용) */

	// 일반 행동(공격, 이동) 가능 여부: Blackboard CanAct
	bool CanBossAct() const;

	// 전환 요청 또는 진행 중: Blackboard PhaseTransitionRequested
	bool IsPhaseTransitionInProgress() const;

	// Getter: 현재 페이즈 ID
	UFUNCTION(BlueprintPure, Category = "Boss|Phase")
	FName GetCurrentPhaseId() const;

	// Getter: 현재 페이즈 설정 (시작 전 nullptr)
	const FBossPhaseDefinition* GetCurrentPhaseDefinition() const;

	// Getter: 페이즈 컴포넌트
	UBossPhaseComponent* GetPhaseComponent() const { return PhaseComponent; }

	// Getter: 시퀀스에서 이 보스를 연결할 바인딩 태그
	FName GetSequenceBindingTag() const { return SequenceBindingTag; }

	// Getter: 체력바에 표시할 보스 이름 (비어 있으면 위젯 기본 이름)
	const FText& GetBossDisplayName() const { return BossDisplayName; }

protected :

	/* AEnemyCharacter */

	// 기본 Enemy 초기화 후 페이즈 설정, 패턴 ID, 기믹 연결 검사
	virtual bool InitializeEnemyExtensions() override;

	// 사망 처리를 먼저 수행한 뒤 살아 있는 보스의 체력 조건 평가
	virtual void HandleHealthChanged(float NewHealth, float MaxHealth) override;

	// 전투 정리 후 기존 Enemy 사망 연출 수행
	virtual void BeginDeath() override;

	// 액터 제거 시 전투 실행 및 델리게이트 구독 정리
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private :

	/* 페이즈 컴포넌트 콜백 */

	// 전환 요청 즉시 공격 및 피해를 막고 기존 공격 취소
	void HandleTransitionRequested();

	// 새 페이즈의 공격 목록 및 기믹 적용
	bool HandlePhaseEntered(const FBossPhaseDefinition& Phase);

	// 전환 결과: 성공이면 페이즈 활성화 마무리, 실패면 전투 실패 예약
	void HandleTransitionFinished(int64 TransitionId, bool bSucceeded);

	/* 내부 처리 */

	// 페이즈 활성화 마무리: 위치, 기준 방향, 공격 허용, Blackboard, 레벨 알림
	void FinishPhaseActivation();

	// 설정의 패턴 ID와 추격 반경 검사
	bool ValidatePhasePatterns() const;

	// 보스에 부착된 기믹 계약 구현 컴포넌트 하나를 찾아 연결
	bool InitializeGimmickConnection();

	// 연결한 기믹의 실행 중지 호출
	void StopPhaseGimmicks();

	// 새 공격 요청을 막고 진행 중인 패턴(주, 보조, 시작 지연 예약, 상시)과 페이즈 기믹을 정리 (전투 실패, 페이즈 전환)
	void HaltCombatActions();

	// 페이즈 설정이 켜져 있으면 첫 Waypoint로 순간이동
	void TeleportToFirstWaypoint(const FBossPhaseDefinition& Phase);

	// 보스 상태 변화를 AI 컨트롤러의 Blackboard에 반영
	void NotifyCombatStateChanged();

	// 빙의한 보스 AI 컨트롤러
	ABossAIController* GetBossAIController() const;

public :

	/* 이벤트 */

	// 새 페이즈가 활성화된 뒤 레벨 BP에 알림 (첫 페이즈 포함)
	UPROPERTY(BlueprintAssignable, Category = "Boss|Phase")
	FBossPhaseStartedSignal OnPhaseStarted;

	// 보스 처치 (사망 시작): 매니저가 클리어 처리
	UPROPERTY(BlueprintAssignable, Category = "Boss")
	FBossEncounterSignal OnDefeated;

	// 사망 연출 종료: 매니저가 다음 레벨 이동
	FBossEncounterSignal OnDeathFinished;

	// 전환 실패 등의 사유로 보스방 초기화 요청
	UPROPERTY(BlueprintAssignable, Category = "Boss")
	FBossEncounterSignal OnEncounterFailed;

	// 페이즈 전환 종료 (BT 전환 Task용)
	FOnBossPhaseTransitionResult OnPhaseTransitionResult;

protected :

	// 보스 페이즈 진행 관리
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UBossPhaseComponent> PhaseComponent;

	// 페이즈 순서 및 전환 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UBossPhaseConfigData> PhaseConfig;

	// 시퀀스에서 이 보스를 연결할 바인딩 태그 (시퀀서 바인딩 Tags와 같은 이름)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Cinematic")
	FName SequenceBindingTag = TEXT("Boss");

	// 보스 체력바에 표시할 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|UI")
	FText BossDisplayName;

private :

	// 같은 보스에 붙은 기믹 계약 구현 컴포넌트
	UPROPERTY(Transient)
	TObjectPtr<UActorComponent> PhaseGimmickComponent;

	// 매니저가 넘긴 PhaseId별 레벨 기믹 액터 (배리어 등)
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<AActor>> LevelGimmickActors;

	// 매니저가 넘긴 기믹 영역 중심 액터
	UPROPERTY(Transient)
	TObjectPtr<AActor> GimmickAreaCenter;

	// 안전한 다음 Tick 실패 처리용 타이머
	FTimerHandle FailureTimerHandle;

	// 초기화 및 사망 시 증가시켜 이전 전투의 지연 콜백 무효화
	int64 EncounterGeneration = 0;

	// 보스방 전투 시작 여부
	bool bEncounterActive = false;

	// 실패 처리 예약 중 새 공격 및 피해를 차단하기 위한 상태
	bool bFailurePending = false;
};
