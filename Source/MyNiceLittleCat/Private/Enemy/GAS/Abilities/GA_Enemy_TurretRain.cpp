#include "Enemy/GAS/Abilities/GA_Enemy_TurretRain.h"
#include "Enemy/Data/TurretRainPatternSettings.h"
#include "Enemy/EnemyCharacter.h"
#include "Gimmicks/Turret/Turret.h"
#include "Characters/PlayerCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Animation/SkeletalMeshActor.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"

#include "Gimmicks/Magnetic/MagneticComponent.h"

namespace
{
	// 보스가 죽거나 사라지거나 플레이어가 죽으면 스폰된 액터를 제거하는 감시 타이머 (어빌리티 인스턴스 수명과 무관)
	// 보스 사망 시 패턴 컴포넌트가 GA를 회수해서 GA 쪽 바인딩만으로는 정리가 안 됨
	template <typename TSpawned, typename TKillFunc>
	void WatchTurretOwner(UWorld* World, TSpawned* Spawned, AEnemyCharacter* Boss, TKillFunc KillFunc)
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
bool UGA_Enemy_TurretRain::CanStartPattern() const
{
	// 멤버 Settings는 StartPattern에서 채우므로 지역 변수 이름을 따로 씀 (이름 가림 방지)
	const UTurretRainPatternSettings* RainSettings = GetPatternSettings<UTurretRainPatternSettings>();
	if (!RainSettings || !RainSettings->IsValidConfiguration() || !IsValid(ExecutionTarget)
		|| !RainSettings->FitsExecutionTime(ExecutionPattern.MaxExecutionSeconds))
	{
		UE_LOG(LogTemp, Error, TEXT("TurretRain: invalid settings or MaxExecutionSeconds too short (needs > %.1f) on %s"),
			RainSettings ? RainSettings->GetExecutionSeconds() + 0.5f : 0.f, *GetNameSafe(Enemy.Get()));
		return false;
	}
	return true;
}

bool UGA_Enemy_TurretRain::StartPattern()
{
	Settings = GetPatternSettings<UTurretRainPatternSettings>();
	Target = ExecutionTarget.Get();

	// 백그라운드 모드: 이전 포탑이 남아 있으면 이번엔 실행하지 않음 (다른 공격이 선택되도록)
	BackgroundTurrets.RemoveAll([](const TObjectPtr<ATurret>& T) { return !IsValid(T); });
	if (Settings->bRunInBackground && BackgroundTurrets.Num() > 0)
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

	BindOwnerCleanup(Enemy.Get());

	CyclesDone = 0;
	GetWorld()->GetTimerManager().SetTimer(CancelCheckTimer, this, &UGA_Enemy_TurretRain::CheckCancel, 0.1f, true);
	StartCycle();
	return true;
}

void UGA_Enemy_TurretRain::StartCycle()
{
	// 낙하 시작 시점의 플레이어 위치를 원 중심으로 고정 (도중에 움직여도 원은 그대로)
	CycleCenter = Enemy.IsValid() ? Enemy->GetActorLocation() : CycleCenter;
	AngleOffset = FMath::FRandRange(0.f, 360.f);
	DroppedThisCycle = 0;
	DropNext();
}

void UGA_Enemy_TurretRain::DropNext()
{
	if (!Settings) return;

	const float Angle = AngleOffset + 360.f * DroppedThisCycle / Settings->TurretCount;
	const FVector Around = CycleCenter + FRotator(0.f, Angle, 0.f).Vector() * Settings->Radius;

	FVector Ground;
	if (FindGround(Around, Ground))
	{
		if (Settings->WarningClass)
		{
			if (AActor* Warning = GetWorld()->SpawnActor<AActor>(Settings->WarningClass, FTransform(Ground)))
			{
				Warning->SetLifeSpan(Settings->WarningSeconds);
			}
		}

		// 플레이어(원 중심)를 바라보게 스폰
		const FRotator Facing(0.f, (-CycleCenter + Ground).Rotation().Yaw, 0.f);
		FActorSpawnParameters Params;
		Params.Owner = Enemy.Get();
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (ATurret* Turret = GetWorld()->SpawnActor<ATurret>(Settings->TurretClass,
			Ground + FVector(0.f, 0.f, Settings->DropHeight), Facing, Params))
		{
			if (Settings->bRandomizePolarity)
			{
				if (UMagneticComponent* Magnetic = Turret->FindComponentByClass<UMagneticComponent>(); Magnetic && Magnetic->IsMagnetEnabled())
				{
					Magnetic->SetPolarity(FMath::RandBool() ? EMagneticPolarity::N : EMagneticPolarity::S);
				}
			}
			if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Turret->GetRootComponent()); Body && Body->IsSimulatingPhysics())
			{
				Body->SetPhysicsLinearVelocity(FVector(0.f, 0.f, -Settings->DropSpeed));
			}
			Turrets.Add(Turret);

			// 보스 사망·제거, 플레이어 사망 시 정리 (FX 설정은 데이터 에셋이라 GA가 사라져도 유효)
			TWeakObjectPtr<UTurretRainPatternSettings> WeakFX(Settings);
			TWeakObjectPtr<UGA_Enemy_TurretRain> WeakThis(this);
			WatchTurretOwner(GetWorld(), Turret, Enemy.Get(), [WeakFX, WeakThis](ATurret* T)
			{
				UE_LOG(LogTemp, Log, TEXT("[TurretRain] Boss gone or player dead -> destroying %s"), *GetNameSafe(T));
				if (WeakThis.IsValid() && WeakThis->GetWorld()) WeakThis->DestroyWithFX(T);
				else T->Destroy();
			});
		}
	}

	++DroppedThisCycle;
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	if (DroppedThisCycle < Settings->TurretCount)
	{
		Timers.SetTimer(StepTimer, this, &UGA_Enemy_TurretRain::DropNext, FMath::Max(Settings->DropInterval, 0.01f), false);
	}
	else if (Settings->bRunInBackground)
	{
		// 포탑은 남겨두고 패턴만 종료 → 보스는 바로 다음 행동(추격·근접)으로
		for (ATurret* Turret : Turrets)
		{
			if (!IsValid(Turret)) continue;
			Turret->SetLifeSpan(Settings->ActiveSeconds + 2.f);   // 보스가 사라져도 결국 정리되도록 안전장치
			BackgroundTurrets.Add(Turret);
		}
		Turrets.Reset();
		BackgroundSettings = Settings;
		TWeakObjectPtr<UGA_Enemy_TurretRain> WeakThis(this);
		Timers.SetTimer(BackgroundDestroyTimer, FTimerDelegate::CreateLambda([WeakThis]()
		{
			if (WeakThis.IsValid()) WeakThis->DestroyBackgroundTurrets();
		}), Settings->ActiveSeconds, false);

		FinishPattern();
	}
	else
	{
		Timers.SetTimer(StepTimer, this, &UGA_Enemy_TurretRain::DestroyTurrets, Settings->ActiveSeconds, false);
	}
}

