// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 수정 일자: 2026.11.17. (보스/페이즈 타입 의존 제거 → IEnemyLocomotionProvider 사용)
* 공격 패턴과 기본 이동 규칙을 합쳐 이동을 결정하는 Task
* - 기본 이동 제공자가 있으면(보스 페이즈 등) 그 규칙이 바뀔 때까지 Task 유지
* - 없으면(일반 Enemy) 실행 중인 공격 패턴이 끝날 때까지만 패턴 이동 처리
*
* 이동 결정 순서
*   1) 공격 실행 중 패턴 정책이 Stop/AbilityDriven → 패턴이 결정
*   2) 그 외(공격 전 방향 정렬, FollowLocomotion, 후딜) → 기본 이동 규칙 (후딜은 이동 잠금으로 정지)
*   3) 기본 이동 제공자가 없으면 정지
*/

#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "UObject/WeakInterfacePtr.h"
#include "Enemy/AI/EnemyLocomotionProvider.h"
#include "BTTask_Enemy_PatternMovement.generated.h"

class AAIController;
class AEnemyCharacter;
class UEnemyPatternComponent;
class UPathFollowingComponent;
struct FPathFollowingResult;

UCLASS()
class MYNICELITTLECAT_API UBTTask_Enemy_PatternMovement : public UBTTaskNode
{
	GENERATED_BODY()

public :

	// 인스턴스 생성, Tick 및 종료 알림 사용, 타깃 키 필터 설정
	UBTTask_Enemy_PatternMovement();

	// 추격 대상 Blackboard 키 (기본값 TargetActor: 기존 BT 에셋은 다시 설정하지 않아도 동작)
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetKey;

	// 이동 정책 주기 재평가 간격: 상태 변화가 있으면 주기와 관계없이 즉시 재평가
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.05", Units = "s"))
	float UpdateInterval = 0.2f;

protected :

	// 타깃 키를 BB 에셋 기준으로 해석
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

	// 기본 이동 제공자 또는 실행 중인 패턴을 확인하고 이동 시작
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// 종료 조건 확인, 상태 변화 감지, 주기 재평가
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	// 브랜치 중단 시 이 Task가 만든 이동만 취소
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// Task 완료 및 중단 모두 구독과 캐시 정리
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

private :

	// 이 Task가 최종적으로 적용하는 이동 종류
	enum class EResolvedMove : uint8
	{
		Stop,
		Chase,
		MoveToLocation,
		AbilityDriven
	};

	// 현재 상태로 이동 종류를 계산하고 필요하면 이동 요청 갱신
	void RefreshMovement();

	// 이 Task가 발행한 이동 요청 종료 (도착 시 경로 진행)
	void OnMoveFinished(FAIRequestID RequestId, const FPathFollowingResult& Result);

	// 이 Task가 발행한 이동만 취소: 다른 GA나 행동이 만든 새 이동은 건드리지 않음
	void CancelOwnedMove();

	// Task 종료 조건: 기본 이동 규칙 변경 또는 담당 패턴 종료
	bool ShouldFinish() const;

	// 구독 해제와 캐시 초기화
	void Cleanup();

	// Blackboard 타깃 조회
	AActor* GetBlackboardTarget() const;

	static FAIMoveRequest BuildChaseRequest(AActor* Target, float AcceptanceRadius);
	static FAIMoveRequest BuildLocationRequest(const FVector& Destination, float AcceptanceRadius);

private :

	/* 실행 대상 */

	TWeakObjectPtr<AAIController> CachedAI;
	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;
	TWeakObjectPtr<UPathFollowingComponent> CachedPath;
	TWeakObjectPtr<UEnemyPatternComponent> CachedPatternComp;
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;

	// 기본 이동 제공자: 없으면 패턴 이동만 처리
	TWeakInterfacePtr<IEnemyLocomotionProvider> CachedLocomotion;

	// Task 시작 시 기본 이동 규칙 번호: 바뀌면 Task 종료 (0이면 패턴 이동 모드)
	int64 LocomotionContextId = 0;

	// 패턴 이동 모드에서 담당하는 패턴 요청 번호
	int64 PatternRequestId = 0;

	/* 구독 */

	FDelegateHandle RecoveryStartedHandle;
	FDelegateHandle MoveFinishedHandle;

	// 이 Task가 발행한 AI 이동 요청 ID
	FAIRequestID OwnedMoveId = FAIRequestID::InvalidRequest;

	/* 상태 변화 감지: 직전 확인 값과 다르면 주기를 기다리지 않고 즉시 재평가 */

	int64 PreviousRequestId = 0;
	bool bPreviousRecovering = false;
	bool bPreviousLocked = false;
	FName PreviousPatternId;

	// 다음 주기 재평가까지 남은 시간
	float UpdateRemaining = 0.f;

	/* 직전에 적용한 이동 */

	bool bHasAppliedMove = false;
	EResolvedMove AppliedMove = EResolvedMove::Stop;
	float AppliedAcceptanceRadius = 0.f;
	TWeakObjectPtr<AActor> AppliedTarget;
	FVector AppliedDestination = FVector::ZeroVector;
};
