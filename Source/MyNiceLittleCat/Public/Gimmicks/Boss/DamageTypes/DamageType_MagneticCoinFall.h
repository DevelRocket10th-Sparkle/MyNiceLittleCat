/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 자성 코인 낙하 피해 타입입니다.
/// 낙하 중인 자성 코인이 플레이어에게 ApplyDamage 할 때 DamageTypeClass로 넘깁니다.
/// 플레이어가 던진 코인 피해(UDamageType_MagneticCoin)와 구분하기 위한 타입입니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "DamageType_MagneticCoinFall.generated.h"

UCLASS()
class MYNICELITTLECAT_API UDamageType_MagneticCoinFall : public UDamageType
{
	GENERATED_BODY()
};
