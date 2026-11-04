#include "Enemy/GAS/Abilities/GA_Enemy_TurretRain.h"
#include "Enemy/Datas/TurretRainPatternSettings.h"
#include "Enemy/Components/EnemyPatternComponent.h"
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

UGA_Enemy_TurretRain::UGA_Enemy_TurretRain()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UGA_Enemy_TurretRain::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

	Settings = Cast<UTurretRainPatternSettings>(Definition.ExecutionSettings);
	Target = ExecutionTarget;
	if (!Settings || !Settings->IsValidConfiguration() || !Target.IsValid()
		|| Settings->GetExecutionSeconds() + 0.5f >= Definition.MaxExecutionSeconds
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Error, TEXT("TurretRain: invalid settings or MaxExecutionSeconds too short (needs > %.1f) on %s"),
			Settings ? Settings->GetExecutionSeconds() + 0.5f : 0.f, *GetNameSafe(Enemy.Get()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 백그라운드 모드: 이전 포탑이 남아 있으면 이번엔 실행하지 않음 (다른 공격이 선택되도록)
	BackgroundTurrets.RemoveAll([](const TObjectPtr<ATurret>& T) { return !IsValid(T); });
	if (Settings->bRunInBackground && BackgroundTurrets.Num() > 0)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (Definition.bLockRotation)
	{
		Enemy->AcquireRotationLock();
		bOwnsRotationLock = true;
	}

	CyclesDone = 0;
	GetWorld()->GetTimerManager().SetTimer(CancelCheckTimer, this, &UGA_Enemy_TurretRain::CheckCancel, 0.1f, true);
	StartCycle();
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
			if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Turret->GetRootComponent()); Body && Body->IsSimulatingPhysics())
			{
				Body->SetPhysicsLinearVelocity(FVector(0.f, 0.f, -Settings->DropSpeed));
			}
			Turrets.Add(Turret);
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

		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
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
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
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

void UGA_Enemy_TurretRain::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
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
