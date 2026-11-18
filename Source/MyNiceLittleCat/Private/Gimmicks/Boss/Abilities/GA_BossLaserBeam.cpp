#include "Gimmicks/Boss/Abilities/GA_BossLaserBeam.h"
#include "Gimmicks/Boss/Abilities/BossLaserPatternSettings.h"
#include "Gimmicks/Boss/BossGimmickComponent.h"
#include "Enemy/GAS/EnemyAttributeSet.h"
#include "Gimmicks/Laser/LaserBeamComponent.h"
#include "Gimmicks/Warning/AttackWarningDecal.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Enemy/GAS/EnemyCombatTags.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

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
	// 이동 속도와 회전을 제어하므로 몸 점유: 다른 몸 사용 패턴과 동시에 실행되지 않음
	DefaultPatternResources.AddTag(EnemyCombatTags::PatternResourceBody);
}

// 요청 수신, Enemy 생존 확인, CommitAbility는 부모 ActivateAbility가 처리 → 여기서는 실행 조건만 검사
bool UGA_BossLaserBeam::CanStartPattern() const
{
	if (!GetPatternSettings<UBossLaserPatternSettings>() || !FindLaserComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossLaser] %s: 패턴 %s에 BossLaserPatternSettings 또는 보스의 LaserBeamComponent가 없습니다."),
			*Enemy->GetName(), *ExecutionPattern.PatternId.ToString());
		return false;
	}

	// 바닥을 겨누는 패턴이라 타깃이 꼭 필요
	// const 검사라 AimPoint 대신 지역 변수에 받음 (실제 지점은 StartPattern에서 다시 구함)
	FVector FloorPoint;
	return IsValid(ExecutionTarget) && FindTargetFloorPoint(FloorPoint);
}

bool UGA_BossLaserBeam::StartPattern()
{
	LaserSettings = GetPatternSettings<UBossLaserPatternSettings>();
	Laser = FindLaserComponent();

	// 기존 검사 단계에서 채우던 공격 지점: 아래 FaceAimPoint(bFaceTargetOnStart)가 사용
	FindTargetFloorPoint(AimPoint);

	ShotsFired = 0;

	// 근접 패턴과 같은 공식으로 피해량 적용 (이 패턴 동안만)
	if (LaserSettings->bUseBossAttackPower && Enemy->GetAttributeSet())
	{
		OriginalDamagePerHit = Laser->GetDamagePerHit();
		bOverrodeDamage = true;
		Laser->SetDamagePerHit(Enemy->GetAttributeSet()->GetAttackPower() * ExecutionPattern.DamageMultiplier);
		UE_LOG(LogTemp, Log, TEXT("[BossLaser] %s: 피해 %.1f (공격력 %.1f × %.2f)"), *Enemy->GetName(), Laser->GetDamagePerHit(),
			Enemy->GetAttributeSet()->GetAttackPower(), ExecutionPattern.DamageMultiplier);
	}

	// FollowLocomotion이면 이동은 BT(페이즈 기본 이동)에 맡기고, 그 외 (Stop 등)일 때만 제자리 고정
	const bool bFollowPhaseMovement = ExecutionPattern.Movement.Policy == EEnemyPatternMovementPolicy::FollowLocomotion;
	if (!bFollowPhaseMovement)
	{
		// 보스를 지점으로 옮기지 않고 지금 자리에서 쏨 (이동은 보스 쪽 이동 패턴 담당)
		StopOwnerMovement();

		// 이동/회전 잠금 해제는 부모(UEnemyPatternAbility)가 EndAbility에서 처리
		LockMovement();
	}

	// 페이즈 내에서 패턴 이동 속도 오버라이드가 있는 경우, 레이저 활성 기간 동안 적용
	LaserMoveSpeedMultiplier = 1.f;
	if (bFollowPhaseMovement)
	{
		const ABossCharacter* Boss = Cast<ABossCharacter>(Enemy.Get());
		const FBossPhaseDefinition* CurrentPhase = Boss ? Boss->GetCurrentPhaseDefinition() : nullptr;
		if (CurrentPhase)
		{
			LaserMoveSpeedMultiplier = CurrentPhase->ResolveActiveMoveSpeedMultiplier(ExecutionPattern.PatternId);
		}
	}

	// 회전 잠금은 이동 정책과 별개로 패턴 설정을 따름
	if (ExecutionPattern.bLockRotation)
	{
		LockRotation();
	}

	if (ExecutionPattern.bFaceTargetOnStart)
	{
		FaceAimPoint();
	}

	Enemy->GetWorldTimerManager().SetTimer(UpdateTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGA_BossLaserBeam::UpdateAim), AimUpdateInterval, true);

	StartShot();
	return true;
}

