#include "Enemy/GAS/Abilities/GA_Enemy_MoneyGun.h"
#include "Enemy/Datas/MoneyGunPatternSettings.h"
#include "Enemy/Boss/Objects/MoneyGun.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/GAS/EnemyAttributeSet.h"
#include "Characters/PlayerCharacter.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UGA_Enemy_MoneyGun::UGA_Enemy_MoneyGun()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UGA_Enemy_MoneyGun::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Enemy = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	PatternComponent = Enemy.IsValid() ? Enemy->GetPatternComponent() : nullptr;
	FEnemyPatternDefinition Definition;
	AActor* ExecutionTarget = nullptr;
	if (!Enemy.IsValid() || !Enemy->IsAlive() || !PatternComponent.IsValid()
		|| !PatternComponent->GetActiveExecution(Definition, ExecutionTarget, RequestId))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Settings = Cast<UMoneyGunPatternSettings>(Definition.ExecutionSettings);
	Target = ExecutionTarget;
	if (!Settings || !Settings->IsValidConfiguration() || !Target.IsValid()
		|| !Enemy->GetAttributeSet() || Settings->GetExecutionSeconds() + 0.1f >= Definition.MaxExecutionSeconds
		|| !FindAnchor(Settings->LeftAnchorName) || !FindAnchor(Settings->RightAnchorName)
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Error, TEXT("MoneyGun: invalid settings, missing named anchors, or execution timeout too short on %s"), *GetNameSafe(Enemy.Get()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Damage = Enemy->GetAttributeSet()->GetAttackPower() * Definition.DamageMultiplier;
	if (!FMath::IsFinite(Damage) || !SpawnGun(FindAnchor(Settings->LeftAnchorName))
		|| !SpawnGun(FindAnchor(Settings->RightAnchorName)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (Definition.bLockRotation)
	{
		Enemy->AcquireRotationLock();
		bOwnsRotationLock = true;
	}
	GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &UGA_Enemy_MoneyGun::UpdatePattern, 0.05f, true);
}

USceneComponent* UGA_Enemy_MoneyGun::FindAnchor(FName Name) const
{
	if (!Enemy.IsValid())
	{
		return nullptr;
	}
	TArray<USceneComponent*> Components;
	Enemy->GetComponents<USceneComponent>(Components);
	for (USceneComponent* Component : Components)
		if (Component->GetFName() == Name)
		{
			return Component;
		}
	return nullptr;
}

bool UGA_Enemy_MoneyGun::SpawnGun(USceneComponent* Anchor)
{
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
	Gun->GetRootComponent()->SetAbsolute(false, true, true);
	if (!Gun->AttachToComponent(Anchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale))
	{
		return false;
	}
	// 몸체 애니메이션을 상속하지 않도록 부착 지점은 캐릭터 캡슐에 배치한다.
	return Gun->StartPattern(Settings, Target.Get(), Damage);
}

void UGA_Enemy_MoneyGun::UpdatePattern()
{
	bool bCancel = !Enemy.IsValid() || !Enemy->IsAlive() || !Target.IsValid()
		|| !PatternComponent.IsValid() || !PatternComponent->IsRequestRunning(RequestId);
	if (const APlayerCharacter* Player = Cast<APlayerCharacter>(Target.Get()))
	{
		bCancel |= Player->GetbIsDead();
	}
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
	if (bCancel || bAllFinished)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancel);
		return;
	}
}

void UGA_Enemy_MoneyGun::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
	}
	for (AMoneyGun* Gun : Guns)
	{
		if (!IsValid(Gun))
		{
			continue;
		}
		// 정상 종료 시 발사된 탄환은 유지하고, 취소 시에는 남은 탄환도 제거한다.
		if (bWasCancelled)
		{
			Gun->ClearProjectiles();
		}
		Gun->Destroy();
	}
	Guns.Reset();
	if (Enemy.IsValid())
	{
		if (bOwnsRotationLock)
		{
			Enemy->ReleaseRotationLock();
		}
	}
	bOwnsRotationLock = false;
	Settings = nullptr;
	Target.Reset();
	PatternComponent.Reset();
	Enemy.Reset();
	RequestId = 0;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
