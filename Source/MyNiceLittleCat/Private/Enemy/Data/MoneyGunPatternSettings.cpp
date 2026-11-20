#include "Enemy/Data/MoneyGunPatternSettings.h"

#include "Enemy/Boss/Objects/MoneyGun.h"
#include "Enemy/Boss/Objects/MoneyProjectile.h"

namespace
{
	// 총의 전체 연출 시간이 패턴 제한 시간보다 이만큼 짧아야 정상 종료가 시간 초과보다 먼저 옴
	constexpr float ExecutionTimeMargin = 0.1f;
}

bool UMoneyGunPatternSettings::IsValidConfiguration() const
{
	// 사용 클래스와 앵커
	const bool bValidSetup = GunClass && ProjectileClass
		&& !LeftAnchorTag.IsNone() && !RightAnchorTag.IsNone() && LeftAnchorTag != RightAnchorTag;

	// 장전
	const bool bValidLoading = ProjectileCount >= 1 && ProjectileCount <= 256
		&& FMath::IsFinite(AppearSeconds) && AppearSeconds >= 0.f
		&& FMath::IsFinite(LoadSeconds) && LoadSeconds >= 0.05f
		&& FMath::IsFinite(LoadRadius) && LoadRadius > 0.f
		&& FMath::IsFinite(PreFireSeconds) && PreFireSeconds >= 0.f;

	// 발사와 조준
	const bool bValidFiring = FMath::IsFinite(ShotInterval) && ShotInterval >= 0.02f
		&& FMath::IsFinite(ProjectileSpeed) && ProjectileSpeed > 0.f
		&& FMath::IsFinite(ProjectileLifeSeconds) && ProjectileLifeSeconds > 0.f
		&& FMath::IsFinite(AimDegreesPerSecond) && AimDegreesPerSecond > 0.f
		&& !TargetOffset.ContainsNaN();

	return bValidSetup && bValidLoading && bValidFiring;
}

bool UMoneyGunPatternSettings::FitsExecutionTime(float MaxExecutionSeconds) const
{
	return GetExecutionSeconds() + ExecutionTimeMargin < MaxExecutionSeconds;
}

float UMoneyGunPatternSettings::GetExecutionSeconds() const
{
	// 등장 + 장전 + 발사 준비 + (탄 수 - 1) * 연사 간격 + 퇴장(등장과 같은 시간)
	return AppearSeconds + LoadSeconds + PreFireSeconds + (ProjectileCount - 1) * ShotInterval + AppearSeconds;
}
