#include "Enemy/Datas/MoneyGunPatternSettings.h"
#include "Enemy/Boss/Objects/MoneyGun.h"
#include "Enemy/Boss/Objects/MoneyProjectile.h"

UMoneyGunPatternSettings::UMoneyGunPatternSettings()
{
	// 이전 형식의 에셋에서 생략된 기본값을 복원할 때만 사용한다.
	RecoverySeconds = 0.4f;
}

bool UMoneyGunPatternSettings::IsValidConfiguration() const
{
	return GunClass && ProjectileClass && !LeftAnchorName.IsNone() && !RightAnchorName.IsNone()
		&& LeftAnchorName != RightAnchorName && ProjectileCount >= 1 && ProjectileCount <= 256
		&& FMath::IsFinite(AppearSeconds) && AppearSeconds >= 0.f
		&& FMath::IsFinite(LoadSeconds) && LoadSeconds >= 0.05f
		&& FMath::IsFinite(LoadRadius) && LoadRadius > 0.f
		&& FMath::IsFinite(PreFireSeconds) && PreFireSeconds >= 0.f
		&& FMath::IsFinite(ShotInterval) && ShotInterval >= 0.02f
		&& FMath::IsFinite(ProjectileSpeed) && ProjectileSpeed > 0.f
		&& FMath::IsFinite(ProjectileLifeSeconds) && ProjectileLifeSeconds > 0.f
		&& FMath::IsFinite(DamageMultiplier) && DamageMultiplier >= 0.f
		&& FMath::IsFinite(AimDegreesPerSecond) && AimDegreesPerSecond > 0.f
		&& !TargetOffset.ContainsNaN();
}

float UMoneyGunPatternSettings::GetExecutionSeconds() const
{
	return AppearSeconds + LoadSeconds + PreFireSeconds
		+ (ProjectileCount - 1) * ShotInterval + AppearSeconds;
}
