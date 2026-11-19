// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/EnemyLog.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

#include "Enemy/Boss/BossAIController.h"
#include "Enemy/Boss/BossPhaseComponent.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/Interfaces/BossPhaseGimmickInterface.h"

ABossCharacter::ABossCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 보스 전용 컨트롤러가 Blackboard와 Focus를 담당 (BP에서 다른 컨트롤러로 바꿀 수 있음)
	AIControllerClass = ABossAIController::StaticClass();

	// 서브오브젝트 이름은 기존과 같게 유지: BP에 저장된 컴포넌트 설정을 그대로 이어받음
	PhaseComponent = CreateDefaultSubobject<UBossPhaseComponent>(TEXT("PhaseComponent"));
}

/* AEnemyCharacter */

bool ABossCharacter::CanRunEnemyAI() const
{
	return Super::CanRunEnemyAI() && IsEncounterActive();
}

float ABossCharacter::CalculateHealthAfterDamage(float CurrentHealth, float MaxHealth, float Damage) const
{
	// 전투 전, 전환 중, 실패 예약 중에는 피해 무시
	if (!IsAlive() || !IsEncounterActive() || !PhaseComponent->IsActive())
	{
		return CurrentHealth;
	}

	// 이미 하한보다 낮은 경우 체력을 올리지 않도록 현재 체력과 비교
	const float EffectiveFloor = FMath::Min(CurrentHealth, PhaseComponent->GetDamageHealthFloor(MaxHealth));

	return FMath::Clamp(CurrentHealth - FMath::Max(0.f, Damage), EffectiveFloor, CurrentHealth);
}

void ABossCharacter::FinishDeath()
{
	if (LifeState != EEnemyLifeState::Dying)
	{
		return;
	}

	Super::FinishDeath();
	OnDeathFinished.Broadcast(this);
}

/* 보스방 전투 */

// 매니저의 지연 스폰 중 호출: 실제 전달은 기믹 컴포넌트 연결 시 수행
void ABossCharacter::SetLevelGimmickReferences(const TMap<FName, TObjectPtr<AActor>>& PhaseActors, AActor* AreaCenter)
{
	LevelGimmickActors = PhaseActors;
	GimmickAreaCenter = AreaCenter;
}

void ABossCharacter::SetLevelMovementRoutes(const TMap<FName, FBossMovementRoute>& InRoutes)
{
	PhaseComponent->SetMovementRoutes(InRoutes);
}

void ABossCharacter::SetPhaseTransitionHandler(UObject* Handler)
{
	PhaseComponent->SetTransitionHandler(Handler);
}

// 준비된 보스방의 첫 페이즈와 AI 시작
bool ABossCharacter::StartEncounter()
{
	if (!HasAuthority() || !IsEnemyInitialized() || !IsAlive() || bEncounterActive)
	{
		return false;
	}

	++EncounterGeneration;
	bFailurePending = false;
	bEncounterActive = true;

	GetPatternComponent()->ResetAttackInterval();

	if (!PhaseComponent->StartPhases())
	{
		StopEncounter();
		return false;
	}

	FinishPhaseActivation();

	// 전투가 시작되어 CanRunEnemyAI가 true가 된 뒤 BT 시작
	if (ABossAIController* AI = GetBossAIController())
	{
		AI->TryStartEnemyAI();
	}

	NotifyCombatStateChanged();
	return true;
}

// 세션을 먼저 무효화한 뒤 전투 전체 실행과 기믹 정리
void ABossCharacter::StopEncounter()
{
	bEncounterActive = false;
	bFailurePending = false;
	++EncounterGeneration;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FailureTimerHandle);
	}

	PhaseComponent->StopAndInvalidate();
	GetPatternComponent()->StopAndInvalidate();
	StopPhaseGimmicks();

	if (ABossAIController* AI = GetBossAIController())
	{
		AI->StopBossAI(TEXT("Boss encounter stopped"));
	}

	GetCharacterMovement()->StopMovementImmediately();
	SetWantsToMove(false);

	NotifyCombatStateChanged();
}

// 안전한 다음 Tick에 전투 실패를 전달하도록 예약
void ABossCharacter::ScheduleEncounterFailure()
{
	if (!bEncounterActive || bFailurePending)
	{
		return;
	}

	bFailurePending = true;

	// 실패가 확정된 순간부터 공격, 전환, 기믹이 더 진행하지 않도록 즉시 정리
	PhaseComponent->StopAndInvalidate();
	GetPatternComponent()->SetRequestsBlocked(true);
	GetPatternComponent()->CancelAllRequests();
	StopPhaseGimmicks();

	// 이 예약 이후 보스가 사망 또는 초기화되면 실행하지 않음
	const int64 ExpectedGeneration = EncounterGeneration;
	FailureTimerHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this, ExpectedGeneration]()
		{
			if (!bEncounterActive || ExpectedGeneration != EncounterGeneration)
			{
				return;
			}

			StopEncounter();
			OnEncounterFailed.Broadcast(this);
		}));
}

