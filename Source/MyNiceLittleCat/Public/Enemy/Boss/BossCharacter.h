// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.20.
* 보스 전용 기능 구현을 위한 EnemyCharacter 상속 클래스
*/

#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyCharacter.h"

#include "Enemy/Boss/Datas/BossPhaseConfigData.h"

#include "BossCharacter.generated.h"

class UBossPhaseComponent;
class ABossCharacter;

/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBossEncounterSignal, ABossCharacter*, Boss);
UCLASS()
class MYNICELITTLECAT_API ABossCharacter : public AEnemyCharacter
{
	GENERATED_BODY()

public :

	ABossCharacter(const FObjectInitializer& ObjectInitializer);

	// 초기화와 생존 및 보스방 전투 상태를 포함한 AI 실행 가능 여부 확인
	virtual bool CanRunEnemyAI() const override;

	// Health 변경 전에 무적 및 페이즈 체력 하한을 반영
	virtual float CalculateHealthAfterDamage(float CurrentHealth, float MaxHealth, float Damage) const override;

	// 초기화가 끝난 보스의 첫 페이즈 및 BT 시작
	// 준비된 보스방의 첫 페이즈와 타깃 선정 시작
	bool StartEncounter();
	// 전투 상태를 먼저 무효화한 후 공격, 전환 및 기믹 정리
	// 세션을 먼저 무효화한 뒤 전투 전체 실행과 기믹 정리
	void StopEncounter();
	// Task 콜백 도중 보스를 파괴하지 않도록 다음 Tick에 실패 통지
	// 안전한 다음 Tick에 전투 실패를 전달하도록 예약
	void ScheduleEncounterFailure();
	// Getter: 보스방 전투가 진행 중인지 확인
	bool IsEncounterActive() const { return bEncounterActive && !bFailurePending; }
	// Getter: 페이즈 실행 컴포넌트
	UBossPhaseComponent* GetPhaseComponent() const { return PhaseComponent; }
	// 전환 완료 후 무적과 공격 차단을 해제하고 Blackboard 갱신
	void FinishPhaseTransition();
	// 현재 보스 상태를 Blackboard 판단 값에 반영
	// 현재 보스 상태를 일반 행동 및 전환 판단 키에 반영
	void RefreshBossBlackboard();
	// 현재 연결한 기믹 컴포넌트의 완료만 페이즈 관리에 전달
	UFUNCTION(BlueprintCallable, Category = "Boss|Gimmick")
	bool NotifyPhaseGimmickCompleted(UActorComponent* SourceComponent, FName PhaseId, int64 PhaseExecutionId);
	// 보스방 관리자가 보스 사망을 받아 클리어 처리하는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Boss")
	FBossEncounterSignal OnDefeated;
	// 전환 실패 등의 사유로 보스방 초기화를 요청하는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Boss")
	FBossEncounterSignal OnEncounterFailed;

protected :
	// 기본 Enemy 초기화 후 페이즈 설정, 패턴 ID 및 배리어 연결 검사
	// 기본 초기화 뒤 파생 캐릭터의 추가 설정 준비
	virtual bool InitializeEnemyExtensions() override;
	// 사망 처리를 먼저 수행한 뒤 살아 있는 보스의 체력 조건 평가
	// 체력 변경 시 사망 판정을 먼저 수행한 뒤 추가 상태 처리
	virtual void HandleHealthChanged(float NewHealth, float MaxHealth) override;
	// 페이즈와 기믹 정리 후 기존 Enemy 사망 연출 수행
	// 중복 사망을 막고 공격과 AI를 정리한 뒤 사망 연출 시작
	virtual void BeginDeath() override;
	// 액터 제거 시 전투 실행 및 델리게이트 구독 정리
	// 플레이 종료 시 실행, 구독 및 직접 소유한 자원 정리 후 부모 호출
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// 전환 요청 즉시 공격 및 피해를 막고 기존 공격 취소
	void HandleTransitionRequested();
	// 새 페이즈의 공격 목록 및 구체적인 기믹을 적용
	// 페이즈별 공격 목록 및 배리어와 코인 기믹 적용
	bool HandlePhaseEntered(const FBossPhaseDefinition& Phase);

	// 보스 페이즈의 체력 하한 및 전환 상태 관리
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UBossPhaseComponent> PhaseComponent;
	// 페이즈 순서 및 전환 보호 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UBossPhaseConfigData> PhaseConfig;

private :
	// 보스에 부착된 기믹 계약 구현 컴포넌트 하나를 찾아 연결; 실제 기믹 클래스는 별도 담당
	bool InitializeGimmickConnection();
	// 연결한 기믹의 실행 중지 호출: 기믹 구현 내부에서 액터·타이머·구독 정리
	void StopPhaseGimmicks();
	// 같은 보스에 붙은 계약 구현 컴포넌트; 매니저의 레벨 참조 전달은 별도 작업
	UPROPERTY(Transient)
	TObjectPtr<UActorComponent> PhaseGimmickComponent;
	// 안전한 다음 Tick 실패 처리용 타이머
	FTimerHandle FailureTimerHandle;
	// 초기화 및 사망 시 증가시켜 이전 전투의 지연 콜백 무효화
	int64 EncounterGeneration = 0;
	// 보스방 전투 시작 여부
	bool bEncounterActive = false;
	// 실패 처리 예약 중 새 공격 및 피해를 차단하기 위한 상태
	bool bFailurePending = false;

	
};
