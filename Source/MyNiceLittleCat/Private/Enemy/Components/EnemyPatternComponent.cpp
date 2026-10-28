// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Components/EnemyPatternComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AIController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Enemy/EnemyCharacter.h"
#include "Interfaces/TargetableInterface.h"
#include "Characters/PlayerCharacter.h"

// 컴포넌트의 기본 실행 상태를 설정한다.
// 생성자: Tick 대신 GAS 종료 이벤트와 타이머로 실행 관리
UEnemyPatternComponent::UEnemyPatternComponent()
{
	// 전환 시퀀스를 감시할 때만 프레임 갱신을 활성화한다.
	PrimaryComponentTick.bCanEverTick = false;

}

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
	
	TSet<FName> PatternIds;

	/* 패턴 부여 전에 전체 설정 검사 */
	for (const FEnemyPatternDefinition& Pattern : InPatternSet->Patterns)
	{
		if (Pattern.PatternId.IsNone()					// 유효하지 않은 패턴이거나
			|| PatternIds.Contains(Pattern.PatternId)	// 이미 부여된 패턴이거나
			|| (!Pattern.bLogOnly && !Pattern.AbilityClass)					// GA 클래스가 없거나
			|| !FMath::IsFinite(Pattern.MinRange) || Pattern.MinRange < 0.f
			|| !FMath::IsFinite(Pattern.MaxRange) || Pattern.MaxRange < Pattern.MinRange
			|| !FMath::IsFinite(Pattern.MaxHeightDifference) || Pattern.MaxHeightDifference < 0.f
			|| !FMath::IsFinite(Pattern.Weight) || Pattern.Weight < 0.f						// 확률 가중치가 유효하지 않거나
			|| !FMath::IsFinite(Pattern.RecoverySeconds) || Pattern.RecoverySeconds < 0.f
			|| !Pattern.Movement.IsValid()
			|| !FMath::IsFinite(Pattern.MaxExecutionSeconds)
			|| Pattern.MaxExecutionSeconds <= 0.f)		// 공격 종료 누락 감지 제한 시간이 유효하지 않으면
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid pattern settings: %s"), *GetNameSafe(InPatternSet));

			return false;
		}

		// 설정 검사 완료된 패턴 ID 수집
		PatternIds.Add(Pattern.PatternId);
	}

	// 같은 AbilityClass는 한 번만 부여: GrantedClasses에 한 번 부여된 능력 기억
	TMap<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle> GrantedClasses;

	for (const FEnemyPatternDefinition& Pattern : InPatternSet->Patterns)
	{
		if (Pattern.bLogOnly)
		{
			continue;
		}
		// ASC에 부여한 능력 식별 핸들
		FGameplayAbilitySpecHandle Handle;

		if (const FGameplayAbilitySpecHandle* Existing = GrantedClasses.Find(Pattern.AbilityClass))
		{
			Handle = *Existing;	// 동일 GA 클래스의 기존 핸들
		}
		else
		{
			// 능력 부여
			Handle = InASC->GiveAbility(
				FGameplayAbilitySpec(
					Pattern.AbilityClass,
					1,
					INDEX_NONE,
					this
				)
			);

			// 능력 부여 실패 시 기존 능력 삭제
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

float UEnemyPatternComponent::GetAttackIntervalRemaining() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<float>(FMath::Max(0.0, NextAttackAllowedTime - World->GetTimeSeconds())) : 0.f;
}

bool UEnemyPatternComponent::CanStartAttack() const
{
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	return IsValid(Enemy) && Enemy->HasAuthority() && Enemy->CanRunEnemyAI()
		&& !bStopped && !bRequestsBlocked && ASC.IsValid() && HasPatterns()
		&& ActiveRequestId == 0 && IsIdle() && GetAttackIntervalRemaining() <= 0.f;
}

// 조건을 만족하는 후보 중 확률 가중치로 공격 하나 선택
bool UEnemyPatternComponent::TryStartAttack(AActor * Target, int64 & OutRequestId)
{
	OutRequestId = 0;

	if (!CanStartAttack())
	{
		return false;
	}

	// 현재 실행 조건을 만족하는 자동 선택 후보
	TArray<const FEnemyPatternDefinition*> Candidates;
	float TotalWeight = 0.f;

	for (const FEnemyPatternDefinition& Pattern : PatternSet->Patterns)
	{
		if (Pattern.Weight > 0.f && CanExecutePattern(Pattern, Target))
		{
			Candidates.Add(&Pattern);
			TotalWeight += Pattern.Weight;
		}
	}

	if (Candidates.IsEmpty() || !FMath::IsFinite(TotalWeight) || TotalWeight <= 0.f)
		return false;

	float Roll = FMath::FRand() * TotalWeight;
	const FEnemyPatternDefinition* Selected = Candidates.Last();
	for (const FEnemyPatternDefinition* Candidate : Candidates)
	{
		Roll -= Candidate->Weight;

		if (Roll <= 0.f)
		{
			Selected = Candidate;
			break;
		}
	}

	return TryStartPattern(Selected->PatternId, Target, OutRequestId);
}

// 지정한 패턴을 실행하고 요청 번호 반환; true는 시작 성공
bool UEnemyPatternComponent::TryStartPattern(FName PatternId, AActor* Target, int64& OutRequestId)
{
	OutRequestId = 0;

	const FEnemyPatternDefinition* Pattern = FindPattern(PatternId);

	if (!CanStartAttack() || !Pattern || !CanExecutePattern(*Pattern, Target))
	{
		return false;
	}

	UnbindAbilityEnded();

	bRecovering = false;
	ActivePattern = *Pattern;
	ActiveTarget = Target;

	// 미담당 패턴은 실제 능력을 부여하거나 실행하지 않고 요청만 완료
	if (Pattern->bLogOnly)
	{
		ActiveRequestId = ++NextRequestId;
		ActiveResult = EEnemyPatternResult::Completed;
		OutRequestId = ActiveRequestId;
		if (!LoggedPlaceholderPatterns.Contains(PatternId))
		{
			UE_LOG(LogTemp, Log, TEXT("[EnemyPattern][LogOnly] %s skipped"), *PatternId.ToString());
			LoggedPlaceholderPatterns.Add(PatternId);
		}
		return true;
	}
	ActiveAbilityHandle = PatternHandles.FindChecked(PatternId);

	ActiveRequestId = ++NextRequestId;
	ActiveResult = EEnemyPatternResult::Running;

	// 활성화 직전 요청 번호; 이전 실행의 콜백을 무시하는 기준
	const int64 StartedRequestId = ActiveRequestId;

	// 활성화 도중 즉시 종료되는 경우 처리
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

// 요청 번호의 실행 결과 조회; 해제되거나 다른 요청이면 Invalid
EEnemyPatternResult UEnemyPatternComponent::GetRequestResult(int64 RequestId) const
{
	if (bStopped
		|| RequestId == 0
		|| RequestId != ActiveRequestId)
		return EEnemyPatternResult::Invalid;

	return ActiveResult;
}

// 초기화 과정에서 해당 패턴 ID가 등록됐는지 확인
bool UEnemyPatternComponent::HasPattern(FName PatternId) const
{
	return !bStopped && FindPattern(PatternId) != nullptr;
}

// 관리 중인 공격 GA가 실제로 모두 종료됐는지 확인
bool UEnemyPatternComponent::IsIdle() const
{
	if (ActiveResult == EEnemyPatternResult::Running)
		return false;

	// 취소 요청을 했어도 실제 GA가 종료되지 않았으면 새로 실행 하지 않음
	if (ASC.IsValid())
	{
		for (const auto& Pair : PatternHandles)
		{
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Pair.Value);
		
			if (Spec && Spec->IsActive())
				return false;
		}
	}

	return true;
}

// 쿨다운과 시야를 제외한 현재 패턴의 타깃 및 거리 조건 검사
bool UEnemyPatternComponent::HasPatternInRange(AActor* Target) const
{
	if (bStopped || !PatternSet)
		return false;

	for (const FEnemyPatternDefinition& Pattern : PatternSet->Patterns)
	{
		if (MatchesGeometry(Pattern, Target))
		{
			return true;
		}
	}

	return false;
}

// 현재 요청의 설정, 타깃 포인터 및 번호를 GA의 출력 인자로 전달
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

// 어빌리티가 끝난 뒤에도 후딜이 진행 중이면 해당 구간의 이동 정책을 제공한다.
bool UEnemyPatternComponent::GetActiveMovement(FEnemyPatternMovementSettings& OutMovement, AActor*& OutTarget, int64& OutRequestId) const
{
	OutMovement = FEnemyPatternMovementSettings();
	OutTarget = nullptr;
	OutRequestId = 0;
	if (!IsRequestRunning(ActiveRequestId))
	{
		return false;
	}
	if (!bRecovering)
	{
		OutMovement = ActivePattern.Movement;
	}
	OutTarget = ActiveTarget.Get();
	OutRequestId = ActiveRequestId;
	return true;
}

// 지연된 판정이 현재 유효한 요청인지 번호와 실행 상태로 확인한다.
bool UEnemyPatternComponent::IsRequestRunning(int64 RequestId) const
{
	return !bStopped
		&& RequestId != 0 && RequestId == ActiveRequestId
		// 현재 요청의 실행 결과
		&& ActiveResult == EEnemyPatternResult::Running;
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

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ExecutionTimeoutHandle);
		GetWorld()->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	}

	UnbindAbilityEnded();

	if (ASC.IsValid())
	{
		for (const auto& PatternPair : PatternHandles)
		{
			ASC->CancelAbilityHandle(PatternPair.Value);
		}
	}

	ActiveTarget.Reset();
}

