// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/EnemyLog.h"
#include "EnemyPatternOccupancy.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

#include "Enemy/Combat/EnemyTargetUtils.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/GAS/EnemyCombatTags.h"

UEnemyPatternComponent::UEnemyPatternComponent()
{
	// GA 종료 이벤트와 타이머로 동작하므로 Tick 불필요
	PrimaryComponentTick.bCanEverTick = false;
}

/* 초기화와 설정 */

// 이전 실행 정리 후 패턴 설정 검사, 패턴마다 GA Spec 부여, 유효 자원 계산
bool UEnemyPatternComponent::InitializePatterns(UAbilitySystemComponent* InASC, UEnemyPatternSetData* InPatternSet)
{
	StopAndInvalidate();
	ClearGrantedAbilities();

	ASC = InASC;
	Catalog.SetPatternSet(InPatternSet);

	if (!GetOwner()->HasAuthority() || !IsValid(InASC) || !IsValid(InPatternSet) || InPatternSet->Patterns.IsEmpty())
	{
		return false;
	}

	// 설정 검사, 패턴마다 GA Spec 부여, 유효 자원 계산, 기본 선택(모든 패턴 허용)
	if (!Catalog.GrantAbilities(*InASC, this, GetOwner()))
	{
		return false;
	}

	// 종료 이벤트 구독: 모든 패턴 GA를 한 번에 받아 Spec Handle로 슬롯을 찾음
	AbilityEndedDelegateHandle = InASC->OnAbilityEnded.AddUObject(this, &UEnemyPatternComponent::HandleAbilityEnded);

	// 기본 동시 실행: 한 번에 하나
	Concurrency = FEnemyPatternConcurrency();

	bStopped = false;
	bRequestsBlocked = false;

	return true;
}

// 사용할 패턴, 가중치 재정의, 동시 실행 설정을 한 번에 교체
void UEnemyPatternComponent::SetPatternSelection(const TArray<FEnemyPatternSelectionEntry>& Entries, const FEnemyPatternConcurrency& InConcurrency)
{
	// 정지 상태면 모든 패턴을 허용하지 않음
	Catalog.SetSelection(bStopped ? TArray<FEnemyPatternSelectionEntry>() : Entries);

	FString Error;
	Concurrency = InConcurrency.Validate(Error) ? InConcurrency : FEnemyPatternConcurrency();
}

bool UEnemyPatternComponent::ConfigureAttackInterval(float InMinInterval, float InMaxInterval)
{
	return IsIdle() && Pacing.Configure(InMinInterval, InMaxInterval);
}

void UEnemyPatternComponent::ResetAttackInterval()
{
	Pacing.Reset();
}

/* 실행 조건 조회 */

bool UEnemyPatternComponent::CanStartAttack() const
{
	const AEnemyCharacter* Enemy = GetEnemy();

	// 주 패턴은 하나씩: BT가 기다리는 요청이 없고, 몸을 쓰는 GA가 실제로 끝났고, 빈 자리가 있어야 함
	return IsValid(Enemy) && Enemy->HasAuthority() && Enemy->CanRunEnemyAI()
		&& !bStopped && !bRequestsBlocked && ASC.IsValid() && HasPatterns()
		&& PrimaryRequestId == 0 && !IsBodyBusy()
		&& GetRunningSlotCount() < Concurrency.MaxConcurrentPatterns
		&& GetAttackIntervalRemaining() <= 0.f;
}

float UEnemyPatternComponent::GetAttackIntervalRemaining() const
{
	const UWorld* World = GetWorld();
	return World ? Pacing.GetRemaining(World->GetTimeSeconds()) : 0.f;
}

// 쿨다운과 시야를 제외한 현재 패턴의 타깃 및 거리 조건 검사
bool UEnemyPatternComponent::HasPatternInRange(AActor* Target) const
{
	if (bStopped)
	{
		return false;
	}

	return Catalog.GetPatterns().ContainsByPredicate(
		[this, Target](const FEnemyPatternDefinition& Pattern)
		{
			return MatchesGeometry(Pattern, Target);
		});
}

bool UEnemyPatternComponent::HasExecutableAttack(AActor* Target) const
{
	if (!CanStartAttack())
	{
		return false;
	}

	TArray<const FEnemyPatternDefinition*> Candidates;
	CollectCandidates(Target, false, {}, Candidates);
	return !Candidates.IsEmpty();
}

bool UEnemyPatternComponent::CanStartPattern(FName PatternId, AActor* Target) const
{
	if (!CanStartAttack())
	{
		return false;
	}

	const FEnemyPatternDefinition* Pattern = Catalog.Find(PatternId);
	return Pattern && CanJoinNow(*Pattern) && CanExecutePattern(*Pattern, Target);
}

