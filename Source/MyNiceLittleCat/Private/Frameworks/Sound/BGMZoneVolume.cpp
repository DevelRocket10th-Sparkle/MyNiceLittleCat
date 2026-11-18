#include "Frameworks/Sound/BGMZoneVolume.h"
#include "Components/BoxComponent.h"
#include "Frameworks/Sound/BGMSubSystem.h"
ABGMZoneVolume::ABGMZoneVolume()
{
 	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	RootComponent = Box;
	Box->SetBoxExtent(FVector(1000.f, 1000.f, 500.f));

	// 오버랩 감지만 하고 아무것도 막지 않음
	// 쿼리(오버랩)만 사용, 물리 충돌 없음
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// 오브젝트 타입을 Vehicle로: 레이저의 ObjectType 트레이스(WorldStatic/Dynamic/Pawn/PhysicsBody)에 안 걸리게 하려는 의도
	// (프로젝트에서 Vehicle 채널은 쓰는 곳이 없음)
	Box->SetCollisionObjectType(ECC_Vehicle);

	// 모든 채널 무시 → 터렛 프로젝타일(커스텀 채널) 등 아무것도 막지 않음
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);

	// 플레이어 감지만 남김
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Box->SetGenerateOverlapEvents(true);
}

void ABGMZoneVolume::BeginPlay()
{
	Super::BeginPlay();

	Box->OnComponentBeginOverlap.AddDynamic(this, &ABGMZoneVolume::OnBoxBeginOverlap);
	Box->OnComponentEndOverlap.AddDynamic(this, &ABGMZoneVolume::OnBoxEndOverlap);

	// 플레이어가 이미 박스 안에서 스폰된 경우에 대비
	// --> BeginOverlap이 안 올 수 있어서 직접 확인
	TArray<AActor*> Overlapping;

	// 플레이어 스폰/빙의가 모두 끝난 뒤 확인하도록 한 틱 미룸
	GetWorldTimerManager().SetTimerForNextTick(this, &ABGMZoneVolume::CheckInitialPlayerInside);
}

void ABGMZoneVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (true == bPlayerInside)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UBGMSubSystem* BGM = GI->GetSubsystem<UBGMSubSystem>())
				BGM->ExitZone(this, FadeTime);
		}
		bPlayerInside = false;
	}

	Super::EndPlay(EndPlayReason);
}

void ABGMZoneVolume::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (true == bPlayerInside || false == IsPlayerPawn(OtherActor)) return;

	if (UBGMSubSystem* BGM = GetGameInstance()->GetSubsystem<UBGMSubSystem>())
	{
		bPlayerInside = true;
		BGM->EnterZone(this, ZoneSound, Priority, FadeTime);
	}
}

void ABGMZoneVolume::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (false == bPlayerInside || false == IsPlayerPawn(OtherActor)) return;

	if (UBGMSubSystem* BGM = GetGameInstance()->GetSubsystem<UBGMSubSystem>())
	{
		bPlayerInside = false;
		BGM->ExitZone(this, FadeTime);
	}
}

bool ABGMZoneVolume::IsPlayerPawn(const AActor* Actor) const
{
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn && Pawn->IsPlayerControlled();
}

void ABGMZoneVolume::CheckInitialPlayerInside()
{
	// 이미 오버랩 이벤트로 들어온 상태면 중복 처리 안 함
	if (true == bPlayerInside) return;

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (nullptr == Pawn) return;

	// 오버랩 대신 위치로 판정 -> 이벤트 타이밍과 무관하게 정확함
	if (true == IsLocationInsideBox(Pawn->GetActorLocation()))
		OnBoxBeginOverlap(Box, Pawn, nullptr, 0, false, FHitResult());
}

bool ABGMZoneVolume::IsLocationInsideBox(const FVector& WorldLocation) const
{
	// 월드 좌표 → 박스 로컬 좌표(회전·스케일 제거)로 바꾼 뒤 Extent와 비교
	const FVector Local = Box->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = Box->GetUnscaledBoxExtent();
	
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}
