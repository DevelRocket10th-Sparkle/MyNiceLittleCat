#include "Tutorial/TutorialSideWallHider.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"

ATutorialSideWallHider::ATutorialSideWallHider()
{
	PrimaryActorTick.bCanEverTick = true;

	RoomBox = CreateDefaultSubobject<UBoxComponent>(TEXT("RoomBox"));
	RootComponent = RoomBox;
	// 판정용 영역일 뿐이라 충돌 없음
	RoomBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATutorialSideWallHider::BeginPlay()
{
	Super::BeginPlay();
	
	// 숨겨도 그림자는 남기도록 함 (실내 음영 변경되지 않음)
	for (FSideWallSet& Set : Sides)
	{
		for (AActor* Wall : Set.WallActors)
		{
			if (!Wall) continue;

			TInlineComponentArray<UPrimitiveComponent*> Prims(Wall);
			for (UPrimitiveComponent* Prim : Prims)
			{
				Prim->SetCastHiddenShadow(true);
			}
		}
	}
}

void ATutorialSideWallHider::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CameraManager) return;

	// 카메라 위치를 박스 로컬 좌표로 변환: 박스가 회전돼 있어도 축 정렬처럼 비교 가능
	const FVector LocalCamera = RoomBox->GetComponentTransform().InverseTransformPosition(CameraManager->GetCameraLocation());
	const FVector Extent = RoomBox->GetUnscaledBoxExtent();

	// 플레이어가 이 방 박스 안에 있을 때만 숨김 판정
	// 옆방 / 복도에 있을 때 이 방 벽(특히 문 있는 -X 벽)이 미리 사라지는 것 방지
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector LocalPawn = PlayerPawn ? RoomBox->GetComponentTransform().InverseTransformPosition(PlayerPawn->GetActorLocation()) : FVector(FLT_MAX);
	const bool bPlayerInside = FMath::Abs(LocalPawn.X) <= Extent.X && FMath::Abs(LocalPawn.Y) <= Extent.Y;

	for (int32 Side = 0; Side < 4; ++Side)
	{
		const int32 Axis = Side / 2;   // 0 = X, 1 = Y
		const bool bMinSide = (Side % 2) == 0;
		// 카메라가 이 면 평면 바깥으로 나간 거리: 양수면 벽 바깥면을 보고 있음
		const double Outside = bMinSide ? -Extent[Axis] - LocalCamera[Axis] : LocalCamera[Axis] - Extent[Axis];
		// 이미 숨긴 면은 Margin 없이 평면 안쪽으로 들어와야 복구: 경계 깜빡임 방지 (히스테리시스)
		// 플레이어가 방 밖이면 이 방의 모든 면을 다시 보이게 함
		const bool bHide = bPlayerInside && (bSideHidden[Side] ? Outside > 0.0 : Outside > Margin);
		ApplySide(Side, bHide);
	}
}

void ATutorialSideWallHider::ApplySide(int32 Side, bool bHide)
{
	if (bHide == bSideHidden[Side]) return;
	bSideHidden[Side] = bHide;

	for (AActor* Wall : Sides[Side].WallActors)
	{
		if (Wall)
			Wall->SetActorHiddenInGame(bHide);
	}
}