bool UEnemyPatternComponent::PickAttackPattern(AActor* Target, FName& OutPatternId) const
{
	OutPatternId = NAME_None;

	if (!CanStartAttack())
	{
		return false;
	}

	// 주 패턴 후보: 실행 중인 보조 패턴과 남은 생성물의 자원·위험도까지 반영
	TArray<const FEnemyPatternDefinition*> Candidates;
	CollectCandidates(Target, false, {}, Candidates);

	const FEnemyPatternDefinition* Selected = EnemyPatternOccupancy::PickWeighted(Candidates, Catalog);
	if (!Selected)
	{
		return false;
	}

	OutPatternId = Selected->PatternId;
	return true;
}

/* 실행 요청 */

// 지정한 패턴을 주 패턴으로 실행하고, 이어서 빈 자리에 보조 패턴 추첨
bool UEnemyPatternComponent::TryStartPattern(FName PatternId, AActor* Target, int64& OutRequestId)
{
	OutRequestId = 0;

	PruneLingeringHazards();

	// 직접 지정은 가중치를 보지 않음: 가중치 0 = 자동 선택에서만 제외
	if (!CanStartPattern(PatternId, Target))
	{
		return false;
	}

	/* 1) 주 패턴 슬롯 생성과 GA 활성화 */

	// 실행 설정으로 인계했으므로 사전 선택 기록은 비움
	ReservedPatternId = NAME_None;

	const int64 RequestId = AddSlot(*Catalog.Find(PatternId), Target, true).RequestId;
	PrimaryRequestId = RequestId;

	if (!ActivateSlot(RequestId))
	{
		return false;
	}

	// 활성화 중 사망 또는 초기화로 실행이 무효화됐으면 요청을 반환하지 않음
	if (bStopped || PrimaryRequestId != RequestId)
	{
		return false;
	}

	OutRequestId = RequestId;

	/* 2) 보조 패턴: 주 패턴의 자원이 점유된 뒤에 추첨해야 겹치지 않음 */

	FillCompanions(Target);

	return true;
}

// 요청 번호가 일치하는 실행만 취소
void UEnemyPatternComponent::CancelRequest(int64 RequestId)
{
	FEnemyPatternSlot* Slot = FindSlot(RequestId);
	if (!Slot || Slot->Result != EEnemyPatternResult::Running)
	{
		return;
	}

	const bool bStarted = Slot->bStarted;
	const FGameplayAbilitySpecHandle AbilityHandle = Slot->AbilityHandle;

	if (Slot->bPrimary)
	{
		ReleaseRecoveryMovementLock();
	}

	// 결과를 먼저 기록: GA 취소로 들어오는 종료 이벤트가 이 슬롯을 다시 처리하지 않음
	FinishSlot(RequestId, EEnemyPatternResult::Cancelled);

	// 시작 지연 예약은 GA가 없으므로 슬롯만 정리
	if (bStarted && ASC.IsValid())
	{
		ASC->CancelAbilityHandle(AbilityHandle);
	}
}

// 주 패턴, 보조 패턴, 시작 지연 예약, 상시 패턴을 모두 취소 (남은 생성물은 각 GA의 정리 규칙을 따름)
void UEnemyPatternComponent::CancelAllRequests()
{
	// 상시 패턴은 재시작 예약까지 먼저 해제: 아래 취소로 들어오는 종료가 다시 시작하지 않도록
	StopPersistentPatterns();

	TArray<int64, TInlineAllocator<4>> RunningIds;
	for (const FEnemyPatternSlot& Slot : Slots)
	{
		if (Slot.Result == EEnemyPatternResult::Running)
		{
			RunningIds.Add(Slot.RequestId);
		}
	}

	for (const int64 RequestId : RunningIds)
	{
		CancelRequest(RequestId);
	}
}

// 주 패턴 요청 해제: 실행 중이면 먼저 취소하고 결과와 타깃 해제
void UEnemyPatternComponent::ReleaseRequest(int64 RequestId)
{
	if (RequestId == 0 || RequestId != PrimaryRequestId)
	{
		return;
	}

	CancelRequest(RequestId);
	RemoveSlot(RequestId);
	PrimaryRequestId = 0;
}

// 모든 요청을 무효화한 뒤 타이머, 남은 생성물 기록, 능력 실행 정리
void UEnemyPatternComponent::StopAndInvalidate()
{
	StopPersistentPatterns();
	ReleaseRecoveryMovementLock();
	ResetAttackInterval();

	bStopped = true;
	bRequestsBlocked = true;

	for (FEnemyPatternSlot& Slot : Slots)
	{
		ClearSlotTimers(Slot);
	}

	Slots.Reset();
	LingeringHazards.Reset();
	PrimaryRequestId = 0;
	ReservedPatternId = NAME_None;

	// 슬롯을 먼저 비웠으므로 취소로 들어오는 종료 이벤트는 무시됨
	if (ASC.IsValid())
	{
		for (const TPair<FName, FGameplayAbilitySpecHandle>& Pair : Catalog.GetHandles())
		{
			ASC->CancelAbilityHandle(Pair.Value);
		}
	}
}