// 플레이 종료 시 실행, 구독 및 직접 소유한 자원 정리 후 부모 호출
void UEnemyPatternComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAndInvalidate();
	ClearGrantedAbilities();

	Super::EndPlay(EndPlayReason);
}

// DataAsset에서 ID가 일치하는 설정의 주소 조회
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
	
	// Enemy 또는 Target 또는 사용 가능한 패턴 유효성 검사
	if (!IsValid(Enemy) || !Enemy->IsAlive()
		|| !AvailablePatterns.Contains(Pattern.PatternId))
		return false;

	if (Pattern.bLogOnly)
	{
		return true;
	}

	// 타깃, 거리, 시야 조건을 모두 사용하지 않는 아레나 패턴은 타깃이 없어도 실행 가능
	const bool bNeedsTarget = Pattern.bRequiresTarget
		|| Pattern.bCheckTargetDistance || Pattern.bRequiresLineOfSight;
	if (!bNeedsTarget)
	{
		return true;
	}
	if (!IsValid(Target) || Target == Enemy)
	{
		return false;
	}

	// Player는 현재 TargetableInterface 대신 사망 상태 Getter를 사용
	if (const APlayerCharacter* Player = Cast<APlayerCharacter>(Target))
	{
		if (Player->GetbIsDead())
		{
			return false;
		}
	}
	else if (Target->Implements<UTargetableInterface>())
	{
		if (!ITargetableInterface::Execute_IsTargetAlive(Target))
		{
			return false;
		}
	}
	else return false;

	if (!Pattern.bCheckTargetDistance)
	{
		return true;
	}
	const FVector DeltaLocation = Target->GetActorLocation() - Enemy->GetActorLocation();
	const float DistanceSquared = DeltaLocation.SizeSquared2D();

	return DistanceSquared >= FMath::Square(Pattern.MinRange)
		&& DistanceSquared <= FMath::Square(Pattern.MaxRange)
		&& FMath::Abs(DeltaLocation.Z) <= Pattern.MaxHeightDifference;
}

