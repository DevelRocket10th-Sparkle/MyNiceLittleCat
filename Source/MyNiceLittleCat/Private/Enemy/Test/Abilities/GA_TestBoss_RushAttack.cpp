#include "Enemy/Test/Abilities/GA_TestBoss_RushAttack.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/Components/EnemyMeleeTraceComponent.h"
#include "Enemy/GAS/EnemyAttributeSet.h"
#include "Characters/PlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

bool UGA_TestBoss_RushAttack::ValidatePattern() const
{
	const UAnimMontage* Montage = MeleeSettings ? MeleeSettings->Montage.Get() : nullptr;
	if (!Montage || !IsValid(ExecutionTarget)
		|| !FMath::IsFinite(RushSpeed) || RushSpeed <= 0.f || RushHitOffset.ContainsNaN()
		|| !FMath::IsFinite(RushHitRadius) || RushHitRadius <= 0.f) return false;
	const int32 RushIndex = Montage->GetSectionIndex(RushSection);
	const int32 AttackIndex = Montage->GetSectionIndex(AttackSection);
	const int32 StartIndex = MeleeSettings->MontageSection.IsNone() ? 0 : Montage->GetSectionIndex(MeleeSettings->MontageSection);
	return RushIndex != INDEX_NONE && AttackIndex > RushIndex && StartIndex != INDEX_NONE && StartIndex <= RushIndex;
}

void UGA_TestBoss_RushAttack::OnMontageStarted()
{
	UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance();
	if (!Anim) { HandleMontageCancelled(); return; }
	bRushStarted = false;
	bRushing = false;
	DistanceRemaining = 0.f;
	float RushEndTime = 0.f;
	MeleeSettings->Montage->GetSectionStartAndEndTime(MeleeSettings->Montage->GetSectionIndex(RushSection), RushStartTime, RushEndTime);
	// Movement is driven by this ability; authored root motion must not add extra distance.
	PreviousRootMotionMode = Anim->RootMotionMode;
	bOwnsRootMotionOverride = true;
	Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	if (!bOwnsRotationLock)
	{
		Enemy->AcquireRotationLock();
		bOwnsRotationLock = true;
	}
	MontageSetNextSectionName(RushSection, AttackSection);
	LastUpdateTime = GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().SetTimer(RushTimer, this, &UGA_TestBoss_RushAttack::UpdateRush, 0.01f, true);
	UpdateRush();
}

