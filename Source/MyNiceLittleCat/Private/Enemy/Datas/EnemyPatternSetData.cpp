#include "Enemy/Datas/EnemyPatternSetData.h"

#if WITH_EDITOR
// 전용 객체의 판정 배열은 해당 설정 클래스에서 편집 처리
void UEnemyPatternSetData::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// GA별 설정의 편집 처리는 전용 객체에 위임하여 공통 에셋이 판정 방식을 알 필요가 없게 함
	for (FEnemyPatternDefinition& Pattern : Patterns)
		if (Pattern.ExecutionSettings) Pattern.ExecutionSettings->PostEditChangeChainProperty(PropertyChangedEvent);
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif
