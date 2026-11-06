/// 작성자: 최요환
/// 작성일자: 2026-11-05
///
/// 스테이지 기믹의 회전 방식입니다 (거울·빛 장치 공통).
/// 이름은 처음 만든 거울 기준(EGimmickMirrorRotateMode) 그대로 둡니다 (BP·맵에 저장된 값 유지).

#pragma once

#include "CoreMinimal.h"
#include "GimmickRotateMode.generated.h"

UENUM(BlueprintType)
enum class EGimmickMirrorRotateMode : uint8
{
	// 고정
	Fixed,
	// E로 상호작용할 때마다 회전
	Interact,
	// 신호가 켜질 때마다 회전
	Signal
};
