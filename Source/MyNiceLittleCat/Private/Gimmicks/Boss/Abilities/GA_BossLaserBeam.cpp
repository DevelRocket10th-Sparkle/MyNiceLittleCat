#include "Gimmicks/Boss/Abilities/GA_BossLaserBeam.h"
#include "Gimmicks/Boss/Abilities/BossLaserPatternSettings.h"
#include "Gimmicks/Laser/LaserBeamComponent.h"
#include "Gimmicks/Warning/AttackWarningDecal.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

namespace
{
	// 공격 지점 갱신 간격 (초)
	constexpr float AimUpdateInterval = 1.f / 30.f;

	// 바닥 찾기 트레이스 길이 (타깃 위치에서 아래로)
	constexpr float FloorTraceDistance = 500.f;

	// 빔은 반지름만큼 굵은 구로 쓸고 지나가서, 바닥 지점을 그대로 겨누면 비스듬할 때 지점 앞 바닥에 먼저 닿음
	// → 빔 중심이 바닥 위 반지름 높이에서 지점에 닿도록 반지름만큼 올려서 겨눔
	FVector GetBeamTargetLocation(const ULaserBeamComponent* Laser, const FVector& FloorPoint)
	{
		return FloorPoint + FVector(0.f, 0.f, Laser->GetBeamRadius());
	}
}

UGA_BossLaserBeam::UGA_BossLaserBeam()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UGA_BossLaserBeam::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Enemy = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	PatternComponent = Enemy.IsValid() ? Enemy->GetPatternComponent() : nullptr;

	AActor* Target = nullptr;
	if (!Enemy.IsValid() || !Enemy->IsAlive() || !PatternComponent.IsValid()
		|| !PatternComponent->GetActiveExecution(ExecutionPattern, Target, RequestId))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	LaserSettings = Cast<UBossLaserPatternSettings>(ExecutionPattern.ExecutionSettings);
	Laser = FindLaserComponent();
	ExecutionTarget = Target;

	if (!LaserSettings || !Laser)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossLaser] %s: 패턴 %s에 BossLaserPatternSettings 또는 보스의 LaserBeamComponent가 없습니다."),
			*Enemy->GetName(), *ExecutionPattern.PatternId.ToString());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 바닥을 겨누는 패턴이라 타깃이 꼭 필요
	if (!IsValid(Target) || !FindTargetFloorPoint(AimPoint) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
	{
		AI->StopMovement();
	}
	Enemy->GetCharacterMovement()->StopMovementImmediately();

	Enemy->AcquireAttackMovementLock();
	bOwnsMovementLock = true;
	if (ExecutionPattern.bLockRotation)
	{
		Enemy->AcquireRotationLock();
		bOwnsRotationLock = true;
	}

	if (ExecutionPattern.bFaceTargetOnStart)
	{
		FaceAimPoint();
	}

	if (LaserSettings->ChargeMontage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, LaserSettings->ChargeMontage, 1.f, NAME_None, true);
		MontageTask->ReadyForActivation();
	}

	// 1단계: 추적
	Stage = EBossLaserBeamStage::Tracking;
	ShowAimWarning(0.f);
	Enemy->GetWorldTimerManager().SetTimer(UpdateTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGA_BossLaserBeam::UpdateAim), AimUpdateInterval, true);

	UE_LOG(LogTemp, Log, TEXT("[BossLaser] %s: 추적 시작 (%.2f초)"), *Enemy->GetName(), LaserSettings->AimTrackTime);

	if (LaserSettings->AimTrackTime > 0.f)
	{
		UAbilityTask_WaitDelay* AimTask = UAbilityTask_WaitDelay::WaitDelay(this, LaserSettings->AimTrackTime);
		AimTask->OnFinish.AddDynamic(this, &UGA_BossLaserBeam::HandleAimFinished);
		AimTask->ReadyForActivation();
	}
	else
	{
		HandleAimFinished();
	}
}

void UGA_BossLaserBeam::HandleAimFinished()
{
	if (!IsExecutionValid())
	{
		EndWithResult(true);
		return;
	}

	// 2단계: 지점 확정 → 경고가 LockDelay 동안 차오름
	Stage = EBossLaserBeamStage::Locked;
	ShowAimWarning(LaserSettings->LockDelay);

	UE_LOG(LogTemp, Log, TEXT("[BossLaser] %s: 지점 확정 %s, %.2f초 뒤 발사"),
		*Enemy->GetName(), *AimPoint.ToCompactString(), LaserSettings->LockDelay);

	if (LaserSettings->LockDelay > 0.f)
	{
		UAbilityTask_WaitDelay* LockTask = UAbilityTask_WaitDelay::WaitDelay(this, LaserSettings->LockDelay);
		LockTask->OnFinish.AddDynamic(this, &UGA_BossLaserBeam::HandleLockFinished);
		LockTask->ReadyForActivation();
	}
	else
	{
		HandleLockFinished();
	}
}

void UGA_BossLaserBeam::HandleLockFinished()
{
	if (!IsExecutionValid())
	{
		EndWithResult(true);
		return;
	}

	// 3단계: 발사
	Stage = EBossLaserBeamStage::Firing;
	HideAimWarning();

	Laser->OnLaserStopped.AddUniqueDynamic(this, &UGA_BossLaserBeam::HandleLaserStopped);
	Laser->StartLaser(GetBeamTargetLocation(Laser, AimPoint), LaserSettings->FireDuration);

	UE_LOG(LogTemp, Log, TEXT("[BossLaser] %s: 발사 (%.2f초, 발사 중 추적 %s)"),
		*Enemy->GetName(), LaserSettings->FireDuration, LaserSettings->bTrackDuringFire ? TEXT("켬") : TEXT("끔"));
}

