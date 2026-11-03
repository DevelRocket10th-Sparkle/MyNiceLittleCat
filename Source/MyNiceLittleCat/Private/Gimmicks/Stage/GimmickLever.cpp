/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 레버 구현

#include "Gimmicks/Stage/GimmickLever.h"
#include "Gimmicks/GimmickMoverComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"

AGimmickLever::AGimmickLever()
{
	// 레버는 당긴 상태를 기본으로 저장 (SaveId는 배치할 때 지정 권장)
	bSaveState = true;

	HandleMover = CreateDefaultSubobject<UGimmickMoverComponent>(TEXT("HandleMover"));
	HandleMover->SetupAttachment(Mesh);
	// 앞(+X)으로 45도 기운 상태 → 당기면 뒤로 45도
	HandleMover->SetRelativeRotation(FRotator(-45.f, 0.f, 0.f));
	HandleMover->OpenRotation = FRotator(90.f, 0.f, 0.f);
	HandleMover->OpenDuration = 0.3f;

	HandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandleMesh"));
	HandleMesh->SetupAttachment(HandleMover);
	HandleMesh->SetMobility(EComponentMobility::Movable);
	HandleMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}
