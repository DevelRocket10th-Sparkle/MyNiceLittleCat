#include "Enemy/Datas/EnemyPatternSetData.h"
#include "Serialization/CustomVersion.h"
#include "Enemy/Datas/MoneyGunPatternSettings.h"
#include "Enemy/GAS/Abilities/GA_Enemy_RushAttack.h"

namespace EnemyPatternSetVersion
{
	const FGuid Guid(0x9D3E658A, 0xAB414EE8, 0xB37D17B0, 0x13604BF2);
	constexpr int32 CommonRecovery = 1;
	constexpr int32 CommonMovement = 2;
	constexpr int32 CommonDamage = 3;
	const FCustomVersionRegistration Registration(Guid, CommonDamage, TEXT("EnemyPatternSetVersion"));
}

void UEnemyPatternSetData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(EnemyPatternSetVersion::Guid);
}

void UEnemyPatternSetData::PostLoad()
{
	Super::PostLoad();
	if (GetLinkerCustomVersion(EnemyPatternSetVersion::Guid) < EnemyPatternSetVersion::CommonRecovery)
	{
		// 저장 형식이 바뀐 에셋만 이전하여 사용자가 새 위치에 지정한 0초도 보존한다.
		for (FEnemyPatternDefinition& Pattern : Patterns)
		{
			if (Pattern.ExecutionSettings)
			{
				Pattern.ExecutionSettings->ConditionalPostLoad();
				Pattern.RecoverySeconds = Pattern.ExecutionSettings->RecoverySeconds;
			}
		}
	}
	if (GetLinkerCustomVersion(EnemyPatternSetVersion::Guid) < EnemyPatternSetVersion::CommonMovement)
	{
		// 기존 설정의 0 반경도 그대로 이전하고, 새 형식으로 저장된 값은 덮어쓰지 않는다.
		for (FEnemyPatternDefinition& Pattern : Patterns)
		{
			if (UMoneyGunPatternSettings* MoneyGun = Cast<UMoneyGunPatternSettings>(Pattern.ExecutionSettings))
			{
				MoneyGun->ConditionalPostLoad();
				Pattern.Movement.Policy = MoneyGun->bChaseTarget
					? EEnemyPatternMovementPolicy::ChaseTarget : EEnemyPatternMovementPolicy::Stop;
				Pattern.Movement.AcceptanceRadius = MoneyGun->ChaseAcceptanceRadius;
			}
			else if (Pattern.AbilityClass && Pattern.AbilityClass->IsChildOf(UGA_Enemy_RushAttack::StaticClass()))
			{
				Pattern.Movement.Policy = EEnemyPatternMovementPolicy::AbilityDriven;
			}
		}
	}

}

#if WITH_EDITOR
// 전용 객체의 판정 배열은 해당 설정 클래스에서 편집 처리
void UEnemyPatternSetData::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// GA별 설정의 편집 처리는 전용 객체에 위임하여 공통 에셋이 판정 방식을 알 필요가 없게 함
	for (FEnemyPatternDefinition& Pattern : Patterns)
		if (Pattern.ExecutionSettings)
		{
			Pattern.ExecutionSettings->PostEditChangeChainProperty(PropertyChangedEvent);
		}
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif
