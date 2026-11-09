/// 작성자: 최요환
/// 작성일자: 2026-11-05
///
/// 능력 해금 아이템(UItemData ItemType = Ability)의 AbilityTag로 쓰는 네이티브 게임플레이 태그입니다.
/// ini가 아니라 C++로 정의해 다른 담당과 DefaultGameplayTags.ini 충돌을 피합니다.
/// 사용: UInventorySubsystem::HasAbility(ItemAbilityTags::Ability_Pet_Reflect) (BP에서는 태그 이름 "Ability.Pet.Reflect")

#pragma once

#include "NativeGameplayTags.h"

namespace ItemAbilityTags
{
	// 펫 반사판 설치 능력 (스테이지 2 제단 보물 상자에서 획득)
	MYNICELITTLECAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Pet_Reflect);

	// 펫 자성 능력 (스테이지 1 시작 구간 자성 구슬에서 획득) - 추가: 우진 2026-11-07
	MYNICELITTLECAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Pet_Magnet);
}
