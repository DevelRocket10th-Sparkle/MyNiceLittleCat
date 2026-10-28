/// 작성자: 최요환
/// 작성일자: 2026-10-28
///
/// 경기장 배치용 자성 코인 낙하 패턴 액터 구현

#include "Gimmicks/Boss/MagneticCoinPatternActor.h"
#include "Gimmicks/Boss/MagneticCoinPatternComponent.h"
#include "Components/BoxComponent.h"

AMagneticCoinPatternActor::AMagneticCoinPatternActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SpawnArea = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnArea"));
	SpawnArea->SetupAttachment(Root);
	SpawnArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnArea->SetCanEverAffectNavigation(false);
	SpawnArea->SetHiddenInGame(true);
	SpawnArea->ShapeColor = FColor::Cyan;

	Pattern = CreateDefaultSubobject<UMagneticCoinPatternComponent>(TEXT("Pattern"));
}

void AMagneticCoinPatternActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 패턴 영역을 박스로 표시 (높이는 보기 좋게 고정)
	const FVector Extent = Pattern->GetSpawnAreaExtent();
	SpawnArea->SetBoxExtent(FVector(Extent.X, Extent.Y, 50.f));
}

void AMagneticCoinPatternActor::TogglePattern()
{
	if (Pattern->IsPatternActive())
	{
		Pattern->StopPattern(false);
	}
	else
	{
		Pattern->StartPattern();
	}
}

void AMagneticCoinPatternActor::SpawnWave()
{
	Pattern->SpawnWave();
}

void AMagneticCoinPatternActor::ClearCoins()
{
	Pattern->ClearCoins();
}