void UGA_BossLaserBeam::StartShot()
{
	if (!IsExecutionValid())
	{
		EndWithResult(true);
		return;
	}

	FindTargetFloorPoint(AimPoint);

	// 보조·상시 실행이거나 몽타주가 없으면 nullptr
	if (UAbilityTask_PlayMontageAndWait* MontageTask = CreatePatternMontageTask(LaserSettings->ChargeMontage))
	{
		MontageTask->ReadyForActivation();
	}

	// 1단계: 추적
	Stage = EBossLaserBeamStage::Tracking;
	StartChargeEffect();
	ShowAimWarning(0.f);

	UE_LOG(LogTemp, Log, TEXT("[BossLaser] %s: %d/%d발 추적 시작 (%.2f초)"), *Enemy->GetName(), ShotsFired + 1, LaserSettings->ShotCount, LaserSettings->AimTrackTime);

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
	StopChargeEffect();
	HideAimWarning();

	if (!FMath::IsNearlyEqual(LaserMoveSpeedMultiplier, 1.f))
	{
		Enemy->SetPatternMoveSpeedMultiplier(LaserMoveSpeedMultiplier);
		bOverrodeMoveSpeed = true;
	}

	Laser->OnLaserStopped.AddUniqueDynamic(this, &UGA_BossLaserBeam::HandleLaserStopped);
	Laser->StartLaser(GetBeamTargetLocation(Laser, AimPoint), LaserSettings->FireDuration);
	SetStaffEyeGlow(Laser->IsLaserActive());

	UE_LOG(LogTemp, Log, TEXT("[BossLaser] %s: 발사 (%.2f초, 발사 중 추적 %s)"),
		*Enemy->GetName(), LaserSettings->FireDuration, LaserSettings->bTrackDuringFire ? TEXT("켬") : TEXT("끔"));
}

void UGA_BossLaserBeam::HandleLaserStopped(ULaserBeamComponent* InLaser)
{
	SetStaffEyeGlow(false);
	if (IsValid(Laser))
	{
		Laser->OnLaserStopped.RemoveDynamic(this, &UGA_BossLaserBeam::HandleLaserStopped);
	}
	Stage = EBossLaserBeamStage::None;

	RestoreMoveSpeed();	// 레이저 이동 속도 되돌리기

	if (!IsExecutionValid())
	{
		EndWithResult(true);
		return;
	}

	++ShotsFired;
	UE_LOG(LogTemp, Log, TEXT("[BossLaser] %d/%d발 발사 종료"), ShotsFired, LaserSettings->ShotCount);

	if (ShotsFired >= LaserSettings->ShotCount)
	{
		if (UBossGimmickComponent* Gimmick = Enemy->FindComponentByClass<UBossGimmickComponent>())
		{
			Gimmick->CompleteLaserTurn(EBossLaserTurn::Beam);
		}
		EndWithResult(false);
		return;
	}

	if (LaserSettings->ShotInterval > 0.f)
	{
		UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, LaserSettings->ShotInterval);
		WaitTask->OnFinish.AddDynamic(this, &UGA_BossLaserBeam::StartShot);
		WaitTask->ReadyForActivation();
	}
	else
	{
		StartShot();
	}
}

bool UGA_BossLaserBeam::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UBossGimmickComponent* Gimmick = Avatar ? Avatar->FindComponentByClass<UBossGimmickComponent>() : nullptr;
	return !Gimmick || Gimmick->IsLaserTurnAllowed(EBossLaserTurn::Beam);
}

void UGA_BossLaserBeam::UpdateAim()
{
	if (!IsActive() || !IsExecutionValid())
	{
		return;
	}

	if (IsValid(ChargeEffectComponent))
	{
		const float Duration = LaserSettings->AimTrackTime + LaserSettings->LockDelay;
		const float Progress = Duration > 0.f
			? FMath::Clamp(static_cast<float>(GetWorld()->GetTimeSeconds() - ChargeStartTime) / Duration, 0.f, 1.f)
			: 1.f;
		ChargeEffectComponent->SetVariableFloat(TEXT("User.ChargeProgress"), Progress);
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

	// 트리거·카메라 마스킹 볼륨처럼 막힘 응답이 하나도 없는 감지 전용 박스는 바닥으로 치지 않음
	auto IsDetectionOnly = [](const UPrimitiveComponent* Component)
	{
		if (!Component)
		{
			return false;
		}
		for (int32 Channel = 0; Channel < ECC_MAX; ++Channel)
		{
			if (Component->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(Channel)) == ECR_Block)
			{
				return false;
			}
		}
		return true;
	};

	for (int32 Attempt = 0; Attempt < 16; ++Attempt)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, TargetLocation, TargetLocation - FVector(0.f, 0.f, FloorTraceDistance), ObjectParams, QueryParams))
		{
			break;
		}
		if (IsDetectionOnly(Hit.GetComponent()))
		{
			QueryParams.AddIgnoredComponent(Hit.GetComponent());
			continue;
		}
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

	// StartPattern 전(LaserSettings가 비어 있을 때)에도 LaserComponentName 지정이 적용되도록 부모가 받은 설정 사용
	const UBossLaserPatternSettings* Settings = GetPatternSettings<UBossLaserPatternSettings>();

	TArray<ULaserBeamComponent*> Lasers;
	Enemy->GetComponents(Lasers);

	for (ULaserBeamComponent* Candidate : Lasers)
	{
		if (!Settings || Settings->LaserComponentName.IsNone() || Candidate->GetFName() == Settings->LaserComponentName)
		{
			return Candidate;
		}
	}

	return nullptr;
}

