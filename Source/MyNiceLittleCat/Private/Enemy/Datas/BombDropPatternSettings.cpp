#include "Enemy/Datas/BombDropPatternSettings.h"
#include "Enemy/Bomb/BombEnemy.h"

bool UBombDropPatternSettings::IsValidConfiguration() const
{
	return BombClass && BombCount >= 1 && DropHeight > 0.f;
}

float UBombDropPatternSettings::GetExecutionSeconds() const
{
	return (BombCount - 1) * DropInterval;
}

float UBombDropPatternSettings::GetFallSeconds(float GravityZ) const
{
	// h = v*t + 0.5*g*t^2  →  t = (-v + sqrt(v^2 + 2gh)) / g
	const float G = FMath::Max(FMath::Abs(GravityZ), 1.f);
	const float V = DropSpeed;
	return (-V + FMath::Sqrt(V * V + 2.f * G * DropHeight)) / G;
}
