#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BossPhaseGimmickInterface.generated.h"

// 보스가 별도 담당자의 기믹 컴포넌트를 구체 클래스 의존 없이 호출하는 계약
UINTERFACE(BlueprintType)
class MYNICELITTLECAT_API UBossPhaseGimmickInterface : public UInterface
{
	GENERATED_BODY()
};

class MYNICELITTLECAT_API IBossPhaseGimmickInterface
{
	GENERATED_BODY()
public:
	// 페이즈 진입: 전달받은 번호를 저장하고 완료 시 보스에 같은 번호로 통지; 시작 실패는 false
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Boss|Gimmick")
	bool EnterBossPhase(FName PhaseId, int64 PhaseExecutionId);
	// 전환·사망·초기화 시 남은 기믹과 타이머·구독 정리; 여러 번 호출해도 안전해야 함
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Boss|Gimmick")
	void StopBossGimmicks();
};
