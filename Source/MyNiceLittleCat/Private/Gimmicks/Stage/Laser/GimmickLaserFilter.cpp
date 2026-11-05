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

	// 통과형 렌즈는 들어온 방향 그대로 이어지므로 비스듬한 빛도 넉넉히 받음 (렌즈 면에 거의 나란한 옆 5°만 막음)
	InputHalfAngle = 85.f;
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

bool AGimmickLaserFilter::AcceptsIncomingDirection(const FVector& Incoming) const
{
	if (!bPassThrough || bAcceptAnySide)
	{
		return Super::AcceptsIncomingDirection(Incoming);
	}

	// 렌즈 면(액터 정면 축)과 나란히 들어온 빛이면 앞뒤 어느 쪽이든 받음, 옆(테두리)으로 온 빛은 막음
	const FVector LensAxis = GetActorForwardVector().GetSafeNormal2D();
	if (Incoming.IsNearlyZero() || LensAxis.IsNearlyZero())
	{
		return false;
	}

	return FMath::Abs(FVector::DotProduct(Incoming, LensAxis)) >= FMath::Cos(FMath::DegreesToRadians(InputHalfAngle));
}

FVector AGimmickLaserFilter::MakePassThroughStart(const FVector& InputLocation) const
{
	FVector Start = InputLocation;
	Start.Z = OutputLaser ? OutputLaser->GetComponentLocation().Z : GetActorLocation().Z + BeamHeight;
	return Start;
}

void AGimmickLaserFilter::PlaceOutputsForInput(const FVector& InputDirection2D, const FVector& InputLocation)
{
	if (!bPassThrough || !OutputLaser || InputDirection2D.IsNearlyZero())
	{
		return;
	}

	// 들어온 빛이 렌즈를 지나 그대로 이어지는 모양 (출력 첫 구간은 자기 자신을 무시하므로 장치 안에서 시작해도 됨)
	OutputLaser->SetWorldLocationAndRotation(MakePassThroughStart(InputLocation), InputDirection2D.Rotation());
}

bool AGimmickLaserFilter::GetPreviewOutputRay(const ULaserBeamComponent* Output, const FHitResult& Hit, FVector& OutStart, FVector& OutDirection) const
{
	if (!bPassThrough || !Output || Output != OutputLaser.Get())
	{
		return false;
	}

	OutDirection = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal2D();
	if (OutDirection.IsNearlyZero())
	{
		return false;
	}

	OutStart = MakePassThroughStart(Hit.Location);
	return true;
}