void UGA_TestBoss_RushAttack::UpdateRush()
{
	if (!IsActive() || !Enemy.IsValid() || !Enemy->IsAlive() || !PatternComponent.IsValid()
		|| !PatternComponent->IsRequestRunning(RequestId)) { HandleMontageCancelled(); return; }
	UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance();
	UAnimMontage* Montage = MeleeSettings ? MeleeSettings->Montage.Get() : nullptr;
	if (!Anim || !Montage || Anim->GetCurrentActiveMontage() != Montage) { HandleMontageCancelled(); return; }
	const double Now = GetWorld()->GetTimeSeconds();
	const float DeltaTime = FMath::Max(0.f, static_cast<float>(Now - LastUpdateTime));
	LastUpdateTime = Now;
	if (!bRushStarted)
	{
		if (Anim->Montage_GetPosition(Montage) < RushStartTime) return;
		if (!IsValid(ExecutionTarget)) { HandleMontageCancelled(); return; }
		// Snapshot both direction and horizontal distance from the same target position.
		// Moving the target afterwards must not change this dash's destination.
		FVector ToTarget = ExecutionTarget->GetActorLocation() - Enemy->GetActorLocation();
		ToTarget.Z = 0.f;
		if (ToTarget.ContainsNaN()) { HandleMontageCancelled(); return; }
		DistanceRemaining = ToTarget.Size();
		RushDirection = ToTarget.GetSafeNormal();
		if (RushDirection.IsNearlyZero()) RushDirection = Enemy->GetActorForwardVector().GetSafeNormal2D();
		Enemy->SetActorRotation(RushDirection.Rotation());
		bRushStarted = bRushing = true;
		MontageJumpToSection(RushSection);
		GetAbilitySystemComponentFromActorInfo()->CurrentMontageSetPlayRate(0.f);
		// No movement on the entry tick: the Ready interval must not contribute to dash distance.
		if (DistanceRemaining <= KINDA_SMALL_NUMBER) FinishRush();
		return;
	}
	if (!bRushing) return;
	const FVector Before = Enemy->GetActorLocation();
	const FVector Offset = Enemy->GetActorQuat().RotateVector(RushHitOffset);
	const float Step = FMath::Min(DistanceRemaining, RushSpeed * DeltaTime);
	FHitResult MovementHit;
	Enemy->GetCharacterMovement()->SafeMoveUpdatedComponent(RushDirection * Step, Enemy->GetActorQuat(), true, MovementHit);
	if (!IsActive() || !bRushing || !Enemy.IsValid()) return;
	const FVector After = Enemy->GetActorLocation();
	DistanceRemaining = FMath::Max(0.f, DistanceRemaining - FVector::Dist2D(Before, After));
	// Sweep the actual travelled path, including initial overlaps, rather than only the end position.
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TestBossRush), false, Enemy.Get());
	GetWorld()->SweepMultiByObjectType(Hits, Before + Offset, After + Offset, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(RushHitRadius), Params);
	if (const UEnemyMeleeTraceComponent* Trace = Enemy->GetMeleeTraceComponent(); Trace && Trace->bDrawDebugTraces)
	{
		const FVector Start = Before + Offset;
		const FVector End = After + Offset;
		const FVector SweepDelta = End - Start;
		// A sphere swept along a straight line covers a capsule-shaped volume.
		DrawDebugSphere(GetWorld(), End, RushHitRadius, 20, FColor::Orange, false, 0.f);
		if (!SweepDelta.IsNearlyZero())
			DrawDebugCapsule(GetWorld(), (Start + End) * 0.5f, RushHitRadius + SweepDelta.Size() * 0.5f,
				RushHitRadius, FQuat::FindBetweenNormals(FVector::UpVector, SweepDelta.GetSafeNormal()),
				FColor::Orange, false, 0.f);
		DrawDebugString(GetWorld(), End + FVector(0.f, 0.f, RushHitRadius + 8.f),
			FString::Printf(TEXT("Rush Hit (%.1f cm)"), RushHitRadius), nullptr, FColor::Orange, 0.f, true);
	}
	for (const FHitResult& Hit : Hits)
	{
		if (!IsActive() || !bRushing) return; // Damage callbacks may cancel this ability synchronously.
		APlayerCharacter* Player = Cast<APlayerCharacter>(Hit.GetActor());
		if (Player && Hit.GetComponent() == Player->GetCapsuleComponent()) HandlePlayerHit(Hit);
	}
	if (!IsActive()) return;
	// Obstacles end the dash early. Do not teleport through walls or wait indefinitely.
	if (MovementHit.IsValidBlockingHit() || MovementHit.bStartPenetrating || DistanceRemaining <= KINDA_SMALL_NUMBER) FinishRush();
}

void UGA_TestBoss_RushAttack::FinishRush()
{
	bRushing = false;
	GetWorld()->GetTimerManager().ClearTimer(RushTimer);
	Enemy->GetCharacterMovement()->StopMovementImmediately();
	// Resume from Rush's first frame; the authored Rush -> Attack link handles the rest.
	GetAbilitySystemComponentFromActorInfo()->CurrentMontageSetPlayRate(1.f);
}

void UGA_TestBoss_RushAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RushTimer);
	if (Enemy.IsValid())
	{
		if (bRushing)
		{
			if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo()) ASC->CurrentMontageSetPlayRate(1.f);
			Enemy->GetCharacterMovement()->StopMovementImmediately();
		}
		if (bOwnsRootMotionOverride)
			if (UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance()) Anim->SetRootMotionMode(PreviousRootMotionMode);
	}
	bOwnsRootMotionOverride = false;
	bRushStarted = bRushing = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
