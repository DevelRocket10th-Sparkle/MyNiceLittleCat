/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 분광기 구현

#include "Gimmicks/Stage/Laser/GimmickLaserSplitter.h"
#include "Gimmicks/Laser/LaserBeamComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/CollisionProfile.h"

AGimmickLaserSplitter::AGimmickLaserSplitter()
{
	OutputLeft = CreateDefaultSubobject<ULaserBeamComponent>(TEXT("OutputLeft"));
	OutputLeft->SetupAttachment(Mesh);
	OutputLeft->SetRelativeLocation(FVector(0.f, 0.f, BeamHeight));

	OutputRight = CreateDefaultSubobject<ULaserBeamComponent>(TEXT("OutputRight"));
	OutputRight->SetupAttachment(Mesh);
	OutputRight->SetRelativeLocation(FVector(0.f, 0.f, BeamHeight));

	// 표시 메시: 콜리전 없음(빛·플레이어 안 막음), 루트(판정 큐브) 스케일과 상관없이 메시 크기 그대로
	auto CreateMarkerMesh = [this](FName Name, USceneComponent* Parent)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Part->SetCanEverAffectNavigation(false);
		Part->SetUsingAbsoluteScale(true);
		return Part;
	};

	LeftNozzle = CreateMarkerMesh(TEXT("LeftNozzle"), OutputLeft);
	LeftNozzleTip = CreateMarkerMesh(TEXT("LeftNozzleTip"), OutputLeft);
	RightNozzle = CreateMarkerMesh(TEXT("RightNozzle"), OutputRight);
	RightNozzleTip = CreateMarkerMesh(TEXT("RightNozzleTip"), OutputRight);
	InputMarker = CreateMarkerMesh(TEXT("InputMarker"), InputDirection);
	InputMarkerFront = CreateMarkerMesh(TEXT("InputMarkerFront"), InputDirection);

	ApplySplitAngle();
}

void AGimmickLaserSplitter::OnConstruction(const FTransform& Transform)
{
	ApplySplitAngle();
	// 부모가 출력 레이저 높이를 맞춘 뒤 표시 위치 계산
	Super::OnConstruction(Transform);
	UpdateMarkers();
}

void AGimmickLaserSplitter::BeginPlay()
{
	Super::BeginPlay();
	UpdateMarkers();
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

void AGimmickLaserSplitter::PlaceAlongAnchor(USceneComponent* Part, const USceneComponent* Anchor, float ZBase, float Distance, float HeightOffset, const FRotator& Rotation)
{
	if (!Part || !Anchor)
	{
		return;
	}

	// 빛 방향은 수평 기준 (바닥과 나란히)
	const FRotator Facing(0.f, Anchor->GetComponentRotation().Yaw, 0.f);
	FVector Location = Anchor->GetComponentLocation() + Facing.Vector() * Distance;
	Location.Z = ZBase + HeightOffset;
	Part->SetWorldLocationAndRotation(Location, Facing.Quaternion() * Rotation.Quaternion());
}

void AGimmickLaserSplitter::UpdateMarkers()
{
	if (!OutputLeft || !OutputRight)
	{
		return;
	}

	// 모든 표시는 출력 레이저 높이(장치 가운데) 기준
	const float BeamZ = OutputLeft->GetComponentLocation().Z;
	PlaceAlongAnchor(LeftNozzle, OutputLeft, BeamZ, NozzleDistance, NozzleHeightOffset, NozzleRotation);
	PlaceAlongAnchor(LeftNozzleTip, OutputLeft, BeamZ, NozzleTipDistance, NozzleHeightOffset, NozzleTipRotation);
	PlaceAlongAnchor(RightNozzle, OutputRight, BeamZ, NozzleDistance, NozzleHeightOffset, NozzleRotation);
	PlaceAlongAnchor(RightNozzleTip, OutputRight, BeamZ, NozzleTipDistance, NozzleHeightOffset, NozzleTipRotation);

	// 받는 쪽 표시: 화살표 위치(액터 원점 높이)가 아니라 장치 가운데 기준
	if (InputMarker && InputDirection)
	{
		const FRotator Facing(0.f, InputDirection->GetComponentRotation().Yaw, 0.f);
		FVector Location = OutputLeft->GetComponentLocation() + Facing.Vector() * InputMarkerDistance;
		Location.Z = BeamZ + InputMarkerHeightOffset;
		InputMarker->SetWorldLocationAndRotation(Location, Facing.Quaternion() * InputMarkerRotation.Quaternion());

		// 정면 표시: 반대 방향 대칭 위치. 메시·머티리얼·크기는 받는 쪽 표시를 따라감 (따로 지정한 메시가 있으면 그것 사용)
		if (InputMarkerFront)
		{
			const FRotator FrontFacing(0.f, Facing.Yaw + 180.f, 0.f);
			FVector FrontLocation = OutputLeft->GetComponentLocation() + FrontFacing.Vector() * InputMarkerDistance;
			FrontLocation.Z = BeamZ + InputMarkerHeightOffset;
			InputMarkerFront->SetWorldLocationAndRotation(FrontLocation, FrontFacing.Quaternion() * InputMarkerRotation.Quaternion());

			if (!InputMarkerFront->GetStaticMesh() && InputMarker->GetStaticMesh())
			{
				InputMarkerFront->SetStaticMesh(InputMarker->GetStaticMesh());
				for (int32 Slot = 0; Slot < InputMarker->GetNumMaterials(); ++Slot)
				{
					InputMarkerFront->SetMaterial(Slot, InputMarker->GetMaterial(Slot));
				}
				InputMarkerFront->SetWorldScale3D(InputMarker->GetComponentScale());
			}
			InputMarkerFront->SetVisibility(bAcceptBothEnds);
		}
	}
}

bool AGimmickLaserSplitter::AcceptsIncomingDirection(const FVector& Incoming) const
{
	if (!bAcceptBothEnds || bAcceptAnySide || !InputDirection)
	{
		return Super::AcceptsIncomingDirection(Incoming);
	}

	// 받는 면 축과 나란히 들어온 빛이면 뒤·앞 어느 쪽이든 받음 (옆, 즉 출력 방향에서 온 빛은 막음)
	const FVector Axis = InputDirection->GetForwardVector().GetSafeNormal2D();
	if (Incoming.IsNearlyZero() || Axis.IsNearlyZero())
	{
		return false;
	}

	return FMath::Abs(FVector::DotProduct(Incoming, Axis)) >= FMath::Cos(FMath::DegreesToRadians(InputHalfAngle));
}

void AGimmickLaserSplitter::GetOutputLasers(TArray<ULaserBeamComponent*>& OutLasers) const
{
	OutLasers.Add(OutputLeft);
	OutLasers.Add(OutputRight);
}
