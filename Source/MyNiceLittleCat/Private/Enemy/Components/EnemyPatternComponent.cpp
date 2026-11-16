// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Components/EnemyPatternComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

#include "Enemy/Combat/EnemyTargetUtils.h"
#include "Enemy/EnemyCharacter.h"

UEnemyPatternComponent::UEnemyPatternComponent()
{
	// GA 종료 이벤트와 타이머로 동작하므로 Tick 불필요
	PrimaryComponentTick.bCanEverTick = false;

}

/* 초기화와 설정 */

// 이전 실행 정리 후 패턴 설정 검사 및 필요한 GA 부여
bool UEnemyPatternComponent::InitializePatterns(UAbilitySystemComponent* InASC, UEnemyPatternSetData* InPatternSet)
{
	StopAndInvalidate();
	ClearGrantedAbilities();

	ASC = InASC;
	PatternSet = InPatternSet;

	// ASC 또는 패턴 설정이 유효하지 않으면 false 반환
	if (!GetOwner()->HasAuthority() || !IsValid(InASC) || !IsValid(InPatternSet) || InPatternSet->Patterns.IsEmpty())
		return false;
	

	/* 패턴 부여 전에 전체 설정 검사: 하나라도 잘못되면 아무것도 부여하지 않음 */
	TSet<FName> PatternIds;

	for (const FEnemyPatternDefinition& Pattern : InPatternSet->Patterns)
	{
		FString Error;
		if (!Pattern.Validate(Error) || PatternIds.Contains(Pattern.PatternId))
		{
			UE_LOG(LogTemp, Error, TEXT("[EnemyPattern] %s: invalid pattern settings. %s"),
				*GetNameSafe(InPatternSet), Error.IsEmpty() ? TEXT("Duplicate PatternId.") : *Error);
			return false;
		}

		// 설정 검사 완료된 패턴 ID 수집
		PatternIds.Add(Pattern.PatternId);
	}

	/* GA 부여: 같은 AbilityClass는 한 번만 부여하고 핸들 공유 */
	TMap<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle> GrantedClasses;

	for (const FEnemyPatternDefinition& Pattern : InPatternSet->Patterns)
	{
		// ASC에 부여한 능력 식별 핸들
		FGameplayAbilitySpecHandle Handle;

		if (const FGameplayAbilitySpecHandle* Existing = GrantedClasses.Find(Pattern.AbilityClass))
		{
			Handle = *Existing;	// 동일 GA 클래스의 기존 핸들
		}
		else
		{
			// SourceObject = 이 컴포넌트: GA 디버깅 시 부여 출처 확인용
			Handle = InASC->GiveAbility(
				FGameplayAbilitySpec(
					Pattern.AbilityClass,
					1,
					INDEX_NONE,
					this
				)
			);

			// 부여 실패 시 이미 부여한 GA까지 정리하고 초기화 실패
			if (!Handle.IsValid())
			{
				ClearGrantedAbilities();
				return false;
			}

			GrantedClasses.Add(Pattern.AbilityClass, Handle);
		}

		PatternHandles.Add(Pattern.PatternId, Handle);
	}

	AvailablePatterns = MoveTemp(PatternIds);

	bStopped = false;
	bRequestsBlocked = false;

	return true;
}

// 현재 페이즈에서 사용할 패턴 ID만 교체
void UEnemyPatternComponent::SetAvailablePatterns(const TArray<FName>& PatternIds)
{
	AvailablePatterns.Reset();

	for (FName PatternId : PatternIds)
	{
		if (HasPattern(PatternId))
		{
			AvailablePatterns.Add(PatternId);
		}
	}
}

bool UEnemyPatternComponent::ConfigureAttackInterval(float InMinInterval, float InMaxInterval)
{
	if (!FMath::IsFinite(InMinInterval) || !FMath::IsFinite(InMaxInterval)
		|| InMinInterval < 0.f || InMaxInterval < InMinInterval || !IsIdle())
	{
		return false;
	}
	MinAttackInterval = InMinInterval;
	MaxAttackInterval = InMaxInterval;
	return true;
}

