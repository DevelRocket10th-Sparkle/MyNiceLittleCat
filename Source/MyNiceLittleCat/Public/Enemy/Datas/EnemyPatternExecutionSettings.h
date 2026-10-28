#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EnemyPatternExecutionSettings.generated.h"

// GA가 해석하는 전용 실행 설정의 공통 타입이다.
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class MYNICELITTLECAT_API UEnemyPatternExecutionSettings : public UObject
{
	GENERATED_BODY()
public:
	// 이전 에셋을 읽어 패턴 공통 설정으로 이전할 때만 사용한다. 실행과 편집에는 사용하지 않는다.
	UPROPERTY()
	float RecoverySeconds = 0.f;
};
