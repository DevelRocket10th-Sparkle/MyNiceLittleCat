#include "Enemy/GAS/Abilities/GA_Enemy_BombDrop.h"
#include "Enemy/Data/BombDropPatternSettings.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/Bomb/BombEnemy.h"
#include "Characters/PlayerCharacter.h"
#include "GameFramework/Character.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"

#include "Gimmicks/Magnetic/MagneticComponent.h"

namespace
{
	// 조준점을 내비메시 위, 가장자리에서 EdgePadding 이상 안쪽으로 옮김. 못 찾으면 false (플레이어 쪽으로 대체)
	bool ProjectBombAimToNavMesh(UWorld* World, const UBombDropPatternSettings* Settings, const FVector& FallbackCenter, FVector& InOutAim)
	{
		UNavigationSystemV1* NavSys = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
		if (!NavSys || !Settings) return false;

		const FVector Extent(Settings->NavSearchRadius, Settings->NavSearchRadius, 500.f);
		FNavLocation NavLoc;
		if (!NavSys->ProjectPointToNavigation(InOutAim, NavLoc, Extent)
			&& !NavSys->ProjectPointToNavigation(FallbackCenter, NavLoc, Extent))
		{
			return false;
		}
		FVector Point = NavLoc.Location;

#if WITH_RECAST
		if (Settings->EdgePadding > 0.f)
		{
			if (const ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)))
			{
				// 벽(가장자리)에서 밀어내기, 두 번까지 반복해서 모서리도 처리
				for (int32 Step = 0; Step < 2; ++Step)
				{
					FVector Wall;
					const float Dist = NavMesh->FindDistanceToWall(Point, nullptr, Settings->EdgePadding, &Wall);
					if (Dist >= Settings->EdgePadding) break;

					FVector Away = (Point - Wall).GetSafeNormal2D();
					if (Away.IsNearlyZero()) Away = (FallbackCenter - Point).GetSafeNormal2D();
					const FVector Pushed = Point + Away * (Settings->EdgePadding - Dist + 10.f);

					FNavLocation PushedNav;
					if (!NavSys->ProjectPointToNavigation(Pushed, PushedNav, FVector(100.f, 100.f, 500.f))) break;
					Point = PushedNav.Location;
				}
			}
		}
#endif
		InOutAim = Point;
		return true;
	}

	// 보스가 죽거나 사라지거나 플레이어가 죽으면 스폰된 액터를 제거하는 감시 타이머 (어빌리티 인스턴스 수명과 무관)
	// 보스 사망 시 패턴 컴포넌트가 GA를 회수해서 GA 쪽 바인딩만으로는 정리가 안 됨
	template <typename TSpawned, typename TKillFunc>
	void WatchBombOwner(UWorld* World, TSpawned* Spawned, AEnemyCharacter* Boss, TKillFunc KillFunc)
	{
		if (!World || !IsValid(Spawned)) return;

		TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
		TWeakObjectPtr<UWorld> WeakWorld(World);
		TWeakObjectPtr<TSpawned> WeakSpawned(Spawned);
		TWeakObjectPtr<AEnemyCharacter> WeakBoss(Boss);

		World->GetTimerManager().SetTimer(*Handle, FTimerDelegate::CreateLambda([WeakWorld, WeakSpawned, WeakBoss, Handle, KillFunc]()
		{
			UWorld* W = WeakWorld.Get();
			if (!W || W->bIsTearingDown) return;

			const APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(W, 0));
			const bool bBossGone = !WeakBoss.IsValid() || !WeakBoss->IsAlive();
			const bool bPlayerDead = Player && Player->GetbIsDead();
			if (WeakSpawned.IsValid() && !bBossGone && !bPlayerDead) return;

			W->GetTimerManager().ClearTimer(*Handle);
			if (TSpawned* Target = WeakSpawned.Get())
			{
				KillFunc(Target);
			}
		}), 0.1f, true);
	}
}

// 요청 수신, Enemy 생존 확인, CommitAbility는 부모 ActivateAbility가 처리 → 여기서는 설정만 검사
bool UGA_Enemy_BombDrop::CanStartPattern() const
{
	// 멤버 Settings는 StartPattern에서 채우므로 지역 변수 이름을 따로 씀 (이름 가림 방지)
	const UBombDropPatternSettings* BombSettings = GetPatternSettings<UBombDropPatternSettings>();
	if (!BombSettings || !BombSettings->IsValidConfiguration() || !IsValid(ExecutionTarget)
		|| BombSettings->GetExecutionSeconds() + 0.5f >= ExecutionPattern.MaxExecutionSeconds)
	{
		UE_LOG(LogTemp, Error, TEXT("BombDrop: invalid settings or MaxExecutionSeconds too short (needs > %.1f) on %s"),
			BombSettings ? BombSettings->GetExecutionSeconds() + 0.5f : 0.f, *GetNameSafe(Enemy.Get()));
		return false;
	}
	return true;
}