// 거리 조건에 행동 가능 여부, 시야 및 GAS 활성화 조건을 추가 검사
bool UEnemyPatternComponent::CanExecutePattern(const FEnemyPatternDefinition& Pattern, AActor* Target) const
{
	if (!MatchesGeometry(Pattern, Target)
		|| !ASC.IsValid())
	{
		return false;
	}

	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	
	if (!Enemy->CanRunEnemyAI())
		return false;

	if (Pattern.bLogOnly)
	{
		return true;
	}

	if (Pattern.bRequiresLineOfSight)
	{
		const AAIController* AI = Cast<AAIController>(Enemy->GetController());
		
		if (!IsValid(Target) || !AI || !AI->LineOfSightTo(Target))
			return false;
	}

	const FGameplayAbilitySpecHandle* Handle = PatternHandles.Find(Pattern.PatternId);
	// 능력 또는 효과의 실행 스펙
	const FGameplayAbilitySpec* Spec = Handle
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
		return;

	GetWorld()->GetTimerManager().ClearTimer(ExecutionTimeoutHandle);

	UnbindAbilityEnded();
	if (EndedData.bWasCancelled)
	{
		ActiveResult = EEnemyPatternResult::Cancelled;
		return;
	}

	const float RecoverySeconds = ActivePattern.RecoverySeconds;
	if (RecoverySeconds > 0.f)
	{
		StartRecovery(RecoverySeconds);
		return;
	}
	HandleRecoveryFinished();
}