/* 페이즈 전환 */

bool ABossCharacter::BeginPhaseTransition()
{
	return HasAuthority() && IsEncounterActive() && PhaseComponent->BeginPendingTransition();
}

bool ABossCharacter::NotifyPhaseTransitionPresentationFinished(int64 TransitionId)
{
	return HasAuthority() && IsEncounterActive() && PhaseComponent->NotifyTransitionPresentationFinished(TransitionId);
}

int64 ABossCharacter::GetPendingTransitionId() const
{
	return PhaseComponent->GetActiveTransitionId();
}

/* 기믹 */

// 연결된 컴포넌트와 현재 전투에서 발생한 완료 통지만 수락
bool ABossCharacter::NotifyPhaseGimmickCompleted(UActorComponent* SourceComponent, FName PhaseId, int64 PhaseExecutionId)
{
	if (!HasAuthority() || !IsEncounterActive() || !IsAlive()
		|| !IsValid(PhaseGimmickComponent) || SourceComponent != PhaseGimmickComponent
		|| SourceComponent->GetOwner() != this)
	{
		return false;
	}

	return PhaseComponent->RequestNextPhaseFromGimmick(PhaseId, PhaseExecutionId);
}

/* 상태 조회 */

bool ABossCharacter::CanBossAct() const
{
	return IsEncounterActive() && IsAlive() && PhaseComponent->IsActive();
}

bool ABossCharacter::IsPhaseTransitionInProgress() const
{
	return IsEncounterActive() && (PhaseComponent->HasPendingTransition() || PhaseComponent->IsTransitioning());
}

FName ABossCharacter::GetCurrentPhaseId() const
{
	return PhaseComponent->GetCurrentPhaseId();
}

const FBossPhaseDefinition* ABossCharacter::GetCurrentPhaseDefinition() const
{
	return PhaseComponent->GetCurrentPhase();
}

/* AEnemyCharacter 확장 */

// 기본 Enemy 초기화 후 페이즈 설정, 패턴 ID, 기믹 연결 검사
bool ABossCharacter::InitializeEnemyExtensions()
{
	if (!Super::InitializeEnemyExtensions() || !PhaseConfig || !GetPatternComponent()->HasPatterns())
	{
		UE_LOG(LogEnemy, Error, TEXT("[Boss] %s: PhaseConfig or PatternSet is missing"), *GetName());
		return false;
	}

	if (!ValidatePhasePatterns() || !InitializeGimmickConnection())
	{
		return false;
	}

	// 페이즈 컴포넌트 이벤트 연결
	PhaseComponent->OnTransitionRequested.AddUObject(this, &ABossCharacter::HandleTransitionRequested);
	PhaseComponent->OnTransitionFinished.AddUObject(this, &ABossCharacter::HandleTransitionFinished);
	PhaseComponent->ApplyPhase.BindUObject(this, &ABossCharacter::HandlePhaseEntered);

	// 전투 시작 전에는 공격 요청 차단
	GetPatternComponent()->SetRequestsBlocked(true);

	return PhaseComponent->InitializePhases(PhaseConfig);
}

// 체력 변경 시 사망 판정을 먼저 수행한 뒤 추가 상태 처리
void ABossCharacter::HandleHealthChanged(float NewHealth, float MaxHealth)
{
	Super::HandleHealthChanged(NewHealth, MaxHealth);

	if (IsAlive() && IsEncounterActive())
	{
		PhaseComponent->EvaluateHealth(NewHealth, MaxHealth);
	}
}

// 중복 사망을 막고 전투를 정리한 뒤 사망 연출 시작
void ABossCharacter::BeginDeath()
{
	if (!IsAlive())
	{
		return;
	}

	StopEncounter();
	Super::BeginDeath();
	OnDefeated.Broadcast(this);
}

// 플레이 종료 시 실행, 구독 및 직접 소유한 자원 정리 후 부모 호출
void ABossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopEncounter();

	PhaseComponent->OnTransitionRequested.RemoveAll(this);
	PhaseComponent->OnTransitionFinished.RemoveAll(this);
	PhaseComponent->ApplyPhase.Unbind();
	OnPhaseTransitionResult.Clear();

	Super::EndPlay(EndPlayReason);
}

/* 페이즈 컴포넌트 콜백 */

