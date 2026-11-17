#include "Enemy/GAS/EnemyCombatTags.h"

namespace EnemyCombatTags
{
	UE_DEFINE_GAMEPLAY_TAG(MeleeWindowBegin, "Event.Enemy.Melee.Window.Begin");
	UE_DEFINE_GAMEPLAY_TAG(MeleeWindowEnd, "Event.Enemy.Melee.Window.End");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(PatternResource, "Enemy.Pattern.Resource", "동시 실행 패턴이 함께 쓸 수 없는 기술 자원");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(PatternResourceBody, "Enemy.Pattern.Resource.Body", "보스 몸: 이동, 회전, 몽타주를 제어하는 패턴");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(PatternHazard, "Enemy.Pattern.Hazard", "겹치면 회피가 어려운 위험 영역");
}