void UGA_Enemy_TurretRain::DestroyTurrets()
{
	for (ATurret* Turret : Turrets)
	{
		if (!IsValid(Turret)) continue;
		if (Settings && Settings->bSpareCarriedTurrets && Turret->IsCarried()) continue;

		DestroyWithFX(Turret);
	}
	Turrets.Reset();

	++CyclesDone;
	if (!Settings || CyclesDone >= Settings->Cycles)
	{
		FinishPattern();
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(StepTimer, this, &UGA_Enemy_TurretRain::StartCycle, FMath::Max(Settings->CycleGapSeconds, 0.01f), false);
}

void UGA_Enemy_TurretRain::DestroyBackgroundTurrets()
{
	for (ATurret* Turret : BackgroundTurrets)
	{
		if (!IsValid(Turret)) continue;
		if (BackgroundSettings && BackgroundSettings->bSpareCarriedTurrets && Turret->IsCarried()) continue;
		DestroyWithFX(Turret);
	}
	BackgroundTurrets.Reset();
}

void UGA_Enemy_TurretRain::DestroyWithFX(ATurret* Turret) const
{

	const UTurretRainPatternSettings* FX = Settings ? Settings.Get() : BackgroundSettings.Get();
	const FString FXName = GetNameSafe(FX);
	const FString MeshName = FX ? GetNameSafe(FX->DestroyMesh) : TEXT("-");
	const FString AnimName = FX ? GetNameSafe(FX->DestroyAnim) : TEXT("-");
	UE_LOG(LogTemp, Warning, TEXT("[TurretRain] DestroyWithFX FX=%s Mesh=%s Anim=%s"), *FXName, *MeshName, *AnimName);
	if (FX && FX->DestroyEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), FX->DestroyEffect, Turret->GetActorLocation());
	}
	if (FX && FX->DestroySound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), FX->DestroySound, Turret->GetActorLocation());
	}
	
	if (FX && FX->DestroyMesh && FX->DestroyAnim)
	{
		const FTransform Xf(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Turret->GetActorLocation(), FVector(FX->DestroyMeshScale));

		// 지연 스폰: 첫 프레임에 레퍼런스 포즈(모든 조각이 한꺼번에 보임)가 렌더링되지 않도록 세팅 후 스폰 완료
		ASkeletalMeshActor* Boom = GetWorld()->SpawnActorDeferred<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), Xf,
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Boom)
		{
			USkeletalMeshComponent* Comp = Boom->GetSkeletalMeshComponent();
			Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Comp->SetCastShadow(false);
			Comp->bEnableUpdateRateOptimizations = false;
			Comp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

			// 레벨에 배치한 것과 동일하게: Use Animation Asset + 재생 1회 (등록 시 엔진이 초기화)
			Comp->SetSkeletalMeshAsset(FX->DestroyMesh);
			Comp->AnimationData.AnimToPlay = FX->DestroyAnim;
			Comp->AnimationData.bSavedLooping = false;
			Comp->AnimationData.bSavedPlaying = true;
			Comp->AnimationData.SavedPosition = 0.f;
			Comp->AnimationData.SavedPlayRate = 1.f;
			Comp->SetAnimationMode(EAnimationMode::AnimationSingleNode);

			Boom->FinishSpawning(Xf);
			Boom->SetLifeSpan(FX->DestroyAnim->GetPlayLength());
		}
	}
	Turret->Destroy();
}

