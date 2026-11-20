#include "Enemy/GAS/Abilities/GA_Enemy_RushAttack.h"

#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

#include "Enemy/Combat/EnemyFeedback.h"
#include "Enemy/Combat/EnemyTargetUtils.h"
#include "Enemy/Components/EnemyMeleeTraceComponent.h"
#include "Enemy/Data/EnemyMeleePatternSettings.h"
#include "Enemy/EnemyCharacter.h"

/* UEnemyPatternAbility / UGA_Enemy_MeleeAttack */

bool UGA_Enemy_RushAttack::CanStartPattern() const
{
	if (!Super::CanStartPattern() || !IsValid(ExecutionTarget)
		|| !FMath::IsFinite(RushSpeed) || RushSpeed <= 0.f || RushHitOffset.ContainsNaN()
		|| !FMath::IsFinite(RushHitRadius) || RushHitRadius <= 0.f)
	{
		return false;
	}

	// 섹션 순서: 시작 섹션 <= Rush < Attack
	const UEnemyMeleePatternSettings* Settings = GetPatternSettings<UEnemyMeleePatternSettings>();
	const UAnimMontage* Montage = Settings->Montage;
	const int32 RushIndex = Montage->GetSectionIndex(RushSection);
	const int32 AttackIndex = Montage->GetSectionIndex(AttackSection);
	const int32 StartIndex = Settings->MontageSection.IsNone() ? 0 : Montage->GetSectionIndex(Settings->MontageSection);

	return RushIndex != INDEX_NONE && AttackIndex > RushIndex
		&& StartIndex != INDEX_NONE && StartIndex <= RushIndex;
}

void UGA_Enemy_RushAttack::OnMontageStarted()
{
	UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance();
	if (!Anim)
	{
		CancelPattern();
		return;
	}

	bRushStarted = false;
	bRushing = false;
	DistanceRemaining = 0.f;

	float RushEndTime = 0.f;
	MeleeSettings->Montage->GetSectionStartAndEndTime(MeleeSettings->Montage->GetSectionIndex(RushSection), RushStartTime, RushEndTime);
	
	// 이동은 어빌리티가 직접 처리하므로 루트 모션으로 이동 거리가 더해지지 않게 함
	PreviousRootMotionMode = Anim->RootMotionMode;
	bOwnsRootMotionOverride = true;
	Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);

	// 돌진은 패턴 설정과 관계없이 방향 고정
	LockRotation();

	// Rush 섹션이 끝나면 Attack 섹션으로 이어지게 연결
	MontageSetNextSectionName(RushSection, AttackSection);

	LastUpdateTime = GetWorld()->GetTimeSeconds();
	// 짧은 반복 간격이라도 한 프레임에 한 번만 갱신 (SweepRushHits가 이동 경로 전체를 쓸어 판정하므로 누락 없음)
	FTimerManagerTimerParameters RushTimerParams;
	RushTimerParams.bLoop = true;
	RushTimerParams.bMaxOncePerFrame = true;
	GetWorld()->GetTimerManager().SetTimer(RushTimer, this, &UGA_Enemy_RushAttack::UpdateRush, RushUpdateInterval, RushTimerParams);
	UpdateRush();
}

void UGA_Enemy_RushAttack::StopPattern(bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RushTimer);
	}

	if (Enemy.IsValid())
	{
		UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance();

		// 돌진 중 종료되면 일시 정지한 몽타주를 풀고 이동 정지
		if (bRushing)
		{
			if (Anim && MeleeSettings && MeleeSettings->Montage)
			{
				Anim->Montage_Resume(MeleeSettings->Montage);
			}
			Enemy->GetCharacterMovement()->StopMovementImmediately();
		}

		if (bOwnsRootMotionOverride && Anim)
		{
			Anim->SetRootMotionMode(PreviousRootMotionMode);
		}
	}

	// 돌진 중 취소(사망, 보스방 초기화)되어도 반복 연출이 남지 않게 정지
	RushTrail.Stop();

	bOwnsRootMotionOverride = false;
	bRushStarted = false;
	bRushing = false;

	// 부모 정리(판정 종료, MeleeSettings 해제)는 마지막에
	Super::StopPattern(bWasCancelled);
}

/* 돌진 */

