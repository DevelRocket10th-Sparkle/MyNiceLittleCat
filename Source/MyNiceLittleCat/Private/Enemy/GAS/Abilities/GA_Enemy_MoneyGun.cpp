#include "Enemy/GAS/Abilities/GA_Enemy_MoneyGun.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "Enemy/Boss/Objects/MoneyGun.h"
#include "Enemy/Combat/EnemyTargetUtils.h"
#include "Enemy/Data/MoneyGunPatternSettings.h"
#include "Enemy/EnemyCharacter.h"

namespace
{
	// 총의 전체 연출 시간이 패턴 제한 시간보다 이만큼 짧아야 정상 종료가 시간 초과보다 먼저 옴
	constexpr float ExecutionTimeMargin = 0.1f;
}

bool UGA_Enemy_MoneyGun::CanStartPattern() const
{
	const UMoneyGunPatternSettings* MoneyGunSettings = GetPatternSettings<UMoneyGunPatternSettings>();

	const bool bValid = MoneyGunSettings && MoneyGunSettings->IsValidConfiguration()
		&& EnemyTargetUtils::IsAttackableTarget(ExecutionTarget)
		&& MoneyGunSettings->GetExecutionSeconds() + ExecutionTimeMargin < ExecutionPattern.MaxExecutionSeconds
		&& FindAnchor(MoneyGunSettings->LeftAnchorTag) && FindAnchor(MoneyGunSettings->RightAnchorTag);

	if (!bValid)
	{
		UE_LOG(LogTemp, Error, TEXT("[MoneyGun] %s: invalid settings, missing anchor tags, or MaxExecutionSeconds too short."),
			*GetNameSafe(Enemy.Get()));
	}

	return bValid;
}

bool UGA_Enemy_MoneyGun::StartPattern()
{
	Settings = GetPatternSettings<UMoneyGunPatternSettings>();

	const float Damage = CalculatePatternDamage();
	if (!FMath::IsFinite(Damage)
		|| !SpawnGun(FindAnchor(Settings->LeftAnchorTag), Damage)
		|| !SpawnGun(FindAnchor(Settings->RightAnchorTag), Damage))
	{
		return false;
	}

	if (ExecutionPattern.bLockRotation)
	{
		LockRotation();
	}

	GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &UGA_Enemy_MoneyGun::UpdatePattern, UpdateInterval, true);

	return true;
}

void UGA_Enemy_MoneyGun::StopPattern(bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
	}

	for (AMoneyGun* Gun : Guns)
	{
		if (!IsValid(Gun))
		{
			continue;
		}

		// 정상 종료 시 발사된 탄환은 유지하고, 취소 시에는 남은 탄환도 제거
		if (bWasCancelled)
		{
			Gun->ClearProjectiles();
		}
		Gun->Destroy();
	}

	Guns.Reset();
	Settings = nullptr;
}

/* 내부 처리 */

USceneComponent* UGA_Enemy_MoneyGun::FindAnchor(FName AnchorTag) const
{
	// 컴포넌트 이름 대신 BP에서 붙인 태그로 검색: 컴포넌트 이름을 바꿔도 동작
	return Enemy.IsValid() && !AnchorTag.IsNone() ? Enemy->FindComponentByTag<USceneComponent>(AnchorTag) : nullptr;
}

bool UGA_Enemy_MoneyGun::SpawnGun(USceneComponent* Anchor, float Damage)
{
	if (!Anchor || !Settings->GunClass)
	{
		return false;
	}

	FActorSpawnParameters Params;
	Params.Owner = Enemy.Get();
	Params.Instigator = Enemy.Get();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AMoneyGun* Gun = GetWorld()->SpawnActor<AMoneyGun>(Settings->GunClass, Anchor->GetComponentTransform(), Params);
	if (!Gun)
	{
		return false;
	}

	Guns.Add(Gun);

	// 물체 애니메이션을 상속하지 않도록 앵커는 캐릭터 캡슐 아래에 배치되어 있음
	Gun->GetRootComponent()->SetAbsolute(false, true, true);
	if (!Gun->AttachToComponent(Anchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale))
	{
		return false;
	}

	return Gun->StartPattern(Settings, ExecutionTarget, Damage);
}

void UGA_Enemy_MoneyGun::UpdatePattern()
{
	bool bCancel = !IsPatternExecutionValid() || !EnemyTargetUtils::IsAttackableTarget(ExecutionTarget);
	bool bAllFinished = true;

	for (const AMoneyGun* Gun : Guns)
	{
		if (!IsValid(Gun) || Gun->HasFailed())
		{
			bCancel = true;
		}

		if (IsValid(Gun) && !Gun->HasFinished())
		{
			bAllFinished = false;
		}
	}

	if (bCancel)
	{
		CancelPattern();
	}
	else if (bAllFinished)
	{
		FinishPattern();
	}
}