// 전환 요청 즉시 공격 및 피해 차단, 기존 공격과 기믹 정리
void ABossCharacter::HandleTransitionRequested()
{
	if (!IsEncounterActive() || !IsAlive())
	{
		return;
	}

	GetPatternComponent()->SetRequestsBlocked(true);
	// 주 패턴뿐 아니라 보조 패턴과 시작 지연 예약까지 취소 (전환 Task는 바로 IsIdle을 확인하므로 취소는 즉시 끝나야 함)
	GetPatternComponent()->CancelAllRequests();

	// 이전 페이즈의 공격 간격을 새 페이즈 첫 공격에 넘기지 않음
	GetPatternComponent()->ResetAttackInterval();

	StopPhaseGimmicks();

	if (ABossAIController* AI = GetBossAIController())
	{
		AI->PrepareForPhaseTransition();
	}

	NotifyCombatStateChanged();
}

// 페이즈별 공격 목록 및 기믹 적용
bool ABossCharacter::HandlePhaseEntered(const FBossPhaseDefinition& Phase)
{
	if (!IsEncounterActive())
	{
		return false;
	}

	// 패턴 목록, 가중치 재정의, 동시 실행 설정을 한 번에 교체
	GetPatternComponent()->SetPatternSelection(Phase.Patterns, Phase.Concurrency);

	// 랜덤 선택과 별개로 페이즈 내내 굴러가는 상시 패턴 (전환 요청 때 CancelAllRequests로 정지)
	GetPatternComponent()->SetPersistentPatterns(Phase.PersistentPatterns);

	if (IsValid(PhaseGimmickComponent))
	{
		return IBossPhaseGimmickInterface::Execute_EnterBossPhase(PhaseGimmickComponent,
			Phase.PhaseId, PhaseComponent->GetCurrentPhaseExecutionId());
	}

	// 기믹 구현이 빠진 배리어는 완료될 수 없으므로 진행하지 않음
	if (Phase.Kind == EBossPhaseKind::Barrier)
	{
		UE_LOG(LogEnemy, Error, TEXT("[Boss] %s: missing phase gimmick component for barrier '%s'"),
			*GetName(), *Phase.PhaseId.ToString());
		return false;
	}

	return true;
}

void ABossCharacter::HandleTransitionFinished(int64 TransitionId, bool bSucceeded)
{
	if (bSucceeded)
	{
		FinishPhaseActivation();
	}
	else
	{
		ScheduleEncounterFailure();
	}

	OnPhaseTransitionResult.Broadcast(TransitionId, bSucceeded);
}

/* 내부 처리 */

// 첫 페이즈 시작과 각 페이즈 전환 완료 시 실행
void ABossCharacter::FinishPhaseActivation()
{
	// 진입 직후 다음 전환이 이미 요청된 경우(즉시 완료된 기믹)는 공격 차단 유지
	if (!IsEncounterActive() || !PhaseComponent->IsActive())
	{
		return;
	}

	const FBossPhaseDefinition* Phase = PhaseComponent->GetCurrentPhase();
	if (!Phase)
	{
		return;
	}

	// Hold 기준 방향 저장 전에 위치부터 맞춤
	TeleportToFirstWaypoint(*Phase);

	// 전환 연출 후 현재 Actor의 수평 방향을 이번 페이즈의 기준으로 저장
	if (ABossAIController* AI = GetBossAIController())
	{
		AI->ApplyPhaseRotation(Phase->RotationMode == EBossPhaseRotationMode::Hold, GetActorRotation().Yaw);
	}

	GetPatternComponent()->SetRequestsBlocked(false);
	NotifyCombatStateChanged();

	// 새 페이즈가 활성화된 뒤 레벨 BP에 알림
	OnPhaseStarted.Broadcast(this, Phase->PhaseId);
}

