/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 분광기 구현

#include "Gimmicks/Stage/Laser/GimmickLaserSplitter.h"
#include "Gimmicks/Laser/LaserBeamComponent.h"
#include "Components/StaticMeshComponent.h"

AGimmickLaserSplitter::AGimmickLaserSplitter()
{
	OutputLeft = CreateDefaultSubobject<ULaserBeamComponent>(TEXT("OutputLeft"));
	OutputLeft->SetupAttachment(Mesh);
	OutputLeft->SetRelativeLocation(FVector(0.f, 0.f, BeamHeight));

	OutputRight = CreateDefaultSubobject<ULaserBeamComponent>(TEXT("OutputRight"));
	OutputRight->SetupAttachment(Mesh);
	OutputRight->SetRelativeLocation(FVector(0.f, 0.f, BeamHeight));

	ApplySplitAngle();
}

void AGimmickLaserSplitter::OnConstruction(const FTransform& Transform)
{
	ApplySplitAngle();
	Super::OnConstruction(Transform);
}

void AGimmickLaserSplitter::ApplySplitAngle()
{
	if (OutputLeft)
	{
		OutputLeft->SetRelativeRotation(FRotator(0.f, -SplitAngle, 0.f));
	}
	if (OutputRight)
	{
		OutputRight->SetRelativeRotation(FRotator(0.f, SplitAngle, 0.f));
	}
}

void AGimmickLaserSplitter::GetOutputLasers(TArray<ULaserBeamComponent*>& OutLasers) const
{
	OutLasers.Add(OutputLeft);
	OutLasers.Add(OutputRight);
}
