// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Interfaces/BossPhaseGimmickInterface.h"
#include "Enemy/Components/EnemyPhaseComponent.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/AI/EnemyAIController.h"
#include "AbilitySystemComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"


ABossCharacter::ABossCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = AEnemyAIController::StaticClass();
	PhaseComponent = CreateDefaultSubobject<UEnemyPhaseComponent>(TEXT("PhaseComponent"));
}

// 초기화와 생존 및 보스방 전투 상태를 포함한 AI 실행 가능 여부 확인
bool ABossCharacter::CanRunEnemyAI() const
{
	return Super::CanRunEnemyAI() && IsEncounterActive() && !PhaseComponent->IsCurrentPhaseLogOnly();
}

// Health 변경 전에 무적 및 페이즈 체력 하한을 반영
float ABossCharacter::CalculateHealthAfterDamage(float CurrentHealth, float MaxHealth, float Damage) const
{
	if (!IsAlive() || !IsEncounterActive() || !PhaseComponent->IsActive()
		|| PhaseComponent->IsCurrentPhaseLogOnly()) return CurrentHealth;

	// 이미 하한보다 낮은 경우 체력을 올리지 않도록 현재 체력과 비교
	const float EffectiveFloor = FMath::Min(CurrentHealth, PhaseComponent->GetDamageHealthFloor(MaxHealth));

	return FMath::Clamp(CurrentHealth - FMath::Max(0.f, Damage), EffectiveFloor, CurrentHealth);
}
void ABossCharacter::SetLevelMovementRoutes(const TMap<FName, FBossMovementRoute>& InRoutes)
{
	PhaseComponent->SetMovementRoutes(InRoutes);
}

// 기본 초기화 뒤 파생 캐릭터의 추가 설정 준비
bool ABossCharacter::InitializeEnemyExtensions()
{
	if (!PhaseConfig || !GetPatternComponent()->HasPatterns())
	{
		return false;
	}
	for (int32 Index = 0; Index < PhaseConfig->Phases.Num(); ++Index)
	{
		if (PhaseConfig->IsLogOnlyPhase(Index))
		{
			continue;
		}
		const FBossPhaseDefinition& Phase = PhaseConfig->Phases[Index];
		for (FName PatternId : Phase.AllowedPatternIds)
			if (!GetPatternComponent()->HasPattern(PatternId))
			{
				return false;
			}

	}
	if (!InitializeGimmickConnection())
	{
		return false;
	}
	PhaseComponent->OnTransitionRequested.AddUObject(this, &ABossCharacter::HandleTransitionRequested);
	PhaseComponent->ApplyPhase.BindUObject(this, &ABossCharacter::HandlePhaseEntered);
	GetPatternComponent()->SetRequestsBlocked(true);
	return PhaseComponent->InitializePhases(ASC, PhaseConfig);
}



// 준비된 보스방의 첫 페이즈와 타깃 선정 시작
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
	FinishPhaseTransition();
	if (AEnemyAIController* AI = Cast<AEnemyAIController>(GetController()))
	{
		AI->TryStartEnemyAI();
	}
	RefreshBossBlackboard();
	return true;
}



void ABossCharacter::HandleTransitionRequested()
{
	if (!IsEncounterActive() || !IsAlive())
	{
		return;
	}


	GetPatternComponent()->SetRequestsBlocked(true);
	GetPatternComponent()->CancelActiveRequest();
	
	// 이전 페이즈의 대기를 새 페이즈 첫 공격에 넘기지 않는다.
	GetPatternComponent()->ResetAttackInterval();

	StopPhaseGimmicks();

	if (AEnemyAIController* AI = Cast<AEnemyAIController>(GetController()))
	{
		AI->StopMovement();

		// 이전 페이즈의 Hold Focus와 공격 정렬 Focus 해제
		// 기본 플레이어 Gameplay Focus는 유지
		AI->ClearPhaseHold();
		AI->ClearFocus(EnemyFocusPriority::AttackFacing);
	}

	RefreshBossBlackboard();
}



// 페이즈별 공격 목록 및 배리어와 코인 기믹 적용
bool ABossCharacter::HandlePhaseEntered(const FBossPhaseDefinition& Phase)
{
	if (!IsEncounterActive())
	{
		return false;
	}
	GetPatternComponent()->SetAvailablePatterns(Phase.AllowedPatternIds);
	if (IsValid(PhaseGimmickComponent))
		return IBossPhaseGimmickInterface::Execute_EnterBossPhase(PhaseGimmickComponent,
			Phase.PhaseId, PhaseComponent->GetCurrentPhaseExecutionId());
	// 기믹 구현이 빠진 배리어를 완료된 것처럼 진행하지 않음
	if (Phase.Kind == EBossPhaseKind::Barrier)
	{
		UE_LOG(LogTemp, Error, TEXT("Missing phase gimmick component for %s"), *Phase.PhaseId.ToString());
		return false;
	}
	return true;
}

