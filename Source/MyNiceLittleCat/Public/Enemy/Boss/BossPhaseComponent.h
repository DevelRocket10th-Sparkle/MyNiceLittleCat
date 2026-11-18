// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.20.
* 수정 일자: 2026.11.17. (EnemyPhaseComponent → BossPhaseComponent, 시퀀스 재생/Stage3 의존 제거)
* 보스 페이즈 진행 관리
* - 현재 페이즈, 체력 하한, 기믹 완료에 의한 진행, 전환 요청/완료
* - 전환 연출은 직접 재생하지 않음: 레벨 담당자(IBossPhaseTransitionHandler)에게 맡기거나
*   페이즈 설정의 기본 시퀀스를 UCinematicSubsystem에 요청하고 완료 통지만 기다림
* - 페이즈 기본 이동(추격/정지/경로 순회)을 IEnemyLocomotionProvider로 BT Task에 제공
*
* 전환 흐름
*   RequestNextPhase (체력/기믹) → TransitionPending → [BT] BeginPendingTransition → Transitioning
*   → 연출(담당자 또는 기본 시퀀스) → NotifyTransitionPresentationFinished → 다음 페이즈 적용 → Active
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Cinematics/CinematicTypes.h"
#include "Enemy/AI/EnemyLocomotionProvider.h"
#include "Enemy/Data/BossMovementTypes.h"
#include "Enemy/Data/BossPhaseConfigData.h"

#include "BossPhaseComponent.generated.h"

class IBossPhaseTransitionHandler;

// 현재 페이즈와 별개로 관리하는 진행 상태
UENUM(BlueprintType)
enum class EBossPhaseRunState : uint8
{
	Stopped,			// 시작 전 또는 정지
	Active,				// 페이즈 진행 중: 공격 및 체력 조건 평가 가능
	TransitionPending,	// 다음 페이즈 요청됨: BT가 공격 종료 후 전환 시작
	Transitioning		// 전환 연출 또는 다음 페이즈 적용 중
};

// 보스가 페이즈별 공격 목록과 기믹을 적용하고 성공 여부 반환
DECLARE_DELEGATE_RetVal_OneParam(bool, FApplyBossPhase, const FBossPhaseDefinition&);

// 전환 요청 즉시 알림: 보스가 공격과 피해를 막음
DECLARE_MULTICAST_DELEGATE(FOnBossPhaseTransitionRequested);

// 전환 완료(다음 페이즈 적용) 또는 실패 알림
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBossPhaseTransitionFinished, int64 /*TransitionId*/, bool /*bSucceeded*/);

