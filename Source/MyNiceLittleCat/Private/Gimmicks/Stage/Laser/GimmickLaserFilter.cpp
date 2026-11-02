/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 색 필터 구현

#include "Gimmicks/Stage/Laser/GimmickLaserFilter.h"
#include "Gimmicks/Laser/LaserBeamComponent.h"
#include "Components/StaticMeshComponent.h"

AGimmickLaserFilter::AGimmickLaserFilter()
{
	OutputLaser = CreateDefaultSubobject<ULaserBeamComponent>(TEXT("OutputLaser"));
	OutputLaser->SetupAttachment(Mesh);
	OutputLaser->SetRelativeLocation(FVector(0.f, 0.f, BeamHeight));
}

void AGimmickLaserFilter::GetOutputLasers(TArray<ULaserBeamComponent*>& OutLasers) const
{
	OutLasers.Add(OutputLaser);
}

bool AGimmickLaserFilter::ComputeOutputColor(ELaserBeamColor InputColor, ELaserBeamColor& OutColor) const
{
	// 흰빛은 필터 색으로, 같은 색은 통과, 다른 색은 막음
	if (FilterColor == ELaserBeamColor::White)
	{
		OutColor = InputColor;
		return true;
	}

	OutColor = FilterColor;
	return InputColor == ELaserBeamColor::White || InputColor == FilterColor;
}
