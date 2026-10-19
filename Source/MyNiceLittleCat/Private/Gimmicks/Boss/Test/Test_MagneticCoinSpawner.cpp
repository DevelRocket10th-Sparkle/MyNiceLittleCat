/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 자성 코인 낙하 패턴 테스트용 스포너 구현

#include "Gimmicks/Boss/Test/Test_MagneticCoinSpawner.h"
#include "Gimmicks/Boss/MagneticCoinPatternComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

ATest_MagneticCoinSpawner::ATest_MagneticCoinSpawner()
{
	PrimaryActorTick.bCanEverTick = true;

	SpawnArea = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnArea"));
	SetRootComponent(SpawnArea);
	SpawnArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Pattern = CreateDefaultSubobject<UMagneticCoinPatternComponent>(TEXT("Pattern"));
}

void ATest_MagneticCoinSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 패턴 영역을 박스로 표시 (높이는 보기 좋게 고정)
	const FVector Extent = Pattern->GetSpawnAreaExtent();
	SpawnArea->SetBoxExtent(FVector(Extent.X, Extent.Y, 50.f));
}

void ATest_MagneticCoinSpawner::BeginPlay()
{
	Super::BeginPlay();

	Pattern->OnCoinFallHit.AddDynamic(this, &ATest_MagneticCoinSpawner::HandleCoinFallHit);

	if (bBindTestKeys)
	{
		if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
		{
			EnableInput(PlayerController);
			if (InputComponent)
			{
				InputComponent->BindKey(TogglePatternKey, IE_Pressed, this, &ATest_MagneticCoinSpawner::TogglePattern);
				InputComponent->BindKey(SpawnWaveKey, IE_Pressed, this, &ATest_MagneticCoinSpawner::SpawnWave);
			}
		}
	}

	if (bStartPatternOnBeginPlay)
	{
		Pattern->StartPattern();
	}
}

void ATest_MagneticCoinSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GEngine)
	{
		const FString Message = FString::Printf(TEXT("[코인 패턴 테스트] %s | 코인 %d (바닥 %d) / 최대 %d | 낙하 피격 %d회  (B 시작/정지, V 즉시 스폰)"),
			Pattern->IsPatternActive() ? TEXT("ON") : TEXT("OFF"),
			Pattern->GetActiveCoinCount(true),
			Pattern->GetActiveCoinCount(false),
			Pattern->GetMaxActiveCoins(),
			FallHitCount);
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.f, FColor::Cyan, Message);
	}
}

void ATest_MagneticCoinSpawner::TogglePattern()
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

void ATest_MagneticCoinSpawner::SpawnWave()
{
	Pattern->SpawnWave();
}

void ATest_MagneticCoinSpawner::ClearCoins()
{
	Pattern->ClearCoins();
}

void ATest_MagneticCoinSpawner::HandleCoinFallHit(UMagneticCoinPatternComponent* InPattern, AMagneticCoin* Coin, AActor* HitActor)
{
	++FallHitCount;

	UE_LOG(LogTemp, Display, TEXT("[MagneticCoin] 낙하 피격: %s"), *GetNameSafe(HitActor));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, FString::Printf(TEXT("낙하 코인 피격: %s"), *GetNameSafe(HitActor)));
	}
}