void UEnemyPatternComponent::StartRecovery(float Duration)
{
	bRecovering = true;
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		Enemy->AcquireAttackMovementLock();
		bOwnsRecoveryMovementLock = true;
		Enemy->GetCharacterMovement()->StopMovementImmediately();
		Enemy->ConsumeMovementInputVector();
	}
	GetWorld()->GetTimerManager().SetTimer(RecoveryTimerHandle, this,
		&UEnemyPatternComponent::HandleRecoveryFinished, Duration, false);
	// 알림 중 BT가 요청을 취소해도 후딜 타이머를 다시 만들지 않도록 마지막에 통지한다.
	OnRecoveryStarted.Broadcast();
}

void UEnemyPatternComponent::ReleaseRecoveryMovementLock()
{
	if (!bOwnsRecoveryMovementLock)
	{
		return;
	}
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		// 다른 어빌리티나 상태가 보유한 이동 제한까지 해제하지 않는다.
		Enemy->ReleaseAttackMovementLock();
	}
	bOwnsRecoveryMovementLock = false;
}

void UEnemyPatternComponent::HandleRecoveryFinished()
{
	ReleaseRecoveryMovementLock();
	bRecovering = false;
	if (IsRequestRunning(ActiveRequestId))
	{
		// 정상 완료 한 번에만 추첨한다. 요청 해제나 BT 조건 조회는 이 시각을 변경하지 않는다.
		const float Interval = FMath::FRandRange(MinAttackInterval, MaxAttackInterval);
		NextAttackAllowedTime = GetWorld()->GetTimeSeconds() + static_cast<double>(Interval);
		ActiveResult = EEnemyPatternResult::Completed;
	}
}

// 종료 이벤트 누락을 실패로 기록하고 해당 능력 취소
void UEnemyPatternComponent::HandleExecutionTimeout()
{
	if (ActiveResult != EEnemyPatternResult::Running)
		return;

	ActiveResult = EEnemyPatternResult::Failed;
	UnbindAbilityEnded();

	if (ASC.IsValid())
	{
		ASC->CancelAbilityHandle(ActiveAbilityHandle);
	}
}

// 현재 요청의 ASC 종료 이벤트 구독만 해제
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
		// 공유 핸들을 한 번만 제거하기 위한 집합
		TSet<FGameplayAbilitySpecHandle> UniqueHandles;

		for (const auto& PatternPair : PatternHandles)
		{
			UniqueHandles.Add(PatternPair.Value);
		}

		for (FGameplayAbilitySpecHandle Handle : UniqueHandles)
		{
			ASC->ClearAbility(Handle);
		}
	}

	LoggedPlaceholderPatterns.Reset();
	PatternHandles.Reset();
	AvailablePatterns.Reset();
}
