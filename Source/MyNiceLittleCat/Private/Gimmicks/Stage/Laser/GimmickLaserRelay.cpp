/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 빛 중계 장치 구현

#include "Gimmicks/Stage/Laser/GimmickLaserRelay.h"
#include "Gimmicks/Laser/LaserBeamComponent.h"
#include "Components/StaticMeshComponent.h"

AGimmickLaserRelay::AGimmickLaserRelay()
{
	bAcceptAnySide = true;

	OutputLaser = CreateDefaultSubobject<ULaserBeamComponent>(TEXT("OutputLaser"));
	OutputLaser->SetupAttachment(Mesh);
	OutputLaser->SetRelativeLocation(FVector(0.f, 0.f, BeamHeight));
}

void AGimmickLaserRelay::GetOutputLasers(TArray<ULaserBeamComponent*>& OutLasers) const
{
	OutLasers.Add(OutputLaser);
}