void UEnemyPatternComponent::ResetAttackInterval()
{
	NextAttackAllowedTime = 0.0;
}

/* 실행 조건 조회 */

bool UEnemyPatternComponent::CanStartAttack() const
{
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());

	return IsValid(Enemy) && Enemy->HasAuthority() && Enemy->CanRunEnemyAI()
		&& !bStopped && !bRequestsBlocked && ASC.IsValid() && HasPatterns()
		&& ActiveRequestId == 0 && IsIdle() && GetAttackIntervalRemaining() <= 0.f;
}

float UEnemyPatternComponent::GetAttackIntervalRemaining() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<float>(FMath::Max(0.0, NextAttackAllowedTime - World->GetTimeSeconds())) : 0.f;
}

// 쿨다운과 시야를 제외한 현재 패턴의 타깃 및 거리 조건 검사
bool UEnemyPatternComponent::HasPatternInRange(AActor* Target) const
{
	if (bStopped || !PatternSet)
		return false;

	// 실행 가능한 패턴이 하나라도 있으면 true
	for (const FEnemyPatternDefinition& Pattern : PatternSet->Patterns)
	{
		if (MatchesGeometry(Pattern, Target))
		{
			return true;
		}
	}

	// 실행 가능한 패턴이 하나도 없으면 false
	return false;
}

bool UEnemyPatternComponent::HasExecutableAttack(AActor* Target) const
{
	if (!CanStartAttack())
		return false;

	// 자동 선택 대상(가중치 > 0) 중 실행 가능한 패턴이 하나라도 있으면 true
	for (const FEnemyPatternDefinition& Pattern : PatternSet->Patterns)
	{
		// 실행 가능한 패턴이 하나라도 있으면 true
		// 패턴 선택 가중치가 유효하고 실행 가능한 패턴인지 확인
		if (Pattern.Weight > 0.f && CanExecutePattern(Pattern, Target))
		{
			return true;
		}
	}

	// 실행 가능한 패턴이 하나도 없으면 false
	return false;
}

bool UEnemyPatternComponent::CanStartPattern(FName PatternId, AActor* Target) const
{
	if (!CanStartAttack())
		return false;

	const FEnemyPatternDefinition* Pattern = FindPattern(PatternId);

	return Pattern && CanExecutePattern(*Pattern, Target);
}

bool UEnemyPatternComponent::PickAttackPattern(AActor* Target, FName& OutPatternId) const
{
	OutPatternId = NAME_None;

	if (!CanStartAttack())
		return false;

	/* 후보 수집 및 가중치 계산 */
	TArray<const FEnemyPatternDefinition*> Candidates;

	// 패턴 선택 후보들의 가중치 합산 구하기
	float TotalWeight = 0.f;
	for (const FEnemyPatternDefinition& Pattern : PatternSet->Patterns)
	{
		if (Pattern.Weight > 0.f && CanExecutePattern(Pattern, Target))
		{
			Candidates.Add(&Pattern);
			TotalWeight += Pattern.Weight;
		}
	}

	// 후보들의 가중치 합산이 유효하지 않으면 실패 처리
	if (Candidates.IsEmpty()
		|| !FMath::IsFinite(TotalWeight)
		|| TotalWeight <= 0.f)
	{
		return false;
	}

	/* 가중치 추첨: 0 ~ 총합 사이 값을 뽑고, 후보 가중치를 차례로 빼서 처음으로 0 이하가 되는 후보 선택 */

	float Roll = FMath::FRand() * TotalWeight;	// 난수에 총합을 곱해서 값 추첨

	// 부동소수점 오차로 반복문에서 선택되지 않는 경우를 대비해 마지막 후보를 기본값을 설정
	const FEnemyPatternDefinition* Selected = Candidates.Last();

	// 반복문을 돌며 추첨된 값(Roll)에 해당하는 후보자 찾기
	for (const FEnemyPatternDefinition* Candidate : Candidates)
	{
		// 각 후보자들의 가중치를 추첨된 값에서 뺌
		Roll -= Candidate->Weight;

		// 가중치를 차감한 결과가 처음으로 0 이하가 되는 후보를 선택
		if (Roll <= 0.f)
		{
			Selected = Candidate;
			break;
		}
	}

	// 추첨 성공
	OutPatternId = Selected->PatternId;
	return true;
}

