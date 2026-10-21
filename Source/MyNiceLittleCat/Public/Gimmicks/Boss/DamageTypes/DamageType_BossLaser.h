/// 작성자: 최요환
/// 작성일자: 2026-10-20
///
/// 보스 레이저 피해 타입입니다.
/// UBossLaserComponent가 빔에 맞은 액터에 ApplyDamage 할 때 DamageTypeClass로 넘깁니다.
/// 반사된 레이저(UDamageType_ReflectedLaser)와는 상속 관계가 아니라서
/// 생성장치 AllowedDamageTypes에 ReflectedLaser만 넣으면 보스가 직접 쏜 레이저는 막힙니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "DamageType_BossLaser.generated.h"

UCLASS()
class MYNICELITTLECAT_API UDamageType_BossLaser : public UDamageType
{
	GENERATED_BODY()
};