/* 요청 상태 조회 */

EEnemyPatternResult UEnemyPatternComponent::GetRequestResult(int64 RequestId) const
{
	const FEnemyPatternSlot* Slot = bStopped ? nullptr : FindSlot(RequestId);
	return Slot ? Slot->Result : EEnemyPatternResult::Invalid;
}

bool UEnemyPatternComponent::IsRequestRunning(int64 RequestId) const
{
	return GetRequestResult(RequestId) == EEnemyPatternResult::Running;
}

bool UEnemyPatternComponent::IsCompanionRequest(int64 RequestId) const
{
	const FEnemyPatternSlot* Slot = FindSlot(RequestId);
	return Slot && !Slot->bPrimary;
}

bool UEnemyPatternComponent::GetExecutionForAbility(const FGameplayAbilitySpecHandle& AbilityHandle,
	FEnemyPatternDefinition& OutPattern, AActor*& OutTarget, int64& OutRequestId) const
{
	OutPattern = FEnemyPatternDefinition();
	OutTarget = nullptr;
	OutRequestId = 0;

	if (bStopped)
	{
		return false;
	}

	const FEnemyPatternSlot* Slot = FindActiveSlotForAbility(AbilityHandle);

	if (!Slot)
	{
		return false;
	}

	OutPattern = Slot->Pattern;
	OutTarget = Slot->Target.Get();
	OutRequestId = Slot->RequestId;
	return true;
}

bool UEnemyPatternComponent::GetActiveMovement(FEnemyPatternMovementSettings& OutMovement, AActor*& OutTarget, int64& OutRequestId) const
{
	OutMovement = FEnemyPatternMovementSettings();
	OutTarget = nullptr;
	OutRequestId = 0;

	const FEnemyPatternSlot* Primary = IsRequestRunning(PrimaryRequestId) ? FindSlot(PrimaryRequestId) : nullptr;
	if (!Primary)
	{
		return false;
	}

	// 후딜 중에는 기본값(FollowLocomotion) 유지: 실제 정지는 후딜 이동 잠금이 담당
	if (!Primary->bRecovering)
	{
		OutMovement = Primary->Pattern.Movement;
	}

	OutTarget = Primary->Target.Get();
	OutRequestId = Primary->RequestId;
	return true;
}

// 관리 중인 GA와 예약이 모두 끝났는지
bool UEnemyPatternComponent::IsIdle() const
{
	// 상시 패턴을 포함한 모든 실행 중 슬롯
	const bool bAnyRunning = Slots.ContainsByPredicate(
		[](const FEnemyPatternSlot& Slot)
		{
			return Slot.Result == EEnemyPatternResult::Running;
		});

	if (bAnyRunning)
	{
		return false;
	}

	// 취소 요청을 했어도 실제 GA가 종료되지 않았으면 Idle이 아님
	if (ASC.IsValid())
	{
		for (const TPair<FName, FGameplayAbilitySpecHandle>& Pair : Catalog.GetHandles())
		{
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Pair.Value);
			if (Spec && Spec->IsActive())
			{
				return false;
			}
		}
	}

	return true;
}

// 몸을 쓰는 패턴 GA가 실행 중인지 (취소 후 아직 끝나지 않은 GA 포함, 후딜 제외)
bool UEnemyPatternComponent::IsBodyBusy() const
{
	if (!ASC.IsValid())
	{
		return false;
	}

	for (const TPair<FName, FGameplayAbilitySpecHandle>& Pair : Catalog.GetHandles())
	{
		if (!Catalog.UsesBody(Pair.Key))
		{
			continue;
		}

		const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Pair.Value);
		if (!Spec || !Spec->IsActive())
		{
			continue;
		}

		// 보조로 실행 중이면 몸 설정을 쓰지 않으므로 몸 사용이 아님
		const FGameplayAbilitySpecHandle Handle = Pair.Value;
		const bool bRunningAsCompanion = Slots.ContainsByPredicate(
			[&Handle](const FEnemyPatternSlot& Slot)
			{
				return !Slot.bPrimary && Slot.AbilityHandle == Handle;
			});

		if (!bRunningAsCompanion)
		{
			return true;
		}
	}

	return false;
}

bool UEnemyPatternComponent::IsRecovering() const
{
	const FEnemyPatternSlot* Primary = FindSlot(PrimaryRequestId);
	return Primary && Primary->Result == EEnemyPatternResult::Running && Primary->bRecovering;
}

