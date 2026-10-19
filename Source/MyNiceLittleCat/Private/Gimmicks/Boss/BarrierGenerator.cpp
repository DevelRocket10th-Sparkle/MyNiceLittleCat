/// 작성자: 최요환
/// 작성일자: 2026-10-15
///
/// 보스전 배리어 생성장치 구현

#include "Gimmicks/Boss/BarrierGenerator.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"

ABarrierGenerator::ABarrierGenerator()
{
	// 플레이어가 직접 상호작용하지 않고, 배리어가 켜기 전까지 비활성
	bInteractionEnabled = false;
	bStartActivated = false;
}

void ABarrierGenerator::BeginPlay()
{
	CurrentDurability = MaxDurability;

	Super::BeginPlay();

	SetGeneratorPresence(IsActivated());
}

float ABarrierGenerator::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (DamageAmount <= 0.0f || !CanBeDamagedBy(DamageCauser, DamageEvent.DamageTypeClass))
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

bool ABarrierGenerator::CanBeDamagedBy_Implementation(AActor* DamageCauser, TSubclassOf<UDamageType> DamageTypeClass) const
{
	return IsActivated() && !bDisabled && IsAllowedDamageType(DamageTypeClass);
}

bool ABarrierGenerator::IsAllowedDamageType(TSubclassOf<UDamageType> DamageTypeClass) const
{
	if (AllowedDamageTypes.IsEmpty())
	{
		return true;
	}

	if (!DamageTypeClass)
	{
		return false;
	}

	for (const TSubclassOf<UDamageType>& Allowed : AllowedDamageTypes)
	{
		if (Allowed && DamageTypeClass->IsChildOf(Allowed))
		{
			return true;
		}
	}

	return false;
}

void ABarrierGenerator::DisableGenerator(AActor* DisableInstigator)
{
	if (bDisabled)
	{
		return;
	}

	// 부서진 연출은 BP의 On Activation Changed에서 처리
	bDisabled = true;
	SetActivated(false, DisableInstigator);

	// 즉각 피드백용으로 바로 숨김 (액터는 남겨 두고 다음 페이즈에 ActivateGenerator로 다시 가동)
	if (bHideOnDisabled)
	{
		SetGeneratorPresence(false);
	}

	OnGeneratorDisabled.Broadcast(this);
}

void ABarrierGenerator::ActivateGenerator()
{
	bDisabled = false;
	CurrentDurability = MaxDurability;
	OnDurabilityChanged.Broadcast(this, CurrentDurability, MaxDurability);

	SetGeneratorPresence(true);
	SetActivated(true, this);
}

void ABarrierGenerator::DeactivateGenerator()
{
	SetActivated(false, this);
	SetGeneratorPresence(false);
}

void ABarrierGenerator::SetGeneratorPresence(bool bPresent)
{
	SetActorHiddenInGame(!bPresent);
	SetActorEnableCollision(bPresent);
}
