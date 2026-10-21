/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 자성 코인 낙하 패턴 컴포넌트 구현

#include "Gimmicks/Boss/MagneticCoinPatternComponent.h"
#include "Gimmicks/Boss/MagneticCoin.h"
#include "Engine/World.h"

UMagneticCoinPatternComponent::UMagneticCoinPatternComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	CoinClass = AMagneticCoin::StaticClass();
}

void UMagneticCoinPatternComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoStart)
	{
		StartPattern();
	}
}

void UMagneticCoinPatternComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bPatternActive = false;

	Super::EndPlay(EndPlayReason);
}

void UMagneticCoinPatternComponent::StartPattern()
{
	if (bPatternActive)
	{
		return;
	}

	bPatternActive = true;
	IntervalTimer = 0.f;
	DepletedTimer = 0.f;
	SetComponentTickEnabled(true);

	if (bSpawnOnStart)
	{
		SpawnWave();
	}
}

void UMagneticCoinPatternComponent::StopPattern(bool bClearCoins)
{
	bPatternActive = false;
	SetComponentTickEnabled(false);

	if (bClearCoins)
	{
		ClearCoins();
	}
}

void UMagneticCoinPatternComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bPatternActive)
	{
		return;
	}

	RemoveInvalidCoins();

	if (SpawnMode == EMagneticCoinSpawnMode::Interval)
	{
		IntervalTimer += DeltaTime;
		if (IntervalTimer >= SpawnInterval)
		{
			IntervalTimer = 0.f;
			SpawnWave();
		}
		return;
	}

	// WhenDepleted: 낙하 중인 코인도 곧 바닥에 남으므로 함께 셈 (낙하 중 재생성 방지)
	if (GetActiveCoinCount(true) > RespawnThreshold)
	{
		DepletedTimer = 0.f;
		return;
	}

	DepletedTimer += DeltaTime;
	if (DepletedTimer >= RespawnDelay)
	{
		DepletedTimer = 0.f;
		SpawnWave();
	}
}

void UMagneticCoinPatternComponent::SpawnWave()
{
	RemoveInvalidCoins();

	for (int32 Index = 0; Index < CoinsPerWave; ++Index)
	{
		if (SpawnedCoins.Num() >= MaxActiveCoins)
		{
			return;
		}

		SpawnCoin();
	}
}

AMagneticCoin* UMagneticCoinPatternComponent::SpawnCoin()
{
	UWorld* World = GetWorld();
	if (!World || !CoinClass)
	{
		return nullptr;
	}

	const FVector Center = GetSpawnCenter();
	const FVector SpawnLocation(
		Center.X + FMath::FRandRange(-SpawnAreaExtent.X, SpawnAreaExtent.X),
		Center.Y + FMath::FRandRange(-SpawnAreaExtent.Y, SpawnAreaExtent.Y),
		Center.Z + SpawnHeight);

	const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation, CoinScale);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AMagneticCoin* Coin = World->SpawnActor<AMagneticCoin>(CoinClass, SpawnTransform, SpawnParams);
	if (!Coin)
	{
		return nullptr;
	}

	Coin->OnCoinFallHit.AddDynamic(this, &UMagneticCoinPatternComponent::HandleCoinFallHit);
	Coin->StartFall(PickPolarity());
	SpawnedCoins.Add(Coin);

	OnCoinSpawned.Broadcast(this, Coin);
	return Coin;
}

EMagneticPolarity UMagneticCoinPatternComponent::PickPolarity()
{
	if (bRandomPolarity)
	{
		LastPolarity = FMath::RandBool() ? EMagneticPolarity::N : EMagneticPolarity::S;
	}
	else
	{
		LastPolarity = LastPolarity == EMagneticPolarity::N ? EMagneticPolarity::S : EMagneticPolarity::N;
	}

	return LastPolarity;
}

void UMagneticCoinPatternComponent::ClearCoins()
{
	for (const TWeakObjectPtr<AMagneticCoin>& Coin : SpawnedCoins)
	{
		if (Coin.IsValid())
		{
			Coin->Destroy();
		}
	}

	SpawnedCoins.Reset();
}

int32 UMagneticCoinPatternComponent::GetActiveCoinCount(bool bIncludeFalling) const
{
	int32 Count = 0;

	for (const TWeakObjectPtr<AMagneticCoin>& Coin : SpawnedCoins)
	{
		if (Coin.IsValid() && (bIncludeFalling || !Coin->IsAirborne()))
		{
			++Count;
		}
	}

	return Count;
}

FVector UMagneticCoinPatternComponent::GetSpawnCenter() const
{
	if (IsValid(SpawnCenterActor))
	{
		return SpawnCenterActor->GetActorLocation();
	}

	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
}

void UMagneticCoinPatternComponent::RemoveInvalidCoins()
{
	SpawnedCoins.RemoveAll([](const TWeakObjectPtr<AMagneticCoin>& Coin) { return !Coin.IsValid(); });
}

void UMagneticCoinPatternComponent::HandleCoinFallHit(AMagneticCoin* Coin, AActor* HitActor)
{
	OnCoinFallHit.Broadcast(this, Coin, HitActor);
}