int32 UEnemyPatternComponent::GetRunningSlotCount() const
{
	int32 Count = 0;
	for (const FEnemyPatternSlot& Slot : Slots)
	{
		// 상시 패턴은 별도 트랙이라 동시 실행 수에 세지 않음
		Count += Slot.Result == EEnemyPatternResult::Running && !Slot.bPersistent ? 1 : 0;
	}
	return Count;
}

/* 남은 위험 생성물 */

void UEnemyPatternComponent::RegisterLingeringHazard(int64 RequestId, AActor* SpawnedActor)
{
	const FEnemyPatternSlot* Slot = FindSlot(RequestId);
	if (!Slot || !IsValid(SpawnedActor))
	{
		return;
	}

	// 위험 태그만 남김: 기술 자원(몸 등)은 GA가 끝나면 해제
	FEnemyPatternLingeringHazard Hazard;
	Hazard.RequestId = RequestId;
	Hazard.Actor = SpawnedActor;
	Hazard.Hazards = Slot->Resources.Filter(FGameplayTagContainer(EnemyCombatTags::PatternHazard));
	Hazard.Threat = Slot->Threat;

	if (Hazard.Hazards.IsEmpty() && Hazard.Threat == 0)
	{
		return;
	}

	PruneLingeringHazards();
	LingeringHazards.Add(MoveTemp(Hazard));
}

/* 패턴 정보 조회 */

bool UEnemyPatternComponent::HasPattern(FName PatternId) const
{
	return !bStopped && Catalog.Find(PatternId) != nullptr;
}

bool UEnemyPatternComponent::GetPatternDefinition(FName PatternId, FEnemyPatternDefinition& OutDefinition) const
{
	OutDefinition = FEnemyPatternDefinition();

	const FEnemyPatternDefinition* Pattern = bStopped ? nullptr : Catalog.Find(PatternId);
	if (!Pattern)
	{
		return false;
	}

	OutDefinition = *Pattern;
	return true;
}

FName UEnemyPatternComponent::GetActivePatternId() const
{
	const FEnemyPatternSlot* Primary = IsRequestRunning(PrimaryRequestId) ? FindSlot(PrimaryRequestId) : nullptr;
	return Primary ? Primary->Pattern.PatternId : ReservedPatternId;
}

void UEnemyPatternComponent::ClearReservedPattern(FName PatternId)
{
	if (ReservedPatternId == PatternId)
	{
		ReservedPatternId = NAME_None;
	}
}

/* 엔진 이벤트 */

void UEnemyPatternComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAndInvalidate();
	ClearGrantedAbilities();

	Super::EndPlay(EndPlayReason);
}

/* 공통 */

AEnemyCharacter* UEnemyPatternComponent::GetEnemy() const
{
	return Cast<AEnemyCharacter>(GetOwner());
}

/* 후보 검사 */

// 실행 주체, 허용 목록, 타깃 생존 및 선택적인 거리 조건 확인
bool UEnemyPatternComponent::MatchesGeometry(const FEnemyPatternDefinition& Pattern, AActor* Target, bool bIgnorePhaseList) const
{
	const AEnemyCharacter* Enemy = GetEnemy();
	if (!IsValid(Enemy) || !Enemy->IsAlive() || (!bIgnorePhaseList && !Catalog.IsAvailable(Pattern.PatternId)))
	{
		return false;
	}

	// 타깃, 거리, 시야 조건을 모두 사용하지 않는 아레나 패턴은 타깃이 없어도 실행 가능
	const bool bNeedsTarget = Pattern.bRequiresTarget || Pattern.bCheckTargetDistance || Pattern.bRequiresLineOfSight;
	if (!bNeedsTarget)
	{
		return true;
	}

	if (Target == Enemy || !EnemyTargetUtils::IsAttackableTarget(Target))
	{
		return false;
	}

	if (!Pattern.bCheckTargetDistance)
	{
		return true;
	}

	// 수평 거리 범위와 높이 차이
	const FVector DeltaLocation = Target->GetActorLocation() - Enemy->GetActorLocation();
	const double DistanceSquared = DeltaLocation.SizeSquared2D();

	return DistanceSquared >= FMath::Square(Pattern.MinRange)
		&& DistanceSquared <= FMath::Square(Pattern.MaxRange)
		&& FMath::Abs(DeltaLocation.Z) <= Pattern.MaxHeightDifference;
}