void UGA_Enemy_RushAttack::UpdateRush()
{
	if (!IsPatternExecutionValid())
	{
		CancelPattern();
		return;
	}

	UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance();
	UAnimMontage* Montage = MeleeSettings ? MeleeSettings->Montage.Get() : nullptr;
	if (!Anim || !Montage || Anim->GetCurrentActiveMontage() != Montage)
	{
		CancelPattern();
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	const float DeltaTime = FMath::Max(0.f, static_cast<float>(Now - LastUpdateTime));
	LastUpdateTime = Now;

	/* 돌진 섹션 시작 대기 */

	if (!bRushStarted)
	{
		if (Anim->Montage_GetPosition(Montage) < RushStartTime)
		{
			return;
		}

		if (!IsValid(ExecutionTarget))
		{
			CancelPattern();
			return;
		}

		// 돌진 거리는 지금 타깃까지의 수평 거리, 방향은 준비 단계에서 정렬한 정면 (재회전하지 않음)
		FVector ToTarget = ExecutionTarget->GetActorLocation() - Enemy->GetActorLocation();
		ToTarget.Z = 0.f;
		DistanceRemaining = ToTarget.Size();
		RushDirection = Enemy->GetActorForwardVector().GetSafeNormal2D();

		bRushStarted = true;
		bRushing = true;

		// 재생 속도만 0으로 두면 평가 시간이 누적되므로 일시 정지로 돌진 자세 유지
		MontageJumpToSection(RushSection);
		Anim->Montage_Pause(Montage);

		// 돌진 시작 연출: 발밑에서 한 번, 돌진하는 동안 몸에 붙은 반복 연출
		EnemyFeedback::PlayAtLocation(Enemy.Get(), RushStartCue, Enemy->GetMesh()->GetComponentLocation(), RushDirection.Rotation());
		RushTrail = EnemyFeedback::PlayAttached(RushTrailCue, Enemy->GetMesh(), RushTrailSocket);

		// 진입 프레임에는 이동하지 않아 준비 시간만큼 거리가 늘어나지 않게 함
		if (DistanceRemaining <= KINDA_SMALL_NUMBER)
		{
			FinishRush();
		}
		return;
	}

	if (!bRushing)
	{
		return;
	}

	/* 이동 */

	const FVector Before = Enemy->GetActorLocation();
	const float Step = FMath::Min(DistanceRemaining, RushSpeed * DeltaTime);

	FHitResult MovementHit;
	Enemy->GetCharacterMovement()->SafeMoveUpdatedComponent(RushDirection * Step, Enemy->GetActorQuat(), true, MovementHit);

	// 이동 중 충돌 콜백으로 종료됐을 수 있음
	if (!IsActive() || !bRushing || !Enemy.IsValid())
	{
		return;
	}

	const FVector After = Enemy->GetActorLocation();
	DistanceRemaining = FMath::Max(0.f, DistanceRemaining - static_cast<float>(FVector::Dist2D(Before, After)));

	/* 이동 경로 전체 판정 */

	const FVector Offset = Enemy->GetActorQuat().RotateVector(RushHitOffset);
	SweepRushHits(Before + Offset, After + Offset);

	if (!IsActive())
	{
		return;
	}

	// 장애물에 막히거나 목표 거리만큼 이동하면 돌진 종료 (벽 통과나 무한 대기 방지)
	if (MovementHit.IsValidBlockingHit() || MovementHit.bStartPenetrating || DistanceRemaining <= KINDA_SMALL_NUMBER)
	{
		// 막혀서 멈춘 경우만 충돌 연출 (목표 거리 도달은 공격 섹션 몽타주 노티파이가 담당)
		if (MovementHit.IsValidBlockingHit())
		{
			EnemyFeedback::PlayAtLocation(Enemy.Get(), RushBlockedCue, MovementHit.ImpactPoint, MovementHit.ImpactNormal.Rotation());
		}

		FinishRush();
	}
}

void UGA_Enemy_RushAttack::SweepRushHits(const FVector& Start, const FVector& End)
{
	// 시작 위치에서 이미 겹친 대상도 포함해 이동 구간 전체를 구 Sweep
	TArray<FHitResult> Hits;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyRushSweep), false, Enemy.Get());
	GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(RushHitRadius), Params);

	// 판정 디버그 표시: 근접 판정 컴포넌트의 디버그 설정을 함께 사용 (주황)
	if (const UEnemyMeleeTraceComponent* Trace = Enemy->GetMeleeTraceComponent(); Trace && Trace->bDrawDebugTraces)
	{
		const FVector SweepDelta = End - Start;
		DrawDebugSphere(GetWorld(), End, RushHitRadius, 20, FColor::Orange, false, 0.f);
		if (!SweepDelta.IsNearlyZero())
		{
			DrawDebugCapsule(GetWorld(), (Start + End) * 0.5f, RushHitRadius + SweepDelta.Size() * 0.5f,
				RushHitRadius, FQuat::FindBetweenNormals(FVector::UpVector, SweepDelta.GetSafeNormal()),
				FColor::Orange, false, 0.f);
		}
	}

	for (const FHitResult& Hit : Hits)
	{
		// 피해 콜백에서 GA가 즉시 취소될 수 있음
		if (!IsActive() || !bRushing)
		{
			return;
		}

		if (EnemyTargetUtils::IsHitReceiverComponent(Hit.GetActor(), Hit.GetComponent()))
		{
			HandleTargetHit(Hit);
		}
	}
}

void UGA_Enemy_RushAttack::FinishRush()
{
	bRushing = false;
	RushTrail.Stop();
	GetWorld()->GetTimerManager().ClearTimer(RushTimer);
	Enemy->GetCharacterMovement()->StopMovementImmediately();

	// 일시정지한 위치에서 재생을 재개하고, 섹션 연결을 따라 공격으로 이어감
	if (UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance(); Anim && MeleeSettings && MeleeSettings->Montage)
	{
		Anim->Montage_Resume(MeleeSettings->Montage);
	}
}
