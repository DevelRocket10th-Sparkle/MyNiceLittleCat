// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Boss/Components/BossPhaseComponent.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/AI/EnemyAIController.h"
#include "AbilitySystemComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"


ABossCharacter::ABossCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = AEnemyAIController::StaticClass();
	PhaseComponent = CreateDefaultSubobject<UBossPhaseComponent>(TEXT("PhaseComponent"));
}

// 초기화와 생존 및 보스방 전투 상태를 포함한 AI 실행 가능 여부 확인
bool ABossCharacter::CanRunEnemyAI() const
{
	//return Super::CanRunEnemyAI() && bEncounterActive;
	return Super::CanRunEnemyAI() && IsEncounterActive() && !PhaseComponent->IsCurrentPhaseLogOnly();
}

// Health 변경 전에 무적 및 페이즈 체력 하한을 반영
float ABossCharacter::CalculateHealthAfterDamage(float CurrentHealth, float MaxHealth, float Damage) const
{
	//// 초기화 중, 전투 대기 중, 사망 중에는 피해 차단
	//if (!IsAlive() || !bEncounterActive)
	//{
	//	return CurrentHealth;
	//}

	//if (IsDamageBlocked())
	//{
	//	return CurrentHealth;
	//}

	//const float HealthFloor = PhaseComponent->GetDamageHealthFloor(MaxHealth);

	//return 0.0f;
	if (!IsAlive() || !IsEncounterActive() || !PhaseComponent->IsActive()) return CurrentHealth;
	// 이미 하한보다 낮은 경우 체력을 올리지 않도록 현재 체력과 비교
	const float EffectiveFloor = FMath::Min(CurrentHealth, PhaseComponent->GetDamageHealthFloor(MaxHealth));
	return FMath::Clamp(CurrentHealth - FMath::Max(0.f, Damage), EffectiveFloor, CurrentHealth);
}
// 기본 초기화 뒤 파생 캐릭터의 추가 설정 준비
bool ABossCharacter::InitializeEnemyExtensions()
{
	if (!PhaseConfig || !GetPatternComponent()->HasPatterns()) return false;
	for (int32 Index = 0; Index < PhaseConfig->Phases.Num(); ++Index)
	{
		if (PhaseConfig->IsLogOnlyPhase(Index)) continue;
		const FBossPhaseDefinition& Phase = PhaseConfig->Phases[Index];
		for (FName PatternId : Phase.AllowedPatternIds)
			if (!GetPatternComponent()->HasPattern(PatternId)) return false;

	}
	PhaseComponent->OnTransitionRequested.AddUObject(this, &ABossCharacter::HandleTransitionRequested);
	PhaseComponent->ApplyPhase.BindUObject(this, &ABossCharacter::HandlePhaseEntered);
	GetPatternComponent()->SetRequestsBlocked(true);
	return PhaseComponent->InitializePhases(ASC, PhaseConfig);
}



// 준비된 보스방의 첫 페이즈와 타깃 선정 시작
bool ABossCharacter::StartEncounter()
{
	if (!HasAuthority() || !IsEnemyInitialized() || !IsAlive() || bEncounterActive) return false;
	++EncounterGeneration;
	bFailurePending = false;
	bEncounterActive = true;
	if (!PhaseComponent->StartPhases()) { StopEncounter(); return false; }
	FinishPhaseTransition();
	if (AEnemyAIController* AI = Cast<AEnemyAIController>(GetController())) AI->TryStartEnemyAI();
	RefreshBossBlackboard();
	return true;
}



void ABossCharacter::HandleTransitionRequested()
{
	if (!IsEncounterActive() || !IsAlive()) return;
	GetPatternComponent()->SetRequestsBlocked(true);
	GetPatternComponent()->CancelActiveRequest();
	if (AEnemyAIController* AI = Cast<AEnemyAIController>(GetController())) AI->StopMovement();
	RefreshBossBlackboard();
}



// 페이즈별 공격 목록 및 배리어와 코인 기믹 적용
bool ABossCharacter::HandlePhaseEntered(const FBossPhaseDefinition& Phase)
{
	if (!IsEncounterActive()) return false;
	GetPatternComponent()->SetAvailablePatterns(Phase.AllowedPatternIds);
	return true;
}

void ABossCharacter::FinishPhaseTransition()
{
	if (!IsEncounterActive() || !PhaseComponent->IsActive()) return;
	// 마지막 로그 페이즈까지 통과했으면 이전 공격 목록을 재사용하지 않음
	if (PhaseComponent->IsCurrentPhaseLogOnly()) GetPatternComponent()->SetAvailablePatterns({});
	GetPatternComponent()->SetRequestsBlocked(PhaseComponent->IsCurrentPhaseLogOnly());
	RefreshBossBlackboard();
}



// 체력 변경 시 사망 판정을 먼저 수행한 뒤 추가 상태 처리
void ABossCharacter::HandleHealthChanged(float NewHealth, float MaxHealth)
{
	Super::HandleHealthChanged(NewHealth, MaxHealth);
	if (IsAlive() && IsEncounterActive()) PhaseComponent->EvaluateHealth(NewHealth, MaxHealth);
}

// 현재 보스 상태를 일반 행동 및 전환 판단 키에 반영
void ABossCharacter::RefreshBossBlackboard()
{
	// 보스의 AI와 BT 판단 데이터
	AEnemyAIController* AI = Cast<AEnemyAIController>(GetController());
	UBlackboardComponent* Blackboard = AI ? AI->GetBlackboardComponent() : nullptr;
	if (!Blackboard) return;
	Blackboard->SetValueAsBool(TEXT("CanAct"), IsEncounterActive() && IsAlive() && PhaseComponent->IsActive() && !PhaseComponent->IsCurrentPhaseLogOnly());
	if (Blackboard->GetKeyID(TEXT("PhaseTransitionRequested")) != FBlackboard::InvalidKey)
		Blackboard->SetValueAsBool(TEXT("PhaseTransitionRequested"), IsEncounterActive()
			&& (PhaseComponent->HasPendingTransition() || PhaseComponent->IsTransitioning()));
}

// 안전한 다음 Tick에 전투 실패를 전달하도록 예약
void ABossCharacter::ScheduleEncounterFailure()
{
	if (!bEncounterActive || bFailurePending) return;
	bFailurePending = true;
	PhaseComponent->StopAndInvalidate();
	GetPatternComponent()->SetRequestsBlocked(true);
	GetPatternComponent()->CancelActiveRequest();
	// 이 예약 이후 보스가 사망 또는 초기화되면 실행하지 않음
	const int64 ExpectedGeneration = EncounterGeneration;
	FailureTimerHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this, ExpectedGeneration]()
		{
			if (!bEncounterActive || ExpectedGeneration != EncounterGeneration) return;
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
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(FailureTimerHandle);
	PhaseComponent->StopAndInvalidate();
	GetPatternComponent()->StopAndInvalidate();
	if (AEnemyAIController* AI = Cast<AEnemyAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AI->GetBrainComponent()) Brain->StopLogic(TEXT("Boss encounter stopped"));
		AI->StopMovement();
		AI->ClearCombatTarget();
	}
	RefreshBossBlackboard();
}

// 중복 사망을 막고 공격과 AI를 정리한 뒤 사망 연출 시작
void ABossCharacter::BeginDeath()
{
	if (!IsAlive()) return;
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