/* 실행 요청 */

// 조건을 만족하는 후보 중 확률 가중치로 공격 하나 선택
bool UEnemyPatternComponent::TryStartAttack(AActor * Target, int64 & OutRequestId)
{
	OutRequestId = 0;

	// 패턴 추첨해서 시작 시도
	FName SelectedId;
	return PickAttackPattern(Target, SelectedId) && TryStartPattern(SelectedId, Target, OutRequestId);
}

// 지정한 패턴을 실행하고 요청 번호 반환
bool UEnemyPatternComponent::TryStartPattern(FName PatternId, AActor* Target, int64& OutRequestId)
{
	OutRequestId = 0;

	const FEnemyPatternDefinition* Pattern = FindPattern(PatternId);
	if (!CanStartAttack() || !Pattern || !CanExecutePattern(*Pattern, Target))
		return false;

	/* 요청 상태 준비 */

	UnbindAbilityEnded();

	bRecovering = false;
	ActivePattern = *Pattern;
	ActiveTarget = Target;

	// 실행 설정으로 인계했으므로 사전 선택 기록은 비움
	ReservedPatternId = NAME_None;

	ActiveAbilityHandle = PatternHandles.FindChecked(PatternId);
	ActiveRequestId = ++NextRequestId;
	ActiveResult = EEnemyPatternResult::Running;

	// 활성화 직전 요청 번호: 이전 실행의 콜백을 무시하는 기준
	const int64 StartedRequestId = ActiveRequestId;

	/* 종료 이벤트 구독: 활성화 도중 즉시 종료되는 경우도 받기 위해 활성화 전에 구독 */

	AbilityEndedDelegateHandle = ASC->OnAbilityEnded.AddWeakLambda(
		this,
		[this, StartedRequestId](const FAbilityEndedData& EndedData)
		{
			if (StartedRequestId == ActiveRequestId)
			{
				HandleAbilityEnded(EndedData);
			}
		}
	);

	/* GA 활성화 */

	if (!ASC->TryActivateAbility(ActiveAbilityHandle))
	{
		ActiveResult = EEnemyPatternResult::Failed;
		UnbindAbilityEnded();
		ActiveRequestId = 0;
		ActiveTarget.Reset();
		return false;
	}

	// 활성화 중 사망 또는 초기화로 실행이 무효화됐으면 요청을 반환하지 않음
	if (bStopped || ActiveRequestId != StartedRequestId)
	{
		return false;
	}

	OutRequestId = ActiveRequestId;

	/* GA 종료 누락 감시 */

	if (ActiveResult == EEnemyPatternResult::Running && AbilityEndedDelegateHandle.IsValid())
	{
		GetWorld()->GetTimerManager().SetTimer(
			ExecutionTimeoutHandle,
			this,
			&UEnemyPatternComponent::HandleExecutionTimeout,
			Pattern->MaxExecutionSeconds,
			false
		);
	}

	return true;
}

// 요청 번호가 일치하는 공격만 취소하고 Cancelled 결과 유지
void UEnemyPatternComponent::CancelRequest(int64 RequestId)
{
	if (!IsRequestRunning(RequestId))
		return;

	bRecovering = false;
	ActiveResult = EEnemyPatternResult::Cancelled;
	ReleaseRecoveryMovementLock();
	
	GetWorld()->GetTimerManager().ClearTimer(ExecutionTimeoutHandle);
	GetWorld()->GetTimerManager().ClearTimer(RecoveryTimerHandle);

	UnbindAbilityEnded();

	if (ASC.IsValid())
	{
		ASC->CancelAbilityHandle(ActiveAbilityHandle);
	}
}