// 거리 조건에 행동 가능 여부, 시야 및 GAS 활성화 조건을 추가 검사
bool UEnemyPatternComponent::CanExecutePattern(const FEnemyPatternDefinition& Pattern, AActor* Target, bool bIgnorePhaseList,
	TOptional<bool>* LineOfSightCache) const
{
	if (!MatchesGeometry(Pattern, Target, bIgnorePhaseList) || !ASC.IsValid())
	{
		return false;
	}

	const AEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy->CanRunEnemyAI())
	{
		return false;
	}

	if (Pattern.bRequiresLineOfSight)
	{
		// 같은 타깃의 시야는 후보 평가 한 번에 한 번만 트레이스
		bool bVisible = false;
		if (LineOfSightCache && LineOfSightCache->IsSet())
		{
			bVisible = LineOfSightCache->GetValue();
		}
		else
		{
			const AAIController* AI = Cast<AAIController>(Enemy->GetController());
			bVisible = IsValid(Target) && AI && AI->LineOfSightTo(Target);
			if (LineOfSightCache)
			{
				*LineOfSightCache = bVisible;
			}
		}

		if (!bVisible)
		{
			return false;
		}
	}

	// 이 패턴의 GA가 이미 실행 중이 아니고, GA 자체의 활성화 조건(태그, 쿨다운 등)도 만족해야 함
	const FGameplayAbilitySpecHandle Handle = Catalog.GetHandle(Pattern.PatternId);
	const FGameplayAbilitySpec* Spec = Handle.IsValid() ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;

	return Spec && !Spec->IsActive() && Spec->Ability
		&& Spec->Ability->CanActivateAbility(Handle, ASC->AbilityActorInfo.Get());
}

// 자동 선택 후보 수집
void UEnemyPatternComponent::CollectCandidates(AActor* Target, bool bCompanion, const TSet<FName>& Excluded,
	TArray<const FEnemyPatternDefinition*>& OutCandidates) const
{
	OutCandidates.Reset();

	// 지금 점유 중인 자원과 위험도: 실행 중, 예약 중, 남은 생성물 모두 포함
	FGameplayTagContainer Occupied;
	int32 OccupiedThreat = 0;
	CollectOccupied(Occupied, OccupiedThreat);

	// 타깃 시야는 후보마다가 아니라 이번 평가에서 한 번만
	TOptional<bool> LineOfSight;

	for (const FEnemyPatternDefinition& Pattern : Catalog.GetPatterns())
	{
		// 싼 검사부터: 가중치(자동 선택 대상) → 역할(보조는 몸 사용 금지) → 실행 중 → 자원·위험도 → 실행 조건(시야 트레이스, GA 조건)
		if (Excluded.Contains(Pattern.PatternId) || Catalog.GetSelectionWeight(Pattern) <= 0.f)
		{
			continue;
		}

		// 보조 후보: 보조 가능으로 체크했고 GA가 몸을 쓰지 않는 패턴만
		if (bCompanion && !Catalog.CanRunAsCompanion(Pattern.PatternId))
		{
			continue;
		}

		// 이미 다른 슬롯(상시 패턴 포함)에서 실행 중인 패턴: GA Spec이 하나라 다시 실행할 수 없음
		if (IsPatternRunning(Pattern.PatternId))
		{
			continue;
		}

		if (CanJoin(Pattern, Occupied, OccupiedThreat, bCompanion) && CanExecutePattern(Pattern, Target, false, &LineOfSight))
		{
			OutCandidates.Add(&Pattern);
		}
	}
}

// 살아 있는 슬롯과 남은 생성물이 점유한 자원·위험 태그와 위험도 합
void UEnemyPatternComponent::CollectOccupied(FGameplayTagContainer& OutResources, int32& OutThreat) const
{
	EnemyPatternOccupancy::Collect(Slots, LingeringHazards, OutResources, OutThreat);
}

// 점유 중인 자원·위험도와 겹치지 않고 함께 실행할 수 있는지
bool UEnemyPatternComponent::CanJoin(const FEnemyPatternDefinition& Pattern, const FGameplayTagContainer& Occupied, int32 OccupiedThreat,
	bool bAsCompanion) const
{
	const FGameplayTagContainer* Resources = Catalog.GetResources(Pattern.PatternId, bAsCompanion);
	return Resources && EnemyPatternOccupancy::CanJoin(*Resources, Pattern.Threat, Occupied, OccupiedThreat, Concurrency.MaxThreat);
}

bool UEnemyPatternComponent::CanJoinNow(const FEnemyPatternDefinition& Pattern, bool bAsCompanion) const
{
	FGameplayTagContainer Occupied;
	int32 OccupiedThreat = 0;
	CollectOccupied(Occupied, OccupiedThreat);
	return CanJoin(Pattern, Occupied, OccupiedThreat, bAsCompanion);
}

bool UEnemyPatternComponent::IsPatternRunning(FName PatternId) const
{
	return Slots.ContainsByPredicate(
		[PatternId](const FEnemyPatternSlot& Slot)
		{
			return Slot.Result == EEnemyPatternResult::Running && Slot.Pattern.PatternId == PatternId;
		});
}

/* 슬롯 관리 */

