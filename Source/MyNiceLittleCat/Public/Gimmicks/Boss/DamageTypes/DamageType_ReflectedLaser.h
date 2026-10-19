/// 작성자: 최요환
/// 작성일자: 2026-10-15
///
/// 반사된 레이저 피해 타입입니다.
/// 보스 레이저가 펫의 반사판에 반사된 뒤 ApplyDamage 할 때 DamageTypeClass로 넘깁니다.
/// 30% 페이즈 배리어 생성장치는 이 타입의 피해만 받도록 설정합니다. (반사 전 레이저와 구분)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "DamageType_ReflectedLaser.generated.h"

UCLASS()
class MYNICELITTLECAT_API UDamageType_ReflectedLaser : public UDamageType
{
	GENERATED_BODY()
};
