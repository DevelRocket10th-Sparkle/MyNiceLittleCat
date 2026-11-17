#pragma once
#include "NativeGameplayTags.h"

namespace EnemyCombatTags
{
	/* 이벤트 */

	// 근접 판정 구간을 Notify State에서 공격 GA에 전달하는 이벤트
	MYNICELITTLECAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(MeleeWindowBegin);
	MYNICELITTLECAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(MeleeWindowEnd);

	/* 패턴 동시 실행 */

	// 기술 자원: 실행 중인 패턴끼리 같은 자원을 쓰면 동시에 실행하지 않음 (GA 종료 시 해제)
	MYNICELITTLECAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PatternResource);

	// 보스 몸(이동, 회전, 몽타주): 몸을 쓰는 패턴은 BT가 고른 주 패턴으로만 실행
	MYNICELITTLECAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PatternResourceBody);

	// 위험 영역: 같은 위험 태그끼리 겹치지 않음. 생성물이 남아 있으면 GA가 끝나도 유지
	MYNICELITTLECAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PatternHazard);
}
