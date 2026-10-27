#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnemyPhaseGimmickInterface.generated.h"

// 보스가 별도 담당자의 기믹 컴포넌트를 구체 클래스 의존 없이 호출하는 계약
UINTERFACE(BlueprintType)
class MYNICELITTLECAT_API UEnemyPhaseGimmickInterface : public UInterface
{
	GENERATED_BODY()
};

class MYNICELITTLECAT_API IEnemyPhaseGimmickInterface
{
	GENERATED_BODY()
public:
	// 페이즈 진입: 전달받은 번호를 저장하고 완료 시 보스에 같은 번호로 통지; 시작 실패는 false
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Boss|Gimmick")
	bool EnterPhase(FName PhaseId, int64 PhaseExecutionId);
	// 전환·사망·초기화 시 남은 기믹과 타이머·구독 정리; 여러 번 호출해도 안전해야 함
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Boss|Gimmick")
	void StopGimmicks();
	// 보스 초기화 중 매니저가 넘긴 레벨 기믹 참조 전달: 키는 PhaseId, 첫 페이즈 진입 전에 호출
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Boss|Gimmick")
	void ReceiveLevelGimmickReferences(const TMap<FName, AActor*>& PhaseActors, AActor* AreaCenter);
};
