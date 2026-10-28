#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Enemy/Datas/EnemyPatternSetData.h"
#include "BTTask_Enemy_PatternMovement.generated.h"

class AAIController;
class UEnemyPatternComponent;

// Simple Parallel의 배경에서 공격과 후딜 동안 유지한다. 도착해도 종료하지 않고 재추격을 감시한다.
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

	TWeakObjectPtr<AAIController> CachedAI;
	TWeakObjectPtr<UEnemyPatternComponent> CachedPatternComp;
	TWeakObjectPtr<AActor> CachedTarget;
	FEnemyPatternMovementSettings CachedMovement;
	FAIRequestID OwnedMoveId = FAIRequestID::InvalidRequest;
	FDelegateHandle RecoveryStartedHandle;
	int64 PatternRequestId = 0;
	float UpdateRemaining = 0.f;
	bool bHasPolicy = false;

	friend class FEnemyPatternMovementTest;
};