// 현재 요청 번호로 실행 중인 공격 취소
void UEnemyPatternComponent::CancelActiveRequest()
{
	CancelRequest(ActiveRequestId);
}

// 실행 중이면 먼저 취소하고 요청 번호, 결과 및 타깃 해제
void UEnemyPatternComponent::ReleaseRequest(int64 RequestId)
{
	if (RequestId == 0 || RequestId != ActiveRequestId)
		return;

	CancelRequest(RequestId);

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ExecutionTimeoutHandle);
		GetWorld()->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	}

	ActiveRequestId = 0;
	ActiveResult = EEnemyPatternResult::Invalid;
	ActiveTarget.Reset();

	UnbindAbilityEnded();
}

// 요청을 먼저 무효화한 뒤 타이머, 구독 및 능력 실행 정리
void UEnemyPatternComponent::StopAndInvalidate()
{
	ReleaseRecoveryMovementLock();
	ResetAttackInterval();

	bRecovering = false;
	bStopped = true;
	bRequestsBlocked = true;

	ActiveRequestId = 0;
	ActiveResult = EEnemyPatternResult::Invalid;
	ReservedPatternId = NAME_None;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExecutionTimeoutHandle);
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	}

	UnbindAbilityEnded();

	if (ASC.IsValid())
	{
		for (const TPair<FName, FGameplayAbilitySpecHandle>& Pair : PatternHandles)
		{
			ASC->CancelAbilityHandle(Pair.Value);
		}
	}

	ActiveTarget.Reset();
}

/* 요청 상태 조회 */

EEnemyPatternResult UEnemyPatternComponent::GetRequestResult(int64 RequestId) const
{
	if (bStopped
		|| RequestId == 0
		|| RequestId != ActiveRequestId)
		return EEnemyPatternResult::Invalid;

	return ActiveResult;
}

bool UEnemyPatternComponent::IsRequestRunning(int64 RequestId) const
{
	return !bStopped
		&& RequestId != 0 && RequestId == ActiveRequestId
		&& ActiveResult == EEnemyPatternResult::Running;
}

bool UEnemyPatternComponent::GetActiveExecution(FEnemyPatternDefinition& OutPattern, AActor*& OutTarget, int64& OutRequestId) const
{
	OutPattern = FEnemyPatternDefinition();
	OutTarget = nullptr;
	OutRequestId = 0;

	// 활성화된 패턴 없으면 false 반환
	if (!IsRequestRunning(ActiveRequestId))
		return false;

	OutPattern = ActivePattern;
	OutTarget = ActiveTarget.Get();
	OutRequestId = ActiveRequestId;

	return true;
}

bool UEnemyPatternComponent::GetActiveMovement(FEnemyPatternMovementSettings& OutMovement, AActor*& OutTarget, int64& OutRequestId) const
{
	OutMovement = FEnemyPatternMovementSettings();
	OutTarget = nullptr;
	OutRequestId = 0;

	// 활성화된 패턴 없으면 false 반환
	if (!IsRequestRunning(ActiveRequestId))
		return false;

	// 후딜 중에는 기본값(FollowLocomotion) 유지: 실제 정지는 후딜 이동 잠금이 담당
	if (!bRecovering)
	{
		OutMovement = ActivePattern.Movement;
	}

	OutTarget = ActiveTarget.Get();
	OutRequestId = ActiveRequestId;
	return true;
}

// 관리 중인 공격 GA가 실제로 모두 종료됐는지 확인
bool UEnemyPatternComponent::IsIdle() const
{
	if (ActiveResult == EEnemyPatternResult::Running)
		return false;

	// 취소 요청을 했어도 실제 GA가 종료되지 않았으면 새로 실행 하지 않음
	if (ASC.IsValid())
	{
		for (const TPair<FName, FGameplayAbilitySpecHandle>& Pair : PatternHandles)
		{
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Pair.Value);
			if (Spec && Spec->IsActive())
				return false;
		}
	}

	return true;
}