// 슬롯 생성: 자원과 동시 실행 수를 이 시점부터 점유
FEnemyPatternSlot& UEnemyPatternComponent::AddSlot(const FEnemyPatternDefinition& Pattern, AActor* Target, bool bPrimary)
{
	FEnemyPatternSlot& Slot = Slots.AddDefaulted_GetRef();

	Slot.RequestId = ++NextRequestId;
	Slot.Pattern = Pattern;
	Slot.Target = Target;
	Slot.AbilityHandle = Catalog.GetHandles().FindChecked(Pattern.PatternId);
	const FGameplayTagContainer* Resources = Catalog.GetResources(Pattern.PatternId, !bPrimary);
	Slot.Resources = Resources ? *Resources : FGameplayTagContainer();

	// 보조 실행: 몸은 주 패턴이 쓰므로 GA가 읽는 설정 복사본에서 몸 제어를 끔
	if (!bPrimary)
	{
		Slot.Pattern.bFaceTargetOnStart = false;
		Slot.Pattern.bLockRotation = false;
		Slot.Pattern.Movement.Policy = EEnemyPatternMovementPolicy::FollowLocomotion;
	}
	Slot.Threat = Pattern.Threat;
	Slot.Result = EEnemyPatternResult::Running;
	Slot.bPrimary = bPrimary;

	// 주의: 반환한 참조는 다음 AddSlot/RemoveSlot 전까지만 유효 (배열 재할당)
	return Slot;
}

FEnemyPatternSlot* UEnemyPatternComponent::FindSlot(int64 RequestId)
{
	return RequestId == 0 ? nullptr : Slots.FindByPredicate(
		[RequestId](const FEnemyPatternSlot& Slot)
		{
			return Slot.RequestId == RequestId;
		});
}

const FEnemyPatternSlot* UEnemyPatternComponent::FindSlot(int64 RequestId) const
{
	return const_cast<UEnemyPatternComponent*>(this)->FindSlot(RequestId);
}

FEnemyPatternSlot* UEnemyPatternComponent::FindActiveSlotForAbility(const FGameplayAbilitySpecHandle& AbilityHandle)
{
	return Slots.FindByPredicate(
		[&AbilityHandle](const FEnemyPatternSlot& Candidate)
		{
			return Candidate.AbilityHandle == AbilityHandle && Candidate.bStarted
				&& Candidate.Result == EEnemyPatternResult::Running && !Candidate.bRecovering;
		});
}

const FEnemyPatternSlot* UEnemyPatternComponent::FindActiveSlotForAbility(const FGameplayAbilitySpecHandle& AbilityHandle) const
{
	return const_cast<UEnemyPatternComponent*>(this)->FindActiveSlotForAbility(AbilityHandle);
}

// 슬롯의 GA 활성화: 실패하면 슬롯을 지우고 false
bool UEnemyPatternComponent::ActivateSlot(int64 RequestId)
{
	FEnemyPatternSlot* Slot = FindSlot(RequestId);
	if (!Slot || !ASC.IsValid())
	{
		return false;
	}

	// GA는 활성화 도중 GetExecutionForAbility로 이 슬롯을 찾으므로 활성화 전에 표시
	Slot->bStarted = true;

	const FGameplayAbilitySpecHandle AbilityHandle = Slot->AbilityHandle;
	const float TimeoutSeconds = Slot->Pattern.MaxExecutionSeconds;
	const bool bPrimary = Slot->bPrimary;

	// 이 아래부터 Slot 포인터는 쓰지 않음: 활성화 중 콜백이 슬롯을 지우거나 배열을 바꿀 수 있음
	if (!ASC->TryActivateAbility(AbilityHandle))
	{
		RemoveSlot(RequestId);

		if (bPrimary && PrimaryRequestId == RequestId)
		{
			PrimaryRequestId = 0;
		}

		return false;
	}

	// 활성화 안에서 바로 끝나지 않았을 때만 종료 누락 감시
	if (FEnemyPatternSlot* Running = FindSlot(RequestId);
		Running && Running->Result == EEnemyPatternResult::Running && !Running->bRecovering)
	{
		GetWorld()->GetTimerManager().SetTimer(Running->TimeoutTimer,
			FTimerDelegate::CreateUObject(this, &UEnemyPatternComponent::HandleExecutionTimeout, RequestId),
			TimeoutSeconds, false);
	}

	return true;
}

