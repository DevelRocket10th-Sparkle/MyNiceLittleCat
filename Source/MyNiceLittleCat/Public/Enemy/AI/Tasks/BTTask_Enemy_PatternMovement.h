#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Enemy/Datas/EnemyPatternSetData.h"
#include "BTTask_Enemy_PatternMovement.generated.h"

class AAIController;
class UEnemyPatternComponent;
class ABossCharacter;
class UEnemyPhaseComponent;

// 보스는 활성 페이즈 동안 기본 이동과 패턴 이동 처리
// 일반 몬스터는 공격과 후딜 동안 패턴 이동 처리
UCLASS()
class MYNICELITTLECAT_API UBTTask_Enemy_PatternMovement : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_Enemy_PatternMovement();

	UPROPERTY(EditAnywhere, Category="Movement", meta=(ClampMin="0.05", Units="s"))
	float UpdateInterval = 0.2f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

private:
	void RefreshMovement();
	void CancelOwnedMove();
	void Cleanup();
	static FAIMoveRequest BuildMoveRequest(AActor* Target, const FEnemyPatternMovementSettings& Settings);

private :

	FDelegateHandle RecoveryStartedHandle;

	TWeakObjectPtr<AAIController> CachedAI;
	TWeakObjectPtr<UEnemyPatternComponent> CachedPatternComp;
	TWeakObjectPtr<AActor> CachedTarget;
	FEnemyPatternMovementSettings CachedMovement;

	// 이 Task가 발행한 AI 이동 요청 ID
	// -> 다른 GA나 행동이 만든 새 이동을 취소하지 않도록 비교
	FAIRequestID OwnedMoveId = FAIRequestID::InvalidRequest;

	// 일반 Enemy의 Task가 담당하는 패턴 요청 번호
	// -> 보스는 개별 패턴 종료가 아닌 페이즈 종료까지 Task 유지
	int64 PatternRequestId = 0;

	// 다음 주기 갱신까지 남은 시간 (초)
	// - Tick마다 차감
	// - 0 이하가 되면 RefreshMovement() 실행 후 UpdateInterval로 재설정
	float UpdateRemaining = 0.f;
	
	// 이동 설정을 한 번 이상 적용했는지 여부
	bool bHasPolicy = false;

	friend class FEnemyPatternMovementTest;

	// 이동을 처리할 보스 참조: 일반 몬스터에서는 비어 있음
	TWeakObjectPtr<ABossCharacter> CachedBoss;
	
	// 보스의 기본 이동 설정과 현재 경로 지점을 조회할 페이즈 컴포넌트
	TWeakObjectPtr<UEnemyPhaseComponent> CachedPhaseComp;

	// 태스크 시작 시 저장한 페이즈 실행 번호
	// - 페이즈 변경 감지와 지점 도착 검증에 사용
	// - 0이면 일반 Enemy의 패턴 이동으로 실행 중이거나 초기화된 상태
	int64 CachedPhaseExecutionId = 0;

	// 현재 적용한 이동이 Waypoints 이동인지 구분
	bool bCachedWaypointMovement = false;

	// 현재 요청에 사용한 목적지
	FVector CachedDestination = FVector::ZeroVector;

	// 직전 확인에서 실행 중이던 패턴 요청 번호 (없으면 0)
	int64 PreviousRequestId = 0;

	// 직전 확인의 후딜 상태
	bool bPreviousRecovering = false;

	// 직전 확인의 공격 이동 잠금 상태
	bool bPreviousLocked = false;
};
