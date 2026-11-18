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
	Box->SetCollisionProfileName(TEXT("Trigger"));
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
	Box->GetOverlappingActors(Overlapping, APawn::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		if (IsPlayerPawn(Actor))
		{
			OnBoxBeginOverlap(Box, Actor, nullptr, 0, false, FHitResult());
			break;
		}
	}
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
