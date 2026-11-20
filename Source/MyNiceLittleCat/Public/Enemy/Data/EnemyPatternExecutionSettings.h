/*
* 작성자: 전선현
* 수정 일자: 2026.11.12.
* 수정 일자: 2026.11.20. (값 범위·실행 시간 검사를 가상 함수로: GA 시작 전 검사와 에셋 검사 공용)
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

public:

	// 모든 값이 실행 가능한 범위인지 (검사할 값이 없는 설정은 true)
	virtual bool IsValidConfiguration() const { return true; }

	// 연출 전체 시간 + 끝 처리 여유가 패턴 MaxExecutionSeconds보다 짧은지: 정상 종료가 시간 초과보다 먼저 와야 함
	// - 실행 시간이 정해지지 않은 설정(근접 등)은 true
	// - GA 시작 전 검사와 패턴 세트 에셋 검사(IsDataValid)가 같은 함수를 씀
	virtual bool FitsExecutionTime(float MaxExecutionSeconds) const { return true; }

protected:

	// 끝 처리(마지막 낙하, 정리) 여유 기본값 (초)
	static constexpr float DefaultExecutionTimeMargin = 0.5f;

};
