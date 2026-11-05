/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 레이저 빛의 색 종류입니다 (스테이지 2 색 퍼즐: 색 필터·색 수신기).
/// White는 색이 없는 기본 빛이라 빔 이펙트는 레이저의 VisualSettings 색을 그대로 쓰고(레이저의 bUseColorTypeDisplay가 켜져 있으면 흰빛),
/// 나머지 색은 GetLaserBeamDisplayColor의 색으로 표시합니다. 색 섞기는 없습니다.

#pragma once

#include "CoreMinimal.h"
#include "LaserBeamColor.generated.h"

UENUM(BlueprintType)
enum class ELaserBeamColor : uint8
{
	// 색 없는 기본 빛 (필터를 통과하면 필터 색이 됨)
	White,
	Red,
	Blue,
	Green
};

// 빔 이펙트 색 (White = 흰빛, 색 없는 빛은 레이저가 색 종류로 표시할 때만 사용)
inline FLinearColor GetLaserBeamDisplayColor(ELaserBeamColor Color)
{
	switch (Color)
	{
	case ELaserBeamColor::Red:
		return FLinearColor(1.f, 0.08f, 0.05f);
	case ELaserBeamColor::Blue:
		return FLinearColor(0.1f, 0.35f, 1.f);
	case ELaserBeamColor::Green:
		return FLinearColor(0.15f, 1.f, 0.2f);
	default:
		return FLinearColor(0.9f, 0.95f, 1.f);
	}
}
