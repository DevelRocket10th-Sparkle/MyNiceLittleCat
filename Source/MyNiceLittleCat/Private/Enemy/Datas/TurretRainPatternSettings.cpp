#include "Enemy/Datas/TurretRainPatternSettings.h"
#include "Gimmicks/Turret/Turret.h"

bool UTurretRainPatternSettings::IsValidConfiguration() const
{
	return TurretClass && TurretCount >= 1 && Cycles >= 1
		&& Radius > 0.f && DropHeight > 0.f && ActiveSeconds > 0.f;
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