void ABossCharacter::FinishPhaseTransition()
{
	if (!IsEncounterActive() || !PhaseComponent->IsActive())
	{
		return;
	}

	const bool bLogOnly = PhaseComponent->IsCurrentPhaseLogOnly();
	
	// 로그 전용 페이즈에서 이전 페이즈의 공격 목록이 남지 않도록 비움
	if (bLogOnly)
	{
		GetPatternComponent()->SetAvailablePatterns({});
	}

	// 첫 페이즈 시작과 각 페이즈 전환 완료 시 기준 방향 설정
	if (AEnemyAIController* AI = Cast<AEnemyAIController>(GetController()))
	{
		const FBossPhaseDefinition* Phase = PhaseComponent->GetCurrentPhase();

		if (!bLogOnly
			&& Phase && Phase->RotationMode == EBossPhaseRotationMode::Hold)
		{
			// 전환 연출 후 현재 Actor의 수평 방향을 이번 페이즈의 기준으로 저장
			AI->SetPhaseHoldYaw(GetActorRotation().Yaw);
		}
		else
		{
			AI->ClearPhaseHold();
		}
	}

	GetPatternComponent()->SetRequestsBlocked(bLogOnly);
	RefreshBossBlackboard();

	// BP Test용
	// 새 페이즈가 활성화된 뒤 레벨 BP에 알림
	OnPhaseStarted.Broadcast(this, PhaseComponent->GetCurrentPhaseId());
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

// 현재 보스 상태를 일반 행동 및 전환 판단 키에 반영
void ABossCharacter::RefreshBossBlackboard()
{
	// 보스의 AI와 BT 판단 데이터
	AEnemyAIController* AI = Cast<AEnemyAIController>(GetController());
	UBlackboardComponent* Blackboard = AI ? AI->GetBlackboardComponent() : nullptr;
	if (!Blackboard)
	{
		return;
	}
	Blackboard->SetValueAsBool(TEXT("CanAct"), IsEncounterActive() && IsAlive() && PhaseComponent->IsActive() && !PhaseComponent->IsCurrentPhaseLogOnly());
	if (Blackboard->GetKeyID(TEXT("PhaseTransitionRequested")) != FBlackboard::InvalidKey)
		Blackboard->SetValueAsBool(TEXT("PhaseTransitionRequested"), IsEncounterActive()
			&& (PhaseComponent->HasPendingTransition() || PhaseComponent->IsTransitioning()));
}

// 안전한 다음 Tick에 전투 실패를 전달하도록 예약
void ABossCharacter::ScheduleEncounterFailure()
{
	if (!bEncounterActive || bFailurePending)
	{
		return;
	}
	bFailurePending = true;
	PhaseComponent->StopAndInvalidate();
	GetPatternComponent()->SetRequestsBlocked(true);
	GetPatternComponent()->CancelActiveRequest();
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

// 세션을 먼저 무효화한 뒤 전투 전체 실행과 기믹 정리
void ABossCharacter::StopEncounter()
{
	bEncounterActive = false;
	bFailurePending = false;

	++EncounterGeneration;

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FailureTimerHandle);
	}

	PhaseComponent->StopAndInvalidate();
	GetPatternComponent()->StopAndInvalidate();
	StopPhaseGimmicks();

	if (AEnemyAIController* AI = Cast<AEnemyAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AI->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Boss encounter stopped"));
		}

		AI->StopMovement();
		AI->ClearPhaseHold();
		AI->ClearFocus(EnemyFocusPriority::AttackFacing);
		AI->ClearCombatTarget();
	}

	GetCharacterMovement()->StopMovementImmediately();
	SetWantsToMove(false);
	RefreshBossBlackboard();
}

// 중복 사망을 막고 공격과 AI를 정리한 뒤 사망 연출 시작
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
	PhaseComponent->ApplyPhase.Unbind();
	Super::EndPlay(EndPlayReason);
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
			UE_LOG(LogTemp, Error, TEXT("Boss requires at most one phase gimmick component: %s"), *GetName());
			return false;
		}
		PhaseGimmickComponent = Component;
	}
	// BP 컴포넌트는 FinishSpawning에서 생성되므로 연결 직후 매니저가 넘긴 참조 전달
	if (PhaseGimmickComponent)
	{
		TMap<FName, AActor*> PhaseActors;
		for (const auto& Pair : LevelGimmickActors)
		{
			PhaseActors.Add(Pair.Key, Pair.Value);
		}
		IBossPhaseGimmickInterface::Execute_ReceiveLevelGimmickReferences(PhaseGimmickComponent, PhaseActors, GimmickAreaCenter);
	}
	return true;
}

// 매니저의 지연 스폰 중 호출: 실제 전달은 기믹 컴포넌트 연결 시 수행
void ABossCharacter::SetLevelGimmickReferences(const TMap<FName, TObjectPtr<AActor>>& PhaseActors, AActor* AreaCenter)
{
	LevelGimmickActors = PhaseActors;
	GimmickAreaCenter = AreaCenter;
}

// 정지 상태를 무효화한 뒤 호출하므로 정리 중 발생한 완료 알림도 페이즈를 진행시키지 못함
void ABossCharacter::StopPhaseGimmicks()
{
	if (HasAuthority() && IsValid(PhaseGimmickComponent))
		IBossPhaseGimmickInterface::Execute_StopBossGimmicks(PhaseGimmickComponent);
}

// 연결된 컴포넌트와 현재 전투에서 발생한 완료 통지만 수락
bool ABossCharacter::NotifyPhaseGimmickCompleted(UActorComponent* SourceComponent, FName PhaseId, int64 PhaseExecutionId)
{
	if (!HasAuthority() || !IsEncounterActive() || !IsAlive() || !IsValid(PhaseGimmickComponent)
		|| SourceComponent != PhaseGimmickComponent || SourceComponent->GetOwner() != this) return false;
	return PhaseComponent->RequestNextPhaseFromGimmick(PhaseId, PhaseExecutionId);
}