bool UGA_Enemy_BombDrop::StartPattern()
{
	// 폭탄은 설정이 유효할 때만 생기므로 바인딩을 설정 검사 뒤로 옮겨도 정리 범위는 같음
	BindOwnerCleanup(Enemy.Get());

	Settings = GetPatternSettings<UBombDropPatternSettings>();
	Target = ExecutionTarget.Get();

	// 이미 폭탄이 꽉 차 있으면 이번엔 실행하지 않음 (다른 공격이 선택되도록)
	if (Settings->MaxAliveBombs > 0 && CountAliveBombs() >= Settings->MaxAliveBombs)
	{
		// 기존과 같이 정상 종료(취소 아님): 이미 종료됐으므로 true 반환
		FinishPattern();
		return true;
	}

	// 회전 잠금 해제는 부모(UEnemyPatternAbility)가 EndAbility에서 처리
	if (ExecutionPattern.bLockRotation)
	{
		LockRotation();
	}

	Dropped = 0;
	GetWorld()->GetTimerManager().SetTimer(CancelCheckTimer, this, &UGA_Enemy_BombDrop::CheckCancel, 0.1f, true);
	DropNext();
	return true;
}

void UGA_Enemy_BombDrop::BindOwnerCleanup(AEnemyCharacter* OwnerEnemy)
{
	if (bCleanupBound || !IsValid(OwnerEnemy)) return;
	OwnerEnemy->OnDeathStarted.AddUObject(this, &UGA_Enemy_BombDrop::HandleOwnerDeathStarted);
	OwnerEnemy->OnDestroyed.AddUniqueDynamic(this, &UGA_Enemy_BombDrop::HandleOwnerDestroyed);
	bCleanupBound = true;
}

void UGA_Enemy_BombDrop::HandleOwnerDeathStarted(AEnemyCharacter* DeadEnemy)
{
	KillSpawnedBombs();
}

void UGA_Enemy_BombDrop::HandleOwnerDestroyed(AActor* DestroyedActor)
{
	KillSpawnedBombs();
}

void UGA_Enemy_BombDrop::KillSpawnedBombs()
{
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown) return;

	World->GetTimerManager().ClearTimer(StepTimer);
	for (const TWeakObjectPtr<ABombEnemy>& Bomb : AliveBombs)
	{
		if (Bomb.IsValid())
		{
			Bomb->Destroy();
		}
	}
	AliveBombs.Reset();
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

	FVector Aim = ComputeAimPoint(Dropped > 0);
	if (Settings->bKeepOnNavMesh)
	{
		ProjectBombAimToNavMesh(GetWorld(), Settings, Target->GetActorLocation(), Aim);
	}

	bool bHasGround = bRoomForBomb && FindGround(Aim, Ground);

	// 플레이어와 다른 높이(계단·구덩이 등)에 떨어지면 폭탄이 갇히므로 플레이어 근처로 다시 조준
	FVector PlayerGround;
	if (bHasGround && Settings->MaxGroundHeightDifference > 0.f && FindGround(Target->GetActorLocation(), PlayerGround)
		&& FMath::Abs(Ground.Z - PlayerGround.Z) > Settings->MaxGroundHeightDifference)
	{
		Aim = PlayerGround;
		if (Settings->bKeepOnNavMesh)
		{
			ProjectBombAimToNavMesh(GetWorld(), Settings, PlayerGround, Aim);
		}
		bHasGround = FindGround(Aim, Ground) && FMath::Abs(Ground.Z - PlayerGround.Z) <= Settings->MaxGroundHeightDifference;
	}

	if (bHasGround)
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
		Params.Owner = Enemy.Get();
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		if (ABombEnemy* Bomb = GetWorld()->SpawnActor<ABombEnemy>(Settings->BombClass, SpawnLocation, Facing, Params))
		{
			if (Settings->bRandomizePolarity)
			{
				if (UMagneticComponent* Magnetic = Bomb->FindComponentByClass<UMagneticComponent>(); Magnetic && Magnetic->IsMagnetEnabled())
				{
					Magnetic->SetPolarity(FMath::RandBool() ? EMagneticPolarity::N : EMagneticPolarity::S);
				}
			}
			// 런타임 스폰은 AutoPossessAI(PlacedInWorld)에 안 걸리므로 직접 AI 붙이기
			if (!Bomb->GetController())
			{
				Bomb->SpawnDefaultController();
			}
			Bomb->LaunchCharacter(FVector(0.f, 0.f, -Settings->DropSpeed), true, true);
			AliveBombs.Add(Bomb);

			// 폭탄이 남아 있는 동안 패턴의 위험 태그와 위험도 유지 (같은 위험 태그의 패턴과 겹치지 않음)
			RegisterLingeringHazard(Bomb);

			WatchBombOwner(GetWorld(), Bomb, Enemy.Get(), [](ABombEnemy* B)
			{
				UE_LOG(LogTemp, Log, TEXT("[BombDrop] Boss gone or player dead -> destroying %s"), *GetNameSafe(B));
				B->Destroy();
			});

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
		FinishPattern();
	}
}

void UGA_Enemy_BombDrop::CheckCancel()
{
	// 활성·생존·요청 진행 확인은 부모 도우미로 대체, 타깃 확인만 이 GA의 조건
	bool bCancel = !IsPatternExecutionValid() || !Target.IsValid();
	if (const APlayerCharacter* Player = Cast<APlayerCharacter>(Target.Get()))
	{
		bCancel |= Player->GetbIsDead();
	}
	if (bCancel)
	{
		CancelPattern();
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

// 부모 EndAbility가 먼저 호출 → 이 정리 뒤에 부모가 회전 잠금 해제, Enemy/PatternComponent/RequestId 초기화, Super::EndAbility 진행
void UGA_Enemy_BombDrop::StopPattern(bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepTimer);
		World->GetTimerManager().ClearTimer(CancelCheckTimer);
	}
	Settings = nullptr;
	Target.Reset();
}
