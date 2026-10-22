#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EnemyPatternExecutionSettings.generated.h"

// 패턴 GA가 해석하는 전용 설정의 공통 타입; 선택과 요청 관리는 이 내용을 해석하지 않음
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class MYNICELITTLECAT_API UEnemyPatternExecutionSettings : public UObject
{
	GENERATED_BODY()
};