// 주 패턴 시작 직후 빈 자리에 보조 패턴을 하나씩 추첨해 예약 또는 실행
void UEnemyPatternComponent::FillCompanions(AActor* Target)
{
	const int32 FreeSlots = Concurrency.MaxConcurrentPatterns - GetRunningSlotCount();

	// 이번 추첨에서 이미 뽑았거나 시작에 실패한 패턴
	TSet<FName> Excluded;

	for (int32 SlotIndex = 0; SlotIndex < FreeSlots; ++SlotIndex)
	{
		// 빈 자리마다 확률 추첨: 상한까지 항상 채우지 않도록 하는 밸런싱 손잡이
		if (FMath::FRand() >= Concurrency.CompanionChance)
		{
			continue;
		}

		// 한 자리를 채울 때까지 후보를 다시 계산하며 추첨 (앞에서 뽑힌 패턴의 자원·위험도 반영)
		while (true)
		{
			TArray<const FEnemyPatternDefinition*> Candidates;
			CollectCandidates(Target, true, Excluded, Candidates);

			const FEnemyPatternDefinition* Selected = EnemyPatternOccupancy::PickWeighted(Candidates, Catalog);
			if (!Selected)
			{
				// 함께 실행할 후보가 더 없음: 남은 자리도 채울 수 없으므로 종료
				return;
			}

			Excluded.Add(Selected->PatternId);

			const int64 RequestId = AddSlot(*Selected, Target, false).RequestId;

			// 시작 지연: 지연 동안에도 자리와 자원은 예약으로 점유
			if (Selected->CompanionStartDelay > 0.f)
			{
				GetWorld()->GetTimerManager().SetTimer(FindSlot(RequestId)->StartDelayTimer,
					FTimerDelegate::CreateUObject(this, &UEnemyPatternComponent::HandleStartDelayFinished, RequestId),
					Selected->CompanionStartDelay, false);
				break;
			}

			if (ActivateSlot(RequestId))
			{
				break;
			}

			// 활성화 실패: 슬롯은 ActivateSlot이 지웠으므로 다른 후보로 다시 추첨
		}
	}
}

// 슬롯 종료: 주 패턴은 결과를 남기고(BT가 해제), 보조 패턴은 슬롯을 지움
void UEnemyPatternComponent::FinishSlot(int64 RequestId, EEnemyPatternResult Result)
{
	FEnemyPatternSlot* Slot = FindSlot(RequestId);
	if (!Slot)
	{
		return;
	}

	ClearSlotTimers(*Slot);
	Slot->bRecovering = false;
	Slot->Result = Result;

	const bool bPersistent = Slot->bPersistent;

	if (!Slot->bPrimary)
	{
		RemoveSlot(RequestId);
	}

	// 상시 패턴: 다음 실행 예약 (슬롯을 지운 뒤 처리해야 같은 패턴을 다시 시작할 수 있음)
	if (bPersistent)
	{
		HandlePersistentSlotFinished(RequestId, Result);
	}
}

void UEnemyPatternComponent::RemoveSlot(int64 RequestId)
{
	const int32 Index = Slots.IndexOfByPredicate(
		[RequestId](const FEnemyPatternSlot& Slot)
		{
			return Slot.RequestId == RequestId;
		});

	if (Index == INDEX_NONE)
	{
		return;
	}

	ClearSlotTimers(Slots[Index]);
	Slots.RemoveAtSwap(Index);
}

void UEnemyPatternComponent::ClearSlotTimers(FEnemyPatternSlot& Slot)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Slot.StartDelayTimer);
		World->GetTimerManager().ClearTimer(Slot.TimeoutTimer);
		World->GetTimerManager().ClearTimer(Slot.RecoveryTimer);
	}
}

void UEnemyPatternComponent::PruneLingeringHazards()
{
	LingeringHazards.RemoveAllSwap(
		[](const FEnemyPatternLingeringHazard& Hazard)
		{
			return !Hazard.Actor.IsValid();
		});
}

/* GAS와 타이머 콜백 */

// 모든 패턴 GA의 종료 이벤트: Spec Handle로 실행 중인 슬롯을 찾아 처리
void UEnemyPatternComponent::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (bStopped)
	{
		return;
	}

	FEnemyPatternSlot* Slot = FindActiveSlotForAbility(EndedData.AbilitySpecHandle);

	// 이미 취소/해제된 실행이나 이 컴포넌트가 관리하지 않는 GA
	if (!Slot)
	{
		return;
	}

	const int64 RequestId = Slot->RequestId;
	GetWorld()->GetTimerManager().ClearTimer(Slot->TimeoutTimer);

	if (EndedData.bWasCancelled)
	{
		FinishSlot(RequestId, EEnemyPatternResult::Cancelled);
		return;
	}

	const float RecoverySeconds = Slot->Pattern.RecoverySeconds;

	// 주 패턴: 후딜(이동 잠금) 후 완료, 다음 공격 간격 추첨
	if (Slot->bPrimary)
	{
		if (RecoverySeconds > 0.f)
		{
			StartPrimaryRecovery(*Slot);
		}
		else
		{
			HandleRecoveryFinished(RequestId);
		}
		return;
	}

	// 보조 패턴: 후딜 동안 자리만 유지 (몸을 쓰지 않으므로 이동 잠금 없음)
	if (RecoverySeconds > 0.f)
	{
		StartRecoveryTimer(*Slot);
		return;
	}

	FinishSlot(RequestId, EEnemyPatternResult::Completed);
}

