/// 작성자: 최요환
/// 작성일자: 2026-10-16
///
/// 상호작용 감지 영역 컴포넌트 구현

#include "Gimmicks/InteractionVolumeComponent.h"

UInteractionVolumeComponent::UInteractionVolumeComponent()
{
	ShapeColor = FColor(80, 255, 120);
	bHiddenInGame = true;
	InitBoxExtent(FVector(50.f));

	// 오브젝트 타입 쿼리(상호작용 감지)에만 걸리고 모든 채널은 무시 → 이동·물리·트레이스를 막지 않음
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
}

bool UInteractionVolumeComponent::IsTouchingCylinder(const FVector& Center, float Radius, float HalfHeight) const
{
	// 박스 로컬 공간(스케일 제외)에서 원기둥 크기만큼 넓힌 박스 안에 중심이 있는지로 근사
	const FVector Local = GetComponentTransform().InverseTransformPositionNoScale(Center);
	const FVector Extent = GetScaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X + Radius
		&& FMath::Abs(Local.Y) <= Extent.Y + Radius
		&& FMath::Abs(Local.Z) <= Extent.Z + HalfHeight;
}
