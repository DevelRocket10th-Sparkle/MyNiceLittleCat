#include "Enemy/GAS/Abilities/GA_Enemy_RushAttack.h"
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

bool UGA_Enemy_RushAttack::ValidatePattern() const
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

void UGA_Enemy_RushAttack::OnMontageStarted()
{
	UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance();
	if (!Anim)
	{
		HandleMontageCancelled();
		return;
	}
	bRushStarted = false;
	bRushing = false;
	DistanceRemaining = 0.f;
	float RushEndTime = 0.f;
	MeleeSettings->Montage->GetSectionStartAndEndTime(MeleeSettings->Montage->GetSectionIndex(RushSection), RushStartTime, RushEndTime);
	// 이동은 어빌리티가 직접 처리하므로 루트 모션으로 이동 거리가 추가되지 않게 한다.
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
	GetWorld()->GetTimerManager().SetTimer(RushTimer, this, &UGA_Enemy_RushAttack::UpdateRush, 0.01f, true);
	UpdateRush();
}

void UGA_Enemy_RushAttack::UpdateRush()
{
	if (!IsActive() || !Enemy.IsValid() || !Enemy->IsAlive() || !PatternComponent.IsValid()
		|| !PatternComponent->IsRequestRunning(RequestId)) { HandleMontageCancelled(); return; }
	UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance();
	UAnimMontage* Montage = MeleeSettings ? MeleeSettings->Montage.Get() : nullptr;
	if (!Anim || !Montage || Anim->GetCurrentActiveMontage() != Montage)
	{
		HandleMontageCancelled();
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const float DeltaTime = FMath::Max(0.f, static_cast<float>(Now - LastUpdateTime));
	LastUpdateTime = Now;
	if (!bRushStarted)
	{
		if (Anim->Montage_GetPosition(Montage) < RushStartTime)
		{
			return;
		}
		if (!IsValid(ExecutionTarget))
		{
			HandleMontageCancelled();
			return;
		}
		// 같은 타깃 위치를 기준으로 돌진 방향과 수평 거리를 한 번만 저장한다.
		// 돌진 시작 후 타깃이 움직여도 목적지를 바꾸지 않는다.
		FVector ToTarget = ExecutionTarget->GetActorLocation() - Enemy->GetActorLocation();
		ToTarget.Z = 0.f;
		if (ToTarget.ContainsNaN())
		{
			HandleMontageCancelled();
			return;
		}

		// 돌진 거리는 현재 타깃까지의 수평 거리로 계산
		DistanceRemaining = ToTarget.Size();

		// 방향은 준비 단계에서 정렬한 캐릭터의 전방 사용
		// 돌진 시작 순간 타깃 방향으로 재회전하지 않음
		RushDirection = Enemy->GetActorForwardVector().GetSafeNormal2D();

		bRushStarted = bRushing = true;
		MontageJumpToSection(RushSection);
		GetAbilitySystemComponentFromActorInfo()->CurrentMontageSetPlayRate(0.f);
		// 돌진 진입 프레임에는 이동하지 않아 준비 시간만큼 이동 거리가 늘어나지 않게 한다.
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
	const FVector Before = Enemy->GetActorLocation();
	const FVector Offset = Enemy->GetActorQuat().RotateVector(RushHitOffset);
	const float Step = FMath::Min(DistanceRemaining, RushSpeed * DeltaTime);
	FHitResult MovementHit;
	Enemy->GetCharacterMovement()->SafeMoveUpdatedComponent(RushDirection * Step, Enemy->GetActorQuat(), true, MovementHit);
	if (!IsActive() || !bRushing || !Enemy.IsValid())
	{
		return;
	}
	const FVector After = Enemy->GetActorLocation();
	DistanceRemaining = FMath::Max(0.f, DistanceRemaining - FVector::Dist2D(Before, After));
	// 시작 위치의 겹침을 포함해 실제 이동 경로 전체에서 타격을 검사한다.
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TestBossRush), false, Enemy.Get());
	GetWorld()->SweepMultiByObjectType(Hits, Before + Offset, After + Offset, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(RushHitRadius), Params);
	if (const UEnemyMeleeTraceComponent* Trace = Enemy->GetMeleeTraceComponent(); Trace && Trace->bDrawDebugTraces)
	{
		const FVector Start = Before + Offset;
		const FVector End = After + Offset;
		const FVector SweepDelta = End - Start;
		// 구체를 직선으로 이동하며 검사한 영역은 캡슐 모양이다.
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
		// 피해 콜백에서 어빌리티가 즉시 취소될 수 있다.
		if (!IsActive() || !bRushing)
		{
			return;
		}
		APlayerCharacter* Player = Cast<APlayerCharacter>(Hit.GetActor());
		if (Player && Hit.GetComponent() == Player->GetCapsuleComponent())
		{
			HandlePlayerHit(Hit);
		}
	}
	if (!IsActive())
	{
		return;
	}
	// 장애물에 막히면 즉시 돌진을 끝내 벽 통과나 무한 대기를 막는다.
	if (MovementHit.IsValidBlockingHit() || MovementHit.bStartPenetrating || DistanceRemaining <= KINDA_SMALL_NUMBER)
	{
		FinishRush();
	}
}

void UGA_Enemy_RushAttack::FinishRush()
{
	bRushing = false;
	GetWorld()->GetTimerManager().ClearTimer(RushTimer);
	Enemy->GetCharacterMovement()->StopMovementImmediately();
	// 돌진의 첫 프레임부터 재생을 재개하고, 설정된 섹션 연결에 따라 공격으로 이어간다.
	GetAbilitySystemComponentFromActorInfo()->CurrentMontageSetPlayRate(1.f);
}

void UGA_Enemy_RushAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RushTimer);
	}
	if (Enemy.IsValid())
	{
		if (bRushing)
		{
			if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
			{
				ASC->CurrentMontageSetPlayRate(1.f);
			}
			Enemy->GetCharacterMovement()->StopMovementImmediately();
		}
		if (bOwnsRootMotionOverride)
		{
			if (UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance())
			{
				Anim->SetRootMotionMode(PreviousRootMotionMode);
			}
		}
	}
	bOwnsRootMotionOverride = false;
	bRushStarted = bRushing = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
