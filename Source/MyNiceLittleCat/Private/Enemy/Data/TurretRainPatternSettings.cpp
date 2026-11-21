#include "Enemy/Data/TurretRainPatternSettings.h"
#include "Gimmicks/Turret/Turret.h"

bool UTurretRainPatternSettings::IsValidConfiguration() const
{
	return TurretClass && TurretCount >= 1 && Cycles >= 1
		&& Radius > 0.f && DropHeight > 0.f && ActiveSeconds > 0.f;
}

// 전체 시간 + 끝 처리 여유(0.5초)가 패턴 제한 시간보다 짧은지
bool UTurretRainPatternSettings::FitsExecutionTime(float MaxExecutionSeconds) const
{
	return GetExecutionSeconds() + DefaultExecutionTimeMargin < MaxExecutionSeconds;
}

float UTurretRainPatternSettings::GetExecutionSeconds() const
{
	if (bRunInBackground)
	{
		return (TurretCount - 1) * DropInterval;
	}
	const float OneCycle = (TurretCount - 1) * DropInterval + ActiveSeconds;
	return Cycles * OneCycle + (Cycles - 1) * CycleGapSeconds;
}
