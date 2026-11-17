/*
* 작성자: 전선현
* 수정 일자: 2026.11.12.
* 패턴별 GA 전용 실행 설정의 공통 부모
* - 패턴 데이터 에셋 안에서 인라인 생성 (별도 에셋 불필요)
* - 각 GA가 자기 설정 타입으로 Cast 해석: 공통 패턴 데이터는 GA별 설정 내용을 모름
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EnemyPatternExecutionSettings.generated.h"

UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class MYNICELITTLECAT_API UEnemyPatternExecutionSettings : public UObject
{
	GENERATED_BODY()

};