// 설정의 패턴 ID가 PatternSet에 있는지, 추격 반경에서 쓸 수 있는 패턴이 있는지 검사
bool ABossCharacter::ValidatePhasePatterns() const
{
	const UEnemyPatternComponent* Patterns = GetPatternComponent();

	for (const FBossPhaseDefinition& Phase : PhaseConfig->Phases)
	{
		// 허용 목록의 모든 패턴이 PatternSet에 있어야 함
		for (const FEnemyPatternSelectionEntry& Entry : Phase.Patterns)
		{
			if (!Patterns->HasPattern(Entry.PatternId))
			{
				UE_LOG(LogEnemy, Error, TEXT("[Boss] %s: phase '%s' uses unknown pattern '%s'"),
					*GetName(), *Phase.PhaseId.ToString(), *Entry.PatternId.ToString());
				return false;
			}
		}

		// 상시 패턴도 PatternSet에 있어야 함
		for (const FEnemyPersistentPatternEntry& Entry : Phase.PersistentPatterns)
		{
			if (!Patterns->HasPattern(Entry.PatternId))
			{
				UE_LOG(LogEnemy, Error, TEXT("[Boss] %s: phase '%s' uses unknown persistent pattern '%s'"),
					*GetName(), *Phase.PhaseId.ToString(), *Entry.PatternId.ToString());
				return false;
			}
		}

		// 기본 Chasing의 도착 반경이 자동 선택 패턴의 거리 범위와 맞는지 경고만 출력
		// - 높이, 시야, GA 활성화 조건 및 패턴별 이동 예외는 이 검사에 포함하지 않음
		if (Phase.MovementMode != EBossPhaseMovementMode::Chasing)
		{
			continue;
		}

		const float Radius = Phase.MovementAcceptanceRadius;
		// 이 페이즈의 유효 가중치(재정의 포함)로 자동 선택 대상인지 판단
		const bool bUsableAtArrival = Phase.Patterns.ContainsByPredicate(
			[Patterns, Radius](const FEnemyPatternSelectionEntry& Entry)
			{
				FEnemyPatternDefinition Pattern;
				return Patterns->GetPatternDefinition(Entry.PatternId, Pattern) && Entry.ResolveWeight(Pattern) > 0.f
					&& (!Pattern.bCheckTargetDistance || (Pattern.MinRange <= Radius && Radius < Pattern.MaxRange));
			});

		if (!bUsableAtArrival)
		{
			UE_LOG(LogEnemy, Warning, TEXT("[Boss] Phase '%s': no auto pattern usable at chase acceptance radius %.0f (check MinRange/MaxRange)"),
				*Phase.PhaseId.ToString(), Radius);
		}
	}

	return true;
}

// 다른 담당자의 실제 기믹 타입은 참조하지 않고 보스 소유 컴포넌트에서 계약 구현 검색
bool ABossCharacter::InitializeGimmickConnection()
{
	PhaseGimmickComponent = nullptr;

	TInlineComponentArray<UActorComponent*> Components;
	GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (!IsValid(Component) || !Component->Implements<UBossPhaseGimmickInterface>())
		{
			continue;
		}

		if (PhaseGimmickComponent)
		{
			UE_LOG(LogEnemy, Error, TEXT("[Boss] %s requires at most one phase gimmick component"), *GetName());
			return false;
		}

		PhaseGimmickComponent = Component;
	}

	// BP 컴포넌트는 FinishSpawning에서 생성되므로 연결 직후 매니저가 넘긴 참조 전달
	if (PhaseGimmickComponent)
	{
		TMap<FName, AActor*> PhaseActors;
		for (const TPair<FName, TObjectPtr<AActor>>& Pair : LevelGimmickActors)
		{
			PhaseActors.Add(Pair.Key, Pair.Value);
		}

		IBossPhaseGimmickInterface::Execute_ReceiveLevelGimmickReferences(PhaseGimmickComponent, PhaseActors, GimmickAreaCenter);
	}

	return true;
}

// 정지 상태를 무효화한 뒤 호출하므로 정리 중 발생한 완료 알림도 페이즈를 진행시키지 못함
void ABossCharacter::StopPhaseGimmicks()
{
	if (HasAuthority() && IsValid(PhaseGimmickComponent))
	{
		IBossPhaseGimmickInterface::Execute_StopBossGimmicks(PhaseGimmickComponent);
	}
}

void ABossCharacter::TeleportToFirstWaypoint(const FBossPhaseDefinition& Phase)
{
	if (!Phase.bTeleportToFirstWaypoint || Phase.MovementMode != EBossPhaseMovementMode::Waypoints)
	{
		return;
	}

	// 페이즈 적용 시 순회 인덱스가 0으로 초기화되므로 첫 지점
	FVector Point;
	if (!PhaseComponent->GetCurrentWaypoint(Point))
	{
		UE_LOG(LogEnemy, Warning, TEXT("[Boss] %s: no first waypoint for '%s', route '%s'. Teleport skipped."),
			*GetName(), *Phase.PhaseId.ToString(), *Phase.MovementRouteId.ToString());
		return;
	}

	if (AController* OwnerController = GetController())
	{
		OwnerController->StopMovement();
	}
	GetCharacterMovement()->StopMovementImmediately();

	// TargetPoint는 바닥에 놓이므로 캡슐 절반 높이만큼 올림
	const FVector Location = Point + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
}

void ABossCharacter::NotifyCombatStateChanged()
{
	if (ABossAIController* AI = GetBossAIController())
	{
		AI->RefreshBossBlackboard();
	}
}

ABossAIController* ABossCharacter::GetBossAIController() const
{
	return Cast<ABossAIController>(GetController());
}