UCLASS(ClassGroup = (Boss), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UBossPhaseComponent : public UActorComponent, public IEnemyLocomotionProvider
{
	GENERATED_BODY()

public :

	// Tick 없이 이벤트와 콜백으로만 동작
	UBossPhaseComponent();

	/* 초기화, 시작, 정지 */

	// 서버에서 페이즈 설정과 이동 경로 검사: 실패하면 false
	bool InitializePhases(UBossPhaseConfigData* InConfig);

	// Setter: 레벨의 이동 경로 (보스방 매니저가 생성 시 전달)
	void SetMovementRoutes(const TMap<FName, FBossMovementRoute>& InRoutes);

	// Setter: 레벨의 전환 연출 담당자 (없으면 nullptr)
	void SetTransitionHandler(UObject* InHandler);

	// 첫 페이즈 진입: 정지 후 재사용하지 않고 보스를 재생성하여 초기화하는 구조
	bool StartPhases();

	// 실행 번호를 먼저 무효화한 뒤 연출과 상태 정리 → 정리 중 발생한 완료 통지가 진행시키지 못함
	void StopAndInvalidate();

	/* 페이즈 진행 조건 */

	// 현재 전투 페이즈의 체력 하한 도달 여부 검사
	void EvaluateHealth(float Health, float MaxHealth);

	// 현재 배리어의 완료만 검증하여 다음 전환 요청: 지연 콜백은 진입 시 받은 번호를 그대로 전달
	UFUNCTION(BlueprintCallable, Category = "Boss|Phase")
	bool RequestNextPhaseFromGimmick(FName PhaseId, int64 PhaseExecutionId);

	// 현재 페이즈에서 피해로 내려갈 수 있는 최소 체력
	float GetDamageHealthFloor(float MaxHealth) const;

	/* 전환 */

	// 대기 중인 전환 시작: 연출 시작(또는 연출 없음이면 즉시 완료)
	bool BeginPendingTransition();

	// 전환 연출 종료 통지: 번호가 현재 전환과 같을 때만 다음 페이즈 적용
	bool NotifyTransitionPresentationFinished(int64 TransitionId);

	// Getter: 현재 전환 요청 번호 (0이면 없음)
	int64 GetActiveTransitionId() const { return ActiveTransitionId; }

	/* 상태 조회 */

	// 현재 페이즈 설정: 시작 전에는 nullptr
	const FBossPhaseDefinition* GetCurrentPhase() const;

	// Getter: 초기화에 사용한 페이즈 설정 (체력바 경계 표시용)
	const UBossPhaseConfigData* GetConfig() const { return Config; }

	// Getter: 현재 페이즈 ID
	UFUNCTION(BlueprintPure, Category = "Boss|Phase")
	FName GetCurrentPhaseId() const;

	// Getter: 현재 페이즈 진입 번호: 전환 요청 또는 정지 이후에는 0
	UFUNCTION(BlueprintPure, Category = "Boss|Phase")
	int64 GetCurrentPhaseExecutionId() const { return ActivePhaseExecutionId; }

	// 일반 공격 및 체력 조건 평가가 가능한 상태인지 확인
	bool IsActive() const { return RunState == EBossPhaseRunState::Active; }

	// BT가 처리해야 하는 전환 요청이 있는지 확인
	bool HasPendingTransition() const { return RunState == EBossPhaseRunState::TransitionPending; }

	// 전환 연출 또는 페이즈 적용 처리가 진행 중인지 확인
	bool IsTransitioning() const { return RunState == EBossPhaseRunState::Transitioning; }

	// 전환 연출이 끝나고 플레이어 도착을 기다리는 중인지 (전환 담당자에게 확인, 체력바 안내용)
	bool IsWaitingForPlayer() const;

	// 현재 경로 지점 좌표: 실제로 Waypoints 이동 중인지는 호출자가 확인
	bool GetCurrentWaypoint(FVector& OutLocation) const;

	/* IEnemyLocomotionProvider: 페이즈 기본 이동 */

	// 현재 페이즈 진입 번호: 페이즈가 바뀌면 이전 이동 Task 종료
	virtual int64 GetLocomotionContextId() const override;

	// 페이즈 이동 모드(패턴 예외 포함)를 이동 의도로 변환
	virtual bool ResolveLocomotion(FName PatternId, FEnemyLocomotionIntent& OutIntent) const override;

	// 경로 지점 도착: 다음 지점으로 진행 (이전 페이즈의 늦은 통지는 무시)
	virtual void NotifyLocomotionDestinationReached(int64 ContextId) override;

public :

	/* 이벤트 */

	// 전환 요청 즉시 알림
	FOnBossPhaseTransitionRequested OnTransitionRequested;

	// 전환 완료/실패 알림
	FOnBossPhaseTransitionFinished OnTransitionFinished;

	// 보스가 페이즈별 공격 목록과 기믹을 적용하는 함수
	FApplyBossPhase ApplyPhase;

protected :

	// 플레이 종료 시 실행, 구독 및 직접 소유한 자원 정리 후 부모 호출
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private :

	/* 페이즈 진행 */

	// 현재 페이즈 다음 인덱스의 전환을 중복 없이 요청하고 즉시 보호 처리 통지
	void RequestNextPhase();

	// 현재 페이즈 적용: 적용 중 즉시 완료되는 기믹은 활성화 이후 처리하도록 구간 관리
	bool ApplyCurrentPhase();

	/* 전환 연출 */

	// 담당자 → 기본 시퀀스 → 연출 없음 순서로 전환 연출 시작
	void StartTransitionPresentation();

	// 기본 전환 시퀀스 종료 콜백
	void HandleDefaultPresentationEnded(FCinematicHandle Handle, ECinematicEndReason Reason);

	// 진행 중인 연출 취소 (완료 통지 없음)
	void CancelTransitionPresentation(int64 TransitionId);

	// 연출이 끝난 전환의 다음 페이즈 적용
	void CompleteTransition();

	// 현재 설정된 전환 담당자
	IBossPhaseTransitionHandler* GetTransitionHandler() const;

	/* 이동 경로 */

	// 페이즈가 사용하는 경로가 레벨에 모두 등록됐는지 검사
	bool ValidateMovementRoutes(FString& OutError) const;

	// 경로 순회 상태 초기화 (MovementRoutes는 유지)
	void ResetWaypointProgress();

private :

	/* 설정 */

	// 공유 페이즈 설정 데이터
	UPROPERTY(Transient)
	TObjectPtr<UBossPhaseConfigData> Config;

	// 레벨에 배치된 경로: 매니저가 보스 생성 시 전달
	UPROPERTY(Transient)
	TMap<FName, FBossMovementRoute> MovementRoutes;

	// 전환 연출 담당자 (IBossPhaseTransitionHandler 구현 객체)
	TWeakObjectPtr<UObject> TransitionHandlerObject;

	/* 페이즈 상태 */

	// 진행 상태
	EBossPhaseRunState RunState = EBossPhaseRunState::Stopped;

	// 현재 적용된 페이즈 인덱스
	int32 CurrentPhaseIndex = INDEX_NONE;

	// 다음으로 적용할 페이즈 인덱스
	int32 PendingPhaseIndex = INDEX_NONE;

	// 페이즈 진입 번호 발급용: 재초기화 시에도 번호를 재사용하지 않음
	int64 NextPhaseExecutionId = 0;

	// 현재 페이즈 진입 번호: 늦은 기믹 완료 이벤트를 거부하는 기준
	int64 ActivePhaseExecutionId = 0;

	// 진입 함수가 실행 중인지: 진입 중 동기 완료를 안전하게 지연
	bool bApplyingPhase = false;

	// 진입 함수 안에서 받은 기믹 완료 요청을 활성화 직후 한 번만 처리
	bool bPendingGimmickCompletion = false;

	/* 전환 상태 */

	// 전환 번호 발급용: 정지 시에도 이전 번호로 되돌리지 않음
	int64 NextTransitionId = 0;

	// 현재 전환 요청 번호: 0은 유효한 요청 없음
	int64 ActiveTransitionId = 0;

	// 전환 연출이 진행 중인지 (완료 통지 대기)
	bool bPresentationRunning = false;

	// 현재 연출을 레벨 담당자가 진행 중인지
	bool bPresentationByHandler = false;

	// 기본 전환 시퀀스 재생 핸들
	FCinematicHandle PresentationCinematic;

	/* 경로 순회 상태 */

	int32 CurrentWaypointIndex = 0;
	int32 WaypointDirection = 1;
};
