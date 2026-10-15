// 작성자: 최요환
// 작성일자: 2026-10-15
//
// 보스전 배리어 구현

#include "Gimmicks/Boss/Barrier.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Gimmicks/Boss/BarrierGenerator.h"

ABarrier::ABarrier()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// 피해 무효는 상태 플래그로 처리하므로 메시는 시각용
	Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

void ABarrier::BeginPlay()
{
	Super::BeginPlay();

	for (ABarrierGenerator* Generator : Generators)
	{
		if (IsValid(Generator))
		{
			Generator->OnGeneratorDisabled.AddUniqueDynamic(this, &ABarrier::HandleGeneratorDisabled);
		}
	}

	ApplyVisualState();

	if (bActivateOnBeginPlay)
	{
		ActivateBarrier();
	}
}

void ABarrier::Activate_Implementation(AActor* Activator)
{
	ActivateBarrier();
}

void ABarrier::Deactivate_Implementation(AActor* Activator)
{
	DeactivateBarrier();
}

void ABarrier::ActivateBarrier()
{
	if (bBarrierActive)
	{
		return;
	}

	if (Generators.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 생성장치가 지정되지 않아 배리어가 붕괴하지 않습니다."), *GetName());
	}

	for (ABarrierGenerator* Generator : Generators)
	{
		if (IsValid(Generator))
		{
			Generator->ResetGenerator();
		}
	}

	bBarrierActive = true;
	ApplyVisualState();
	K2_OnBarrierStateChanged(true, false);
	OnBarrierActivated.Broadcast(this);
}

void ABarrier::DeactivateBarrier()
{
	if (!bBarrierActive)
	{
		return;
	}

	bBarrierActive = false;
	ApplyVisualState();
	K2_OnBarrierStateChanged(false, false);
}

void ABarrier::BreakBarrier()
{
	if (!bBarrierActive)
	{
		return;
	}

	bBarrierActive = false;
	ApplyVisualState();
	K2_OnBarrierStateChanged(false, true);
	OnBarrierBroken.Broadcast(this);
}

void ABarrier::HandleGeneratorDisabled(ABarrierGenerator* Generator)
{
	if (bBarrierActive && AreAllGeneratorsDisabled())
	{
		BreakBarrier();
	}
}

bool ABarrier::AreAllGeneratorsDisabled() const
{
	bool bHasValidGenerator = false;

	for (const ABarrierGenerator* Generator : Generators)
	{
		if (!IsValid(Generator))
		{
			continue;
		}

		bHasValidGenerator = true;

		if (!Generator->IsDisabled())
		{
			return false;
		}
	}

	return bHasValidGenerator;
}

void ABarrier::ApplyVisualState()
{
	if (Mesh)
	{
		Mesh->SetVisibility(bBarrierActive, true);
	}
}