// 시작 지연 종료: 그사이 조건이 바뀌었으면 실행하지 않고 자리 반환
void UEnemyPatternComponent::HandleStartDelayFinished(int64 RequestId)
{
	const FEnemyPatternSlot* Slot = FindSlot(RequestId);
	if (!Slot || Slot->Result != EEnemyPatternResult::Running || Slot->bStarted)
	{
		return;
	}

	const bool bCanRun = !bStopped && !bRequestsBlocked && CanExecutePattern(Slot->Pattern, Slot->Target.Get());
	if (!bCanRun || !ActivateSlot(RequestId))
	{
		// ActivateSlot이 실패했으면 이미 지워졌으므로 남아 있을 때만 정리
		FinishSlot(RequestId, EEnemyPatternResult::Failed);
	}
}

// 종료 이벤트 누락을 실패로 기록하고 해당 능력 취소
void UEnemyPatternComponent::HandleExecutionTimeout(int64 RequestId)
{
	const FEnemyPatternSlot* Slot = FindSlot(RequestId);
	if (!Slot || Slot->Result != EEnemyPatternResult::Running || !Slot->bStarted || Slot->bRecovering)
	{
		return;
	}

	UE_LOG(LogEnemy, Warning, TEXT("[EnemyPattern] %s: pattern '%s' did not end within %.1fs"),
		*GetNameSafe(GetOwner()), *Slot->Pattern.PatternId.ToString(), Slot->Pattern.MaxExecutionSeconds);

	const FGameplayAbilitySpecHandle AbilityHandle = Slot->AbilityHandle;
	FinishSlot(RequestId, EEnemyPatternResult::Failed);

	if (ASC.IsValid())
	{
		ASC->CancelAbilityHandle(AbilityHandle);
	}
}

// 후딜 종료: 주 패턴은 이동 잠금 해제와 다음 공격 간격 추첨
void UEnemyPatternComponent::HandleRecoveryFinished(int64 RequestId)
{
	const FEnemyPatternSlot* Slot = FindSlot(RequestId);
	if (!Slot || Slot->Result != EEnemyPatternResult::Running)
	{
		return;
	}

	if (Slot->bPrimary)
	{
		ReleaseRecoveryMovementLock();

		// 주 패턴 정상 완료 한 번에만 추첨: 요청 해제, BT 조건 조회, 보조 패턴은 이 시각을 바꾸지 않음
		Pacing.Roll(GetWorld()->GetTimeSeconds());
	}

	FinishSlot(RequestId, EEnemyPatternResult::Completed);
}

void UEnemyPatternComponent::StartRecoveryTimer(FEnemyPatternSlot& Slot)
{
	Slot.bRecovering = true;
	GetWorld()->GetTimerManager().SetTimer(Slot.RecoveryTimer,
		FTimerDelegate::CreateUObject(this, &UEnemyPatternComponent::HandleRecoveryFinished, Slot.RequestId),
		Slot.Pattern.RecoverySeconds, false);
}

void UEnemyPatternComponent::StartPrimaryRecovery(FEnemyPatternSlot& Slot)
{
	Slot.bRecovering = true;

	// 후딜 동안 제자리 정지
	if (AEnemyCharacter* Enemy = GetEnemy())
	{
		Enemy->AcquireAttackMovementLock();
		bOwnsRecoveryMovementLock = true;
		Enemy->GetCharacterMovement()->StopMovementImmediately();
		Enemy->ConsumeMovementInputVector();
	}

	StartRecoveryTimer(Slot);

	// 알림 중 BT가 요청을 취소해도 후딜 타이머를 다시 만들지 않도록 마지막에 통지
	OnRecoveryStarted.Broadcast();
}

void UEnemyPatternComponent::ReleaseRecoveryMovementLock()
{
	if (!bOwnsRecoveryMovementLock)
	{
		return;
	}

	// 다른 GA나 상태가 보유한 이동 잠금까지 해제하지 않음
	if (AEnemyCharacter* Enemy = GetEnemy())
	{
		Enemy->ReleaseAttackMovementLock();
	}

	bOwnsRecoveryMovementLock = false;
}

// 이 컴포넌트가 부여한 GA 제거와 종료 이벤트 구독 해제
void UEnemyPatternComponent::ClearGrantedAbilities()
{
	if (ASC.IsValid())
	{
		ASC->OnAbilityEnded.Remove(AbilityEndedDelegateHandle);
	}

	Catalog.ClearAbilities(ASC.Get(), GetOwner()->HasAuthority());
	AbilityEndedDelegateHandle.Reset();
}
