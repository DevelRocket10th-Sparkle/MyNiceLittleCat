#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_Boss_TransitionPhase.generated.h"

class ABossCharacter;

// 전환 연출이 종료될 때까지 대기하고 다음 페이즈를 적용하는 Task
UCLASS()
class MYNICELITTLECAT_API UBTTask_Boss_TransitionPhase : public UBTTaskNode
{
	GENERATED_BODY()
public :
	// 보스별 Task 인스턴스와 결과 확인 Tick 설정
	UBTTask_Boss_TransitionPhase();
protected :
	// 기존 공격의 종료를 확인한 뒤 전환 GA 시작
	// Task 시작 시 행동을 요청하고 비동기 완료까지 InProgress 반환
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	// 전환 결과를 확인하고 성공 시 기믹 적용, 실패 시 보스방 초기화 요청
	// 요청의 종료 결과를 확인하여 BT Task 완료
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	// 외부 중단은 전환 실패로 처리하되 사망 및 초기화 도중에는 재요청하지 않음
	// 브랜치 중단 시 이 Task가 시작한 요청만 취소
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	// Task 종료 시 참조 및 실행 번호 정리
	// Task 완료 및 중단 모두 실행 참조와 요청 번호 정리
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
private :
	// 이 Task가 전환을 요청한 보스
	TWeakObjectPtr<ABossCharacter> Boss;
	// Task가 소유한 전환 요청 번호
	int64 TransitionId = 0;
};
