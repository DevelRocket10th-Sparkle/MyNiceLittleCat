// 작성자: 최요환
// 작성일자: 2026-10-15
//
// 보스전 배리어 생성장치 구현

#include "Gimmicks/Boss/BarrierGenerator.h"
#include "Engine/DamageEvents.h"

ABarrierGenerator::ABarrierGenerator()
{
	// 플레이어가 직접 상호작용하지 않고, 시작부터 가동 상태
	bInteractionEnabled = false;
	bStartActivated = true;
}

void ABarrierGenerator::BeginPlay()
{
	CurrentDurability = MaxDurability;

	Super::BeginPlay();
}

float ABarrierGenerator::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDisabled || DamageAmount <= 0.0f || !CanBeDamagedBy(DamageCauser))
	{
		return 0.0f;
	}

	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	CurrentDurability = FMath::Max(CurrentDurability - AppliedDamage, 0.0f);
	OnDurabilityChanged.Broadcast(this, CurrentDurability, MaxDurability);

	if (CurrentDurability <= 0.0f)
	{
		DisableGenerator(DamageCauser);
	}

	return AppliedDamage;
}

bool ABarrierGenerator::CanBeDamagedBy_Implementation(AActor* DamageCauser) const
{
	return !bDisabled;
}

void ABarrierGenerator::DisableGenerator(AActor* DisableInstigator)
{
	if (bDisabled)
	{
		return;
	}

	bDisabled = true;
	SetActivated(false, DisableInstigator);
	OnGeneratorDisabled.Broadcast(this);
}

void ABarrierGenerator::ResetGenerator()
{
	bDisabled = false;
	CurrentDurability = MaxDurability;
	OnDurabilityChanged.Broadcast(this, CurrentDurability, MaxDurability);
	SetActivated(true, this);
}