/* 패턴 정보 조회 */

bool UEnemyPatternComponent::HasPattern(FName PatternId) const
{
	return !bStopped && FindPattern(PatternId) != nullptr;
}

bool UEnemyPatternComponent::GetPatternDefinition(FName PatternId, FEnemyPatternDefinition& OutDefinition) const
{
	OutDefinition = FEnemyPatternDefinition();

	const FEnemyPatternDefinition* Pattern = bStopped ? nullptr : FindPattern(PatternId);
	if (!Pattern)
		return false;

	OutDefinition = *Pattern;
	return true;
}

FName UEnemyPatternComponent::GetActivePatternId() const
{
	return IsRequestRunning(ActiveRequestId) ? ActivePattern.PatternId : ReservedPatternId;
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

/* 내부 처리 */

const FEnemyPatternDefinition* UEnemyPatternComponent::FindPattern(FName PatternId) const
{
	if (!PatternSet)
	{
		return nullptr;
	}
	return PatternSet->Patterns.FindByPredicate(
		[PatternId](const FEnemyPatternDefinition& Pattern)
		{
			return Pattern.PatternId == PatternId;
		});
}

// 실행 주체, 허용 목록, 타깃 생존 및 선택적인 거리 조건 확인
bool UEnemyPatternComponent::MatchesGeometry(const FEnemyPatternDefinition& Pattern, AActor* Target) const
{
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	if (!IsValid(Enemy) || !Enemy->IsAlive()
		|| !AvailablePatterns.Contains(Pattern.PatternId))
		return false;

	// 타깃, 거리, 시야 조건을 모두 사용하지 않는 아레나 패턴은 타깃이 없어도 실행 가능
	const bool bNeedsTarget = Pattern.bRequiresTarget
		|| Pattern.bCheckTargetDistance || Pattern.bRequiresLineOfSight;
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
	const float DistanceSquared = DeltaLocation.SizeSquared2D();

	return DistanceSquared >= FMath::Square(Pattern.MinRange)
		&& DistanceSquared <= FMath::Square(Pattern.MaxRange)
		&& FMath::Abs(DeltaLocation.Z) <= Pattern.MaxHeightDifference;
}

// 거리 조건에 행동 가능 여부, 시야 및 GAS 활성화 조건을 추가 검사
bool UEnemyPatternComponent::CanExecutePattern(const FEnemyPatternDefinition& Pattern, AActor* Target) const
{
	if (!MatchesGeometry(Pattern, Target) || !ASC.IsValid())
		return false;

	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	if (!Enemy->CanRunEnemyAI())
		return false;

	if (Pattern.bRequiresLineOfSight)
	{
		const AAIController* AI = Cast<AAIController>(Enemy->GetController());
		
		if (!IsValid(Target) || !AI || !AI->LineOfSightTo(Target))
			return false;
	}

	// GA가 이미 실행 중이 아니고, GA 자체의 활성화 조건(태그, 쿨다운 등)도 만족해야 함
	const FGameplayAbilitySpecHandle* Handle = PatternHandles.Find(Pattern.PatternId);
	const FGameplayAbilitySpec* Spec =
		Handle
		? ASC->FindAbilitySpecFromHandle(*Handle)
		: nullptr;

	return Spec && !Spec->IsActive()
		&& Spec->Ability
		&& Spec->Ability->CanActivateAbility(*Handle, ASC->AbilityActorInfo.Get());
}

// 현재 능력의 종료 이벤트만 받아 완료 또는 취소 기록
void UEnemyPatternComponent::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (bStopped
		|| ActiveResult != EEnemyPatternResult::Running
		|| EndedData.AbilitySpecHandle != ActiveAbilityHandle)
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(ExecutionTimeoutHandle);
	UnbindAbilityEnded();

	if (EndedData.bWasCancelled)
	{
		ActiveResult = EEnemyPatternResult::Cancelled;
		return;
	}

	// 정상 종료: 후딜이 있으면 후딜 후 완료
	if (ActivePattern.RecoverySeconds > 0.f)
	{
		StartRecovery(ActivePattern.RecoverySeconds);
		return;
	}

	HandleRecoveryFinished();
}

