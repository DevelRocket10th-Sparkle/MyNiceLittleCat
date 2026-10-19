/// 작성자: 최요환
/// 작성일자: 2026-10-15
///
/// 자성 코인 피해 타입입니다.
/// 플레이어가 날린 자성 코인이 ApplyDamage 할 때 DamageTypeClass로 넘깁니다.
/// 70% 페이즈 배리어 생성장치는 이 타입의 피해만 받도록 설정합니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "DamageType_MagneticCoin.generated.h"

UCLASS()
class MYNICELITTLECAT_API UDamageType_MagneticCoin : public UDamageType
{
	GENERATED_BODY()
};