void UGA_BossLaserBeam::HandleLaserStopped(ULaserBeamComponent* InLaser)
{
	UE_LOG(LogTemp, Log, TEXT("[BossLaser] 발사 종료"));
	EndWithResult(false);
}

void UGA_BossLaserBeam::UpdateAim()
{
	if (!IsActive() || !IsExecutionValid())
	{
		return;
	}

	if (Stage == EBossLaserBeamStage::Tracking)
	{
		FVector Desired;
		if (FindTargetFloorPoint(Desired))
		{
			MoveAimPointToward(Desired, LaserSettings->AimFollowSpeed, AimUpdateInterval);
		}

		if (IsValid(Warning))
		{
			Warning->SetActorLocation(AimPoint);
		}
		FaceAimPoint();
	}
	else if (Stage == EBossLaserBeamStage::Firing && LaserSettings->bTrackDuringFire)
	{
		FVector Desired;
		if (FindTargetFloorPoint(Desired))
		{
			MoveAimPointToward(Desired, LaserSettings->FireTrackSpeed, AimUpdateInterval);
		}

		Laser->SetTargetLocation(GetBeamTargetLocation(Laser, AimPoint));
		FaceAimPoint();
	}
}

void UGA_BossLaserBeam::MoveAimPointToward(const FVector& Desired, float Speed, float DeltaTime)
{
	if (Speed <= 0.f)
	{
		AimPoint = Desired;
		return;
	}

	AimPoint = FMath::VInterpConstantTo(AimPoint, Desired, DeltaTime, Speed);
}

bool UGA_BossLaserBeam::FindTargetFloorPoint(FVector& OutPoint) const
{
	if (!IsValid(ExecutionTarget) || !Enemy.IsValid())
	{
		return false;
	}

	const FVector TargetLocation = ExecutionTarget->GetActorLocation();

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossLaserFloor), false);
	QueryParams.AddIgnoredActor(ExecutionTarget);
	QueryParams.AddIgnoredActor(Enemy.Get());

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, TargetLocation, TargetLocation - FVector(0.f, 0.f, FloorTraceDistance), ObjectParams, QueryParams))
	{
		OutPoint = Hit.ImpactPoint;
		return true;
	}

	// 바닥을 못 찾으면 캡슐 아래 끝
	OutPoint = TargetLocation;
	if (const ACharacter* Character = Cast<ACharacter>(ExecutionTarget))
	{
		OutPoint.Z -= Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	return true;
}

ULaserBeamComponent* UGA_BossLaserBeam::FindLaserComponent() const
{
	if (!Enemy.IsValid())
	{
		return nullptr;
	}

	TArray<ULaserBeamComponent*> Lasers;
	Enemy->GetComponents(Lasers);

	for (ULaserBeamComponent* Candidate : Lasers)
	{
		if (!LaserSettings || LaserSettings->LaserComponentName.IsNone() || Candidate->GetFName() == LaserSettings->LaserComponentName)
		{
			return Candidate;
		}
	}

	return nullptr;
}

void UGA_BossLaserBeam::FaceAimPoint()
{
	if (!Enemy.IsValid())
	{
		return;
	}

	FVector Direction = AimPoint - Enemy->GetActorLocation();
	Direction.Z = 0.f;
	if (!Direction.IsNearlyZero())
	{
		Enemy->SetActorRotation(Direction.Rotation());
	}
}

void UGA_BossLaserBeam::ShowAimWarning(float Duration)
{
	if (!LaserSettings || !LaserSettings->WarningDecalClass || !IsValid(Laser) || !Enemy.IsValid())
	{
		return;
	}

	if (!IsValid(Warning))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Enemy.Get();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		Warning = GetWorld()->SpawnActor<AAttackWarningDecal>(LaserSettings->WarningDecalClass, AimPoint, FRotator::ZeroRotator, SpawnParams);
	}

	if (IsValid(Warning))
	{
		// Duration 0(추적 중)이면 진행도 0 유지, 확정 후에는 Duration 동안 차오름
		Warning->ShowCircleWarning(AimPoint, Laser->GetBeamRadius() + LaserSettings->WarningRadiusPadding, Duration, LaserSettings->WarningColor);
	}
}

void UGA_BossLaserBeam::HideAimWarning()
{
	if (IsValid(Warning))
	{
		Warning->HideWarning();
	}
	Warning = nullptr;
}

bool UGA_BossLaserBeam::IsExecutionValid() const
{
	return IsActive() && Enemy.IsValid() && Enemy->IsAlive() && PatternComponent.IsValid()
		&& PatternComponent->IsRequestRunning(RequestId) && IsValid(Laser) && LaserSettings;
}

void UGA_BossLaserBeam::EndWithResult(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UGA_BossLaserBeam::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	Stage = EBossLaserBeamStage::None;

	if (Enemy.IsValid())
	{
		Enemy->GetWorldTimerManager().ClearTimer(UpdateTimerHandle);
	}

	// 구독을 먼저 풀어 StopLaser의 OnLaserStopped가 다시 들어오지 않게 함
	if (IsValid(Laser))
	{
		Laser->OnLaserStopped.RemoveDynamic(this, &UGA_BossLaserBeam::HandleLaserStopped);
		Laser->StopLaser();
	}

	HideAimWarning();

	if (Enemy.IsValid())
	{
		if (bOwnsRotationLock) Enemy->ReleaseRotationLock();
		if (bOwnsMovementLock) Enemy->ReleaseAttackMovementLock();
	}
	bOwnsRotationLock = false;
	bOwnsMovementLock = false;

	if (bWasCancelled && Enemy.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("[BossLaser] %s: 취소"), *Enemy->GetName());
	}

	Laser = nullptr;
	LaserSettings = nullptr;
	ExecutionTarget = nullptr;
	RequestId = 0;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