void UEnemyPatternComponent::StartRecovery(float Duration)
{
	bRecovering = true;
	
	// 후딜 동안 제자리 정지
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		Enemy->AcquireAttackMovementLock();
		bOwnsRecoveryMovementLock = true;
		Enemy->GetCharacterMovement()->StopMovementImmediately();
		Enemy->ConsumeMovementInputVector();
	}

	GetWorld()->GetTimerManager().SetTimer(RecoveryTimerHandle, this,
		&UEnemyPatternComponent::HandleRecoveryFinished, Duration, false);

	// 알림 중 BT가 요청을 취소해도 후딜 타이머를 다시 만들지 않도록 마지막에 통지
	OnRecoveryStarted.Broadcast();
}

void UEnemyPatternComponent::HandleRecoveryFinished()
{
	ReleaseRecoveryMovementLock();
	bRecovering = false;

	if (IsRequestRunning(ActiveRequestId))
	{
		RollNextAttackTime();
		ActiveResult = EEnemyPatternResult::Completed;
	}
}

void UEnemyPatternComponent::ReleaseRecoveryMovementLock()
{
	if (!bOwnsRecoveryMovementLock)
		return;

	// 다른 GA나 상태가 보유한 이동 제한까지 해제하지 않음
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		Enemy->ReleaseAttackMovementLock();
	}

	bOwnsRecoveryMovementLock = false;
}

void UEnemyPatternComponent::RollNextAttackTime()
{
	// 정상 완료 한 번에만 추첨: 요청 해제나 BT 조건 조회는 이 시각을 바꾸지 않음
	const float Interval = FMath::FRandRange(MinAttackInterval, MaxAttackInterval);
	NextAttackAllowedTime = GetWorld()->GetTimeSeconds() + static_cast<double>(Interval);
}

// 종료 이벤트 누락을 실패로 기록하고 해당 능력 취소
void UEnemyPatternComponent::HandleExecutionTimeout()
{
	if (ActiveResult != EEnemyPatternResult::Running)
		return;

	UE_LOG(LogTemp, Warning, TEXT("[EnemyPattern] %s: pattern '%s' did not end within %.1fs"),
		*GetNameSafe(GetOwner()), *ActivePattern.PatternId.ToString(), ActivePattern.MaxExecutionSeconds);

	ActiveResult = EEnemyPatternResult::Failed;
	UnbindAbilityEnded();

	if (ASC.IsValid())
	{
		ASC->CancelAbilityHandle(ActiveAbilityHandle);
	}
}

void UEnemyPatternComponent::UnbindAbilityEnded()
{
	if (ASC.IsValid())
	{
		ASC->OnAbilityEnded.Remove(AbilityEndedDelegateHandle);
	}

	AbilityEndedDelegateHandle.Reset();
}

// 이 컴포넌트가 부여한 GA를 중복 없이 서버에서 제거
void UEnemyPatternComponent::ClearGrantedAbilities()
{
	if (ASC.IsValid() && GetOwner()->HasAuthority())
	{
		// 같은 GA 클래스는 핸들을 공유하므로 한 번만 제거
		TSet<FGameplayAbilitySpecHandle> UniqueHandles;
		for (const TPair<FName, FGameplayAbilitySpecHandle>& Pair : PatternHandles)
		{
			UniqueHandles.Add(Pair.Value);
		}

		for (FGameplayAbilitySpecHandle Handle : UniqueHandles)
		{
			ASC->ClearAbility(Handle);
		}
	}

	PatternHandles.Reset();
	AvailablePatterns.Reset();
}
