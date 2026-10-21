#pragma once
#include "NativeGameplayTags.h"

// 코드와 GE가 공통으로 사용하는 전투 상태 태그
namespace EnemyCombatTags
{
	// 공격 몽타주의 판정 Notify가 GA에 전달하는 이벤트
	MYNICELITTLECAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PatternHit);
	MYNICELITTLECAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(MeleeWindowBegin);
	MYNICELITTLECAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(MeleeWindowEnd);
}