void UGA_BossLaserBeam::FaceAimPoint()
{
	// 보조·상시 실행에서는 도우미가 회전을 생략
	FaceOwnerToward(AimPoint);
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

void UGA_BossLaserBeam::StartChargeEffect()
{
	StopChargeEffect();
	if (!LaserSettings->ChargeEffect || GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	ChargeStartTime = GetWorld()->GetTimeSeconds();
	ChargeEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		LaserSettings->ChargeEffect, Laser, NAME_None, LaserSettings->ChargeEffectOffset,
		FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, false, false);
	if (IsValid(ChargeEffectComponent))
	{
		ChargeEffectComponent->SetRelativeScale3D(FVector(LaserSettings->ChargeEffectScale));
		ChargeEffectComponent->SetVariableFloat(TEXT("User.ChargeProgress"), 0.f);
		ChargeEffectComponent->Activate(true);
	}
}

void UGA_BossLaserBeam::StopChargeEffect()
{
	if (IsValid(ChargeEffectComponent))
	{
		ChargeEffectComponent->DestroyComponent();
	}
	ChargeEffectComponent = nullptr;
}

void UGA_BossLaserBeam::SetStaffEyeGlow(bool bEnabled)
{
	if (bEnabled && !StaffEyeMaterial && Enemy.IsValid())
	{
		const TArray<UActorComponent*> Meshes = Enemy->GetComponentsByTag(UMeshComponent::StaticClass(), TEXT("LaserEyeGlow"));
		if (!Meshes.IsEmpty())
		{
			StaffEyeMaterial = CastChecked<UMeshComponent>(Meshes[0])->CreateDynamicMaterialInstance(0);
		}
	}
	if (StaffEyeMaterial)
	{
		StaffEyeMaterial->SetVectorParameterValue(TEXT("EyeGlow"),
			bEnabled && LaserSettings ? LaserSettings->StaffEyeGlowColor : FLinearColor::Black);
	}
}

bool UGA_BossLaserBeam::IsExecutionValid() const
{
	// 활성·생존·요청 진행 확인은 부모 도우미, 레이저 고유 조건만 추가
	return IsPatternExecutionValid() && IsValid(Laser) && LaserSettings;
}

void UGA_BossLaserBeam::EndWithResult(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UGA_BossLaserBeam::RestoreMoveSpeed()
{
	if (bOverrodeMoveSpeed && Enemy.IsValid())
	{
		Enemy->ClearPatternMoveSpeedMultiplier();
	}
	bOverrodeMoveSpeed = false;
}

// 부모 EndAbility가 먼저 호출 → 이 정리 뒤에 부모가 이동/회전 잠금 해제, ExecutionTarget/RequestId 등 초기화, Super::EndAbility 진행
void UGA_BossLaserBeam::StopPattern(bool bWasCancelled)
{
	SetStaffEyeGlow(false);
	Stage = EBossLaserBeamStage::None;
	StopChargeEffect();

	if (Enemy.IsValid())
	{
		Enemy->GetWorldTimerManager().ClearTimer(UpdateTimerHandle);
	}

	// 구독을 먼저 풀어 StopLaser의 OnLaserStopped가 다시 들어오지 않게 함
	if (IsValid(Laser))
	{
		Laser->OnLaserStopped.RemoveDynamic(this, &UGA_BossLaserBeam::HandleLaserStopped);
		Laser->StopLaser();

		if (bOverrodeDamage)
		{
			Laser->SetDamagePerHit(OriginalDamagePerHit);
		}
	}
	bOverrodeDamage = false;

	RestoreMoveSpeed();	// 레이저 이동 속도 되돌리기

	HideAimWarning();

	if (bWasCancelled && IsActive() && Enemy.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("[BossLaser] %s: 취소"), *Enemy->GetName());
	}

	Laser = nullptr;
	LaserSettings = nullptr;
	StaffEyeMaterial = nullptr;
	LaserMoveSpeedMultiplier = 1.f;
	ShotsFired = 0;
}