void UGA_Enemy_TurretRain::CheckCancel()
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

void UGA_Enemy_TurretRain::BindOwnerCleanup(AEnemyCharacter* OwnerEnemy)
{
	if (bCleanupBound || !IsValid(OwnerEnemy)) return;
	OwnerEnemy->OnDeathStarted.AddUObject(this, &UGA_Enemy_TurretRain::HandleOwnerDeathStarted);
	OwnerEnemy->OnDestroyed.AddUniqueDynamic(this, &UGA_Enemy_TurretRain::HandleOwnerDestroyed);
	bCleanupBound = true;
}

void UGA_Enemy_TurretRain::HandleOwnerDeathStarted(AEnemyCharacter* DeadEnemy)
{
	KillSpawnedTurrets();
}

void UGA_Enemy_TurretRain::HandleOwnerDestroyed(AActor* DestroyedActor)
{
	KillSpawnedTurrets();
}

void UGA_Enemy_TurretRain::KillSpawnedTurrets()
{
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown) return;

	World->GetTimerManager().ClearTimer(StepTimer);
	World->GetTimerManager().ClearTimer(BackgroundDestroyTimer);

	// 들고 있는 포탑도 예외 없이 파괴
	for (ATurret* Turret : Turrets)
	{
		if (IsValid(Turret)) DestroyWithFX(Turret);
	}
	for (ATurret* Turret : BackgroundTurrets)
	{
		if (IsValid(Turret)) DestroyWithFX(Turret);
	}
	Turrets.Reset();
	BackgroundTurrets.Reset();
}

bool UGA_Enemy_TurretRain::FindGround(const FVector& Around, FVector& OutGround) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TurretRainGround), false);
	if (Enemy.IsValid()) Params.AddIgnoredActor(Enemy.Get());
	if (Target.IsValid()) Params.AddIgnoredActor(Target.Get());

	const FVector Start = Around + FVector(0.f, 0.f, 2000.f);
	const FVector End = Around - FVector(0.f, 0.f, 3000.f);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) return false;

	OutGround = Hit.ImpactPoint;
	return true;
}

// 부모 EndAbility가 먼저 호출 → 이 정리 뒤에 부모가 회전 잠금 해제, Enemy/PatternComponent/RequestId 초기화, Super::EndAbility 진행
void UGA_Enemy_TurretRain::StopPattern(bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepTimer);
		World->GetTimerManager().ClearTimer(CancelCheckTimer);
	}
	// 취소(보스 사망, 플레이어 사망 등) 시 남은 포탑 정리
	for (ATurret* Turret : Turrets)
	{
		if (IsValid(Turret)) Turret->Destroy();
	}
	Turrets.Reset();

	Settings = nullptr;
	Target.Reset();
}
