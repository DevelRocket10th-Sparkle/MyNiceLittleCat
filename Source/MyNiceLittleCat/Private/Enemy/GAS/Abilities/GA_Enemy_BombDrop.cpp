#include "Enemy/GAS/Abilities/GA_Enemy_BombDrop.h"
#include "Enemy/Datas/BombDropPatternSettings.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/Bomb/BombEnemy.h"
#include "Characters/PlayerCharacter.h"
#include "GameFramework/Character.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

UGA_Enemy_BombDrop::UGA_Enemy_BombDrop()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UGA_Enemy_BombDrop::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

	Settings = Cast<UBombDropPatternSettings>(Definition.ExecutionSettings);
	Target = ExecutionTarget;
	if (!Settings || !Settings->IsValidConfiguration() || !Target.IsValid()
		|| Settings->GetExecutionSeconds() + 0.5f >= Definition.MaxExecutionSeconds
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Error, TEXT("BombDrop: invalid settings or MaxExecutionSeconds too short (needs > %.1f) on %s"),
			Settings ? Settings->GetExecutionSeconds() + 0.5f : 0.f, *GetNameSafe(Enemy.Get()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 이미 폭탄이 꽉 차 있으면 이번엔 실행하지 않음 (다른 공격이 선택되도록)
	if (Settings->MaxAliveBombs > 0 && CountAliveBombs() >= Settings->MaxAliveBombs)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (Definition.bLockRotation)
	{
		Enemy->AcquireRotationLock();
		bOwnsRotationLock = true;
	}

	Dropped = 0;
	GetWorld()->GetTimerManager().SetTimer(CancelCheckTimer, this, &UGA_Enemy_BombDrop::CheckCancel, 0.1f, true);
	DropNext();
}

int32 UGA_Enemy_BombDrop::CountAliveBombs()
{
	AliveBombs.RemoveAll([](const TWeakObjectPtr<ABombEnemy>& B) { return !B.IsValid() || !B->IsAlive(); });
	return AliveBombs.Num();
}

FVector UGA_Enemy_BombDrop::ComputeAimPoint(bool bScatter) const
{
	const AActor* T = Target.Get();
	const FVector Location = T->GetActorLocation();

	FVector Velocity = T->GetVelocity();
	Velocity.Z = 0.f;

	// 착지 순간 플레이어가 있을 위치
	const float FallSeconds = Settings->GetFallSeconds(GetWorld()->GetGravityZ());
	FVector Aim = Location + Velocity * FallSeconds * Settings->PredictionScale;

	// 그 앞쪽 (멈춰 있으면 바라보는 방향)
	const FVector Ahead = Velocity.SizeSquared() > FMath::Square(50.f)
		? Velocity.GetSafeNormal()
		: T->GetActorForwardVector().GetSafeNormal2D();
	Aim += Ahead * Settings->ForwardOffset;

	if (bScatter && Settings->ScatterRadius > 0.f)
	{
		const FVector2D R = FMath::RandPointInCircle(Settings->ScatterRadius);
		Aim += FVector(R.X, R.Y, 0.f);
	}
	return Aim;
}

void UGA_Enemy_BombDrop::DropNext()
{
	if (!Settings || !Target.IsValid()) return;

	FVector Ground;
	const bool bRoomForBomb = Settings->MaxAliveBombs <= 0 || CountAliveBombs() < Settings->MaxAliveBombs;

	if (bRoomForBomb && FindGround(ComputeAimPoint(Dropped > 0), Ground))
	{
		const float FallSeconds = Settings->GetFallSeconds(GetWorld()->GetGravityZ());

		if (Settings->WarningClass)
		{
			if (AActor* Warning = GetWorld()->SpawnActor<AActor>(Settings->WarningClass, FTransform(Ground)))
			{
				Warning->SetLifeSpan(FallSeconds);
			}
		}

		const FVector SpawnLocation = Ground + FVector(0.f, 0.f, Settings->DropHeight);
		const FRotator Facing(0.f, (Target->GetActorLocation() - Ground).Rotation().Yaw, 0.f);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		if (ABombEnemy* Bomb = GetWorld()->SpawnActor<ABombEnemy>(Settings->BombClass, SpawnLocation, Facing, Params))
		{
			// 런타임 스폰은 AutoPossessAI(PlacedInWorld)에 안 걸리므로 직접 AI 붙이기
			if (!Bomb->GetController())
			{
				Bomb->SpawnDefaultController();
			}
			Bomb->LaunchCharacter(FVector(0.f, 0.f, -Settings->DropSpeed), true, true);
			AliveBombs.Add(Bomb);

			if (Settings->SpawnEffect)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Settings->SpawnEffect, SpawnLocation);
			}
			if (Settings->DropSound)
			{
				UGameplayStatics::PlaySoundAtLocation(GetWorld(), Settings->DropSound, SpawnLocation);
			}
		}
	}

	++Dropped;
	if (Dropped < Settings->BombCount)
	{
		GetWorld()->GetTimerManager().SetTimer(StepTimer, this, &UGA_Enemy_BombDrop::DropNext, FMath::Max(Settings->DropInterval, 0.01f), false);
	}
	else
	{
		// 폭탄은 스스로 움직이므로 다 떨어뜨리면 바로 패턴 종료
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGA_Enemy_BombDrop::CheckCancel()
{
	bool bCancel = !Enemy.IsValid() || !Enemy->IsAlive() || !Target.IsValid()
		|| !PatternComponent.IsValid() || !PatternComponent->IsRequestRunning(RequestId);
	if (const APlayerCharacter* Player = Cast<APlayerCharacter>(Target.Get()))
	{
		bCancel |= Player->GetbIsDead();
	}
	if (bCancel)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

bool UGA_Enemy_BombDrop::FindGround(const FVector& Around, FVector& OutGround) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BombDropGround), false);
	if (Enemy.IsValid()) Params.AddIgnoredActor(Enemy.Get());
	if (Target.IsValid()) Params.AddIgnoredActor(Target.Get());

	const FVector Start = Around + FVector(0.f, 0.f, 2000.f);
	const FVector End = Around - FVector(0.f, 0.f, 3000.f);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) return false;

	OutGround = Hit.ImpactPoint;
	return true;
}

void UGA_Enemy_BombDrop::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepTimer);
		World->GetTimerManager().ClearTimer(CancelCheckTimer);
	}
	if (Enemy.IsValid() && bOwnsRotationLock)
	{
		Enemy->ReleaseRotationLock();
	}
	bOwnsRotationLock = false;
	Settings = nullptr;
	Target.Reset();
	PatternComponent.Reset();
	Enemy.Reset();
	RequestId = 0;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
