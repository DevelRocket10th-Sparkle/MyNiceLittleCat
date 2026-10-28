#pragma once
#include "NativeGameplayTags.h"

// 근접 판정 구간을 Notify State에서 공격 GA에 전달하는 이벤트
namespace EnemyCombatTags
{
	MYNICELITTLECAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(MeleeWindowBegin);
	MYNICELITTLECAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(MeleeWindowEnd);
}
