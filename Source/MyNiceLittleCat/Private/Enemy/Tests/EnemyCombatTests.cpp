#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/Components/EnemyPhaseComponent.h"
#include "Enemy/Boss/Datas/BossPhaseConfigData.h"
#include "Enemy/Datas/EnemyConfigData.h"
#include "Enemy/Datas/EnemyPatternSetData.h"
#include "TimerManager.h"
#include "Enemy/GAS/EnemyAttributeSet.h"
#include "GA_Enemy_RecoveryTest.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Enemy/AI/Tasks/BTTask_Enemy_PatternMovement.h"

namespace EnemyCombatTests
{
	// 외부 맵 및 Blueprint 없이 실행 정책을 검사할 임시 월드
	struct FFixture
	{
		// 테스트 종료 시 직접 제거하는 월드
		UWorld* World = nullptr;
		// 액터를 생성할 수 있는 최소 월드 초기화
		FFixture()
		{
			// CreateWorld 자체가 월드를 초기화하므로 초기화 옵션만 전달
			const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
				.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		}
		// 등록된 액터와 컴포넌트의 EndPlay까지 함께 정리
		~FFixture() { World->DestroyWorld(false); }
	};

	// 에디터에서 지정하는 설정 참조를 테스트에서 동일하게 주입
	void SetObjectSetting(UObject* Owner, FName Name, UObject* Value)
	{
		FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Owner->GetClass(), Name);
		check(Property);
		Property->SetObjectPropertyValue_InContainer(Owner, Value);
	}

	// 타깃 및 거리 없이 실행되는, 자동 선택 가중치가 0인 테스트 패턴 생성
	UEnemyConfigData* MakeEnemyConfig(UObject* Outer)
	{
		UEnemyConfigData* Config = NewObject<UEnemyConfigData>(Outer);
		Config->PatternSet = NewObject<UEnemyPatternSetData>(Config);
		FEnemyPatternDefinition Pattern;
		Pattern.PatternId = TEXT("Arena");
		Pattern.AbilityClass = UGameplayAbility::StaticClass();
		Pattern.Weight = 0.f;
		Pattern.bRequiresTarget = false;
		Pattern.bCheckTargetDistance = false;
		Pattern.bRequiresLineOfSight = false;
		Config->PatternSet->Patterns.Add(Pattern);
		return Config;
	}

	bool FinishTestAbility(UAbilitySystemComponent* ASC)
	{
		FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(UGA_Enemy_RecoveryTest::StaticClass());
		UGA_Enemy_RecoveryTest* Ability = Spec ? Cast<UGA_Enemy_RecoveryTest>(Spec->GetPrimaryInstance()) : nullptr;
		if (!Ability)
		{
			return false;
		}
		Ability->FinishNormally();
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyPatternLifecycleTest, "MyNiceLittleCat.Enemy.PatternLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 가중치 0의 직접 실행, 타깃 없는 패턴, 취소와 해제 및 오래된 ID 무효화 검사
bool FEnemyPatternLifecycleTest::RunTest(const FString& Parameters)
{
	EnemyCombatTests::FFixture Fixture;
	AEnemyCharacter* Enemy = Fixture.World->SpawnActor<AEnemyCharacter>();
	EnemyCombatTests::SetObjectSetting(Enemy, TEXT("EnemyConfig"), EnemyCombatTests::MakeEnemyConfig(Enemy));
	Enemy->DispatchBeginPlay();
	UEnemyPatternComponent* Patterns = Enemy->GetPatternComponent();
	int64 FirstId = 0;
	TestTrue(TEXT("Enemy initialized"), Enemy->IsEnemyInitialized());
	TestTrue(TEXT("Arena pattern needs no target"), Patterns->HasPatternInRange(nullptr));
	TestFalse(TEXT("Zero weight is excluded from random selection"), Patterns->TryStartAttack(nullptr, FirstId));
	TestTrue(TEXT("Explicit zero-weight pattern can start"), Patterns->TryStartPattern(TEXT("Arena"), nullptr, FirstId));
	TestTrue(TEXT("Request is running"), Patterns->IsRequestRunning(FirstId));
	Patterns->CancelRequest(FirstId);
	TestEqual(TEXT("Cancel retains result"), Patterns->GetRequestResult(FirstId), EEnemyPatternResult::Cancelled);
	Patterns->ReleaseRequest(FirstId);
	TestEqual(TEXT("Release invalidates result"), Patterns->GetRequestResult(FirstId), EEnemyPatternResult::Invalid);
	TestTrue(TEXT("Cancelled GA really ended"), Patterns->IsIdle());
	int64 SecondId = 0;
	TestTrue(TEXT("Next request starts"), Patterns->TryStartPattern(TEXT("Arena"), nullptr, SecondId));
	TestTrue(TEXT("IDs increase"), SecondId > FirstId);
	Patterns->ReleaseRequest(FirstId);
	TestTrue(TEXT("Old cleanup cannot cancel new request"), Patterns->IsRequestRunning(SecondId));
	Patterns->ReleaseRequest(SecondId);
	TestTrue(TEXT("Release cancels a running GA"), Patterns->IsIdle());
	Patterns->StopAndInvalidate();
	TestFalse(TEXT("Stopped component rejects new requests"), Patterns->TryStartPattern(TEXT("Arena"), nullptr, SecondId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyPatternRecoveryTest, "MyNiceLittleCat.Enemy.PatternRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// GA 정상 종료와 요청 완료를 분리하고 후딜 취소 후 남은 콜백이 실행되지 않는지 검사한다.
bool FEnemyPatternRecoveryTest::RunTest(const FString& Parameters)
{
	// 동기 테스트 안에서 프레임을 진행하되 다른 테스트와 에디터의 프레임 번호는 보존한다.
	TGuardValue<uint64> RestoreFrameCounter(GFrameCounter, GFrameCounter);
	EnemyCombatTests::FFixture Fixture;
	AEnemyCharacter* Enemy = Fixture.World->SpawnActor<AEnemyCharacter>();
	UEnemyConfigData* Config = EnemyCombatTests::MakeEnemyConfig(Enemy);
	Config->PatternSet->Patterns[0].AbilityClass = UGA_Enemy_RecoveryTest::StaticClass();
	// 전용 실행 설정이 없어도 공통 후딜이 적용되어야 한다.
	Config->PatternSet->Patterns[0].RecoverySeconds = 0.5f;
	EnemyCombatTests::SetObjectSetting(Enemy, TEXT("EnemyConfig"), Config);
	Enemy->DispatchBeginPlay();
	UEnemyPatternComponent* Patterns = Enemy->GetPatternComponent();
	UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	int64 RequestId = 0;
	if (!TestTrue(TEXT("Pattern starts"), Patterns->TryStartPattern(TEXT("Arena"), nullptr, RequestId)))
	{
		return false;
	}
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(UGA_Enemy_RecoveryTest::StaticClass());
	UGA_Enemy_RecoveryTest* Ability = Spec ? Cast<UGA_Enemy_RecoveryTest>(Spec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Recovery test ability has an execution instance"), Ability))
	{
		return false;
	}
	Ability->FinishNormally();
	// 첫 타이머 갱신은 대기 중인 후딜 타이머를 활성화하므로 경과 시간을 더하지 않는다.
	Fixture.World->GetTimerManager().Tick(0.f);
	TestTrue(TEXT("Recovery keeps request running"), Patterns->IsRequestRunning(RequestId));
	TestFalse(TEXT("Recovery is not idle"), Patterns->IsIdle());
	int64 RejectedId = 0;
	TestFalse(TEXT("Recovery blocks next attack"), Patterns->TryStartPattern(TEXT("Arena"), nullptr, RejectedId));
	++GFrameCounter;
	Fixture.World->GetTimerManager().Tick(0.2f);
	TestTrue(TEXT("Recovery waits full duration"), Patterns->IsRequestRunning(RequestId));
	++GFrameCounter;
	Fixture.World->GetTimerManager().Tick(0.4f);
	TestEqual(TEXT("Recovery completes request"), Patterns->GetRequestResult(RequestId), EEnemyPatternResult::Completed);
	Patterns->ReleaseRequest(RequestId);
	if (!TestTrue(TEXT("Next pattern starts"), Patterns->TryStartPattern(TEXT("Arena"), nullptr, RequestId)))
	{
		return false;
	}
	Spec = ASC->FindAbilitySpecFromClass(UGA_Enemy_RecoveryTest::StaticClass());
	Ability = Spec ? Cast<UGA_Enemy_RecoveryTest>(Spec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Next recovery test ability has an execution instance"), Ability))
	{
		return false;
	}
	Ability->FinishNormally();
	Patterns->CancelRequest(RequestId);
	++GFrameCounter;
	Fixture.World->GetTimerManager().Tick(1.f);
	TestEqual(TEXT("Cancelled recovery stays cancelled"), Patterns->GetRequestResult(RequestId), EEnemyPatternResult::Cancelled);
	Patterns->ReleaseRequest(RequestId);
	Config->PatternSet->Patterns[0].RecoverySeconds = -0.1f;
	AddExpectedError(TEXT("Invalid pattern settings"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Negative common recovery is rejected"), Patterns->InitializePatterns(ASC, Config->PatternSet));
	Config->PatternSet->Patterns[0].RecoverySeconds = 0.f;
	TestTrue(TEXT("Zero common recovery is valid"), Patterns->InitializePatterns(ASC, Config->PatternSet));
	if (!TestTrue(TEXT("Zero recovery pattern starts"), Patterns->TryStartPattern(TEXT("Arena"), nullptr, RequestId)))
	{
		return false;
	}
	Spec = ASC->FindAbilitySpecFromClass(UGA_Enemy_RecoveryTest::StaticClass());
	Ability = Spec ? Cast<UGA_Enemy_RecoveryTest>(Spec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Zero recovery ability has an execution instance"), Ability))
	{
		return false;
	}
	Ability->FinishNormally();
	TestEqual(TEXT("Zero recovery completes immediately"), Patterns->GetRequestResult(RequestId), EEnemyPatternResult::Completed);
	Patterns->ReleaseRequest(RequestId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyPatternMovementTest, "MyNiceLittleCat.Enemy.PatternMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyPatternMovementTest::RunTest(const FString& Parameters)
{
	TGuardValue<uint64> RestoreFrameCounter(GFrameCounter, GFrameCounter);
	EnemyCombatTests::FFixture Fixture;
	AEnemyCharacter* Enemy = Fixture.World->SpawnActor<AEnemyCharacter>();
	AActor* Target = Fixture.World->SpawnActor<AActor>();
	UEnemyConfigData* Config = EnemyCombatTests::MakeEnemyConfig(Enemy);
	FEnemyPatternDefinition& Definition = Config->PatternSet->Patterns[0];
	Definition.AbilityClass = UGA_Enemy_RecoveryTest::StaticClass();
	Definition.RecoverySeconds = 0.5f;
	Definition.Movement.Policy = EEnemyPatternMovementPolicy::ChaseTarget;
	Definition.Movement.AcceptanceRadius = 0.f;
	Definition.Movement.bIncludeAgentRadius = false;
	Definition.Movement.bIncludeGoalRadius = false;
	EnemyCombatTests::SetObjectSetting(Enemy, TEXT("EnemyConfig"), Config);
	Enemy->DispatchBeginPlay();
	UEnemyPatternComponent* Patterns = Enemy->GetPatternComponent();
	int64 RequestId = 0;
	if (!TestTrue(TEXT("Movement pattern starts"), Patterns->TryStartPattern(TEXT("Arena"), Target, RequestId)))
	{
		return false;
	}
	FEnemyPatternMovementSettings Movement;
	AActor* MovementTarget = nullptr;
	int64 MovementRequestId = 0;
	TestTrue(TEXT("Movement is available during ability"), Patterns->GetActiveMovement(Movement, MovementTarget, MovementRequestId));
	TestEqual(TEXT("Chase policy"), Movement.Policy, EEnemyPatternMovementPolicy::ChaseTarget);
	TestTrue(TEXT("Movement uses the attack target and request"), MovementTarget == Target && MovementRequestId == RequestId);
	const FAIMoveRequest MoveRequest = UBTTask_Enemy_PatternMovement::BuildMoveRequest(MovementTarget, Movement);
	TestEqual(TEXT("Zero acceptance radius is preserved"), MoveRequest.GetAcceptanceRadius(), 0.f);
	TestFalse(TEXT("Agent radius inclusion is configurable"), MoveRequest.IsReachTestIncludingAgentRadius());
	TestFalse(TEXT("Goal radius inclusion is configurable"), MoveRequest.IsReachTestIncludingGoalRadius());
	TestFalse(TEXT("Partial paths are opt-in"), MoveRequest.IsUsingPartialPaths());
	// 실행 중 원본 데이터가 바뀌어도 현재 요청의 이동 정책은 바뀌지 않아야 한다.
	Definition.Movement.Policy = EEnemyPatternMovementPolicy::Stop;
	Patterns->GetActiveMovement(Movement, MovementTarget, MovementRequestId);
	TestEqual(TEXT("Active policy is a snapshot"), Movement.Policy, EEnemyPatternMovementPolicy::ChaseTarget);
	UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(UGA_Enemy_RecoveryTest::StaticClass());
	UGA_Enemy_RecoveryTest* Ability = Spec ? Cast<UGA_Enemy_RecoveryTest>(Spec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Movement test ability exists"), Ability))
	{
		return false;
	}
	Enemy->GetCharacterMovement()->Velocity = FVector(300.f, 0.f, 0.f);
	Ability->FinishNormally();
	TestTrue(TEXT("Recovery keeps movement context"), Patterns->GetActiveMovement(Movement, MovementTarget, MovementRequestId));
	TestEqual(TEXT("Chase always switches to stop during recovery"), Movement.Policy, EEnemyPatternMovementPolicy::Stop);
	TestTrue(TEXT("Recovery immediately stops velocity"), Enemy->GetCharacterMovement()->Velocity.IsNearlyZero());
	TestTrue(TEXT("Recovery blocks new navigation movement"), Enemy->IsAttackMovementLocked());
	TestEqual(TEXT("Recovery retains request identity"), MovementRequestId, RequestId);
	Patterns->CancelRequest(RequestId);
	TestFalse(TEXT("Cancellation releases recovery movement lock"), Enemy->IsAttackMovementLocked());
	TestFalse(TEXT("Cancelled request has no movement"), Patterns->GetActiveMovement(Movement, MovementTarget, MovementRequestId));
	TestTrue(TEXT("Invalid context clears outputs"), MovementRequestId == 0 && MovementTarget == nullptr);
	Patterns->ReleaseRequest(RequestId);

	Definition.Movement.Policy = EEnemyPatternMovementPolicy::AbilityDriven;
	if (!TestTrue(TEXT("Shared policy pattern starts"), Patterns->TryStartPattern(TEXT("Arena"), Target, RequestId)))
	{
		return false;
	}
	Enemy->AcquireAttackMovementLock();
	Ability->FinishNormally();
	Patterns->GetActiveMovement(Movement, MovementTarget, MovementRequestId);
	TestEqual(TEXT("Ability-driven recovery also always stops"), Movement.Policy, EEnemyPatternMovementPolicy::Stop);
	Fixture.World->GetTimerManager().Tick(0.f);
	++GFrameCounter;
	Fixture.World->GetTimerManager().Tick(0.6f);
	TestFalse(TEXT("Completed recovery has no movement"), Patterns->GetActiveMovement(Movement, MovementTarget, MovementRequestId));
	TestTrue(TEXT("Recovery preserves other movement locks"), Enemy->IsAttackMovementLocked());
	Enemy->ReleaseAttackMovementLock();
	TestFalse(TEXT("Recovery released only its own lock"), Enemy->IsAttackMovementLocked());
	Patterns->ReleaseRequest(RequestId);
	if (!TestTrue(TEXT("Stop cleanup pattern starts"), Patterns->TryStartPattern(TEXT("Arena"), Target, RequestId)))
	{
		return false;
	}
	Ability->FinishNormally();
	TestTrue(TEXT("Stop cleanup begins in locked recovery"), Enemy->IsAttackMovementLocked());
	Patterns->StopAndInvalidate();
	TestFalse(TEXT("Full stop releases recovery lock"), Enemy->IsAttackMovementLocked());
	Definition.Movement.AcceptanceRadius = -1.f;
	AddExpectedError(TEXT("Invalid pattern settings"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Negative movement radius rejected"), Patterns->InitializePatterns(ASC, Config->PatternSet));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyAttackIntervalTest, "MyNiceLittleCat.Enemy.AttackInterval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyAttackIntervalTest::RunTest(const FString& Parameters)
{
	TGuardValue<uint64> RestoreFrameCounter(GFrameCounter, GFrameCounter);
	EnemyCombatTests::FFixture Fixture;
	// 실제 게임 시간과 타이머 시간을 함께 진행해 대기 시작 시점을 결정적으로 검사한다.
	Fixture.World->TimeSeconds = 10.0;
	AEnemyCharacter* Enemy = Fixture.World->SpawnActor<AEnemyCharacter>();
	UEnemyConfigData* Config = EnemyCombatTests::MakeEnemyConfig(Enemy);
	Config->PatternSet->Patterns[0].AbilityClass = UGA_Enemy_RecoveryTest::StaticClass();
	Config->PatternSet->Patterns[0].Weight = 1.f;
	Config->PatternSet->Patterns[0].RecoverySeconds = 0.5f;
	Config->MinAttackInterval = 2.f;
	Config->MaxAttackInterval = 2.f;
	EnemyCombatTests::SetObjectSetting(Enemy, TEXT("EnemyConfig"), Config);
	Enemy->DispatchBeginPlay();
	UEnemyPatternComponent* Patterns = Enemy->GetPatternComponent();
	UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	TestTrue(TEXT("Enemy initializes interval from config data"), Enemy->IsEnemyInitialized());
	TestTrue(TEXT("First attack has no added delay"), Patterns->CanStartAttack());
	int64 RequestId = 0;
	if (!TestTrue(TEXT("Interval test attack starts"), Patterns->TryStartPattern(TEXT("Arena"), nullptr, RequestId))
		|| !TestTrue(TEXT("Ability ends normally"), EnemyCombatTests::FinishTestAbility(ASC)))
	{
		return false;
	}
	TestEqual(TEXT("Interval is not started at ability end"), Patterns->GetAttackIntervalRemaining(), 0.f);
	Fixture.World->GetTimerManager().Tick(0.f);
	++GFrameCounter;
	Fixture.World->TimeSeconds += 0.2;
	Fixture.World->GetTimerManager().Tick(0.2f);
	TestTrue(TEXT("Recovery still runs"), Patterns->IsRequestRunning(RequestId));
	TestEqual(TEXT("Interval waits until full recovery"), Patterns->GetAttackIntervalRemaining(), 0.f);
	++GFrameCounter;
	Fixture.World->TimeSeconds += 0.4;
	Fixture.World->GetTimerManager().Tick(0.4f);
	TestEqual(TEXT("Recovery completes before attack interval"), Patterns->GetRequestResult(RequestId), EEnemyPatternResult::Completed);
	TestTrue(TEXT("Ability is idle during added interval"), Patterns->IsIdle());
	TestEqual(TEXT("Interval starts after recovery"), Patterns->GetAttackIntervalRemaining(), 2.f);
	const double Deadline = Patterns->GetNextAttackAllowedTime();
	Patterns->ReleaseRequest(RequestId);
	TestTrue(TEXT("Release retains deadline"), Patterns->GetNextAttackAllowedTime() == Deadline);
	for (int32 Index = 0; Index < 5; ++Index)
	{
		TestFalse(TEXT("Readiness checks remain blocked"), Patterns->CanStartAttack());
		TestTrue(TEXT("Readiness does not resample interval"), Patterns->GetNextAttackAllowedTime() == Deadline);
	}
	int64 RejectedId = -1;
	TestFalse(TEXT("Random selection cannot bypass interval"), Patterns->TryStartAttack(nullptr, RejectedId));
	TestEqual(TEXT("Rejected random request clears ID"), RejectedId, int64(0));
	TestFalse(TEXT("Explicit selection cannot bypass interval"), Patterns->TryStartPattern(TEXT("Arena"), nullptr, RejectedId));
	Fixture.World->TimeSeconds = Deadline - 0.01;
	TestFalse(TEXT("Still blocked before deadline"), Patterns->CanStartAttack());
	Fixture.World->TimeSeconds = Deadline + 0.01;
	TestTrue(TEXT("Next attack allowed after deadline"), Patterns->CanStartAttack());
	if (!TestTrue(TEXT("Random selection resumes"), Patterns->TryStartAttack(nullptr, RequestId)))
	{
		return false;
	}
	Patterns->CancelRequest(RequestId);
	Patterns->ReleaseRequest(RequestId);
	TestEqual(TEXT("Cancelled ability does not start interval"), Patterns->GetAttackIntervalRemaining(), 0.f);
	if (!TestTrue(TEXT("Recovery cancellation attack starts"), Patterns->TryStartPattern(TEXT("Arena"), nullptr, RequestId))
		|| !TestTrue(TEXT("Recovery cancellation ability ends"), EnemyCombatTests::FinishTestAbility(ASC)))
	{
		return false;
	}
	Patterns->CancelRequest(RequestId);
	++GFrameCounter;
	Fixture.World->TimeSeconds += 5.0;
	Fixture.World->GetTimerManager().Tick(5.f);
	TestEqual(TEXT("Cancelled recovery stays cancelled"), Patterns->GetRequestResult(RequestId), EEnemyPatternResult::Cancelled);
	TestEqual(TEXT("Cancelled recovery does not start interval later"), Patterns->GetAttackIntervalRemaining(), 0.f);
	Patterns->ReleaseRequest(RequestId);

	TestTrue(TEXT("Random interval configuration accepted"), Patterns->ConfigureAttackInterval(0.25f, 0.75f));
	Config->PatternSet->Patterns[0].RecoverySeconds = 0.f;
	if (!TestTrue(TEXT("Zero recovery attack starts"), Patterns->TryStartPattern(TEXT("Arena"), nullptr, RequestId))
		|| !TestTrue(TEXT("Zero recovery ability ends"), EnemyCombatTests::FinishTestAbility(ASC)))
	{
		return false;
	}
	const float RandomInterval = Patterns->GetAttackIntervalRemaining();
	TestTrue(TEXT("Sample is inside configured range"), RandomInterval >= 0.25f && RandomInterval <= 0.75f);
	Patterns->ReleaseRequest(RequestId);
	// 같은 데이터 에셋을 공유해도 실행 시각과 컴포넌트의 임시 설정은 공유하지 않는다.
	const double FirstDeadline = Patterns->GetNextAttackAllowedTime();
	AEnemyCharacter* OtherEnemy = Fixture.World->SpawnActor<AEnemyCharacter>();
	EnemyCombatTests::SetObjectSetting(OtherEnemy, TEXT("EnemyConfig"), Config);
	OtherEnemy->DispatchBeginPlay();
	UEnemyPatternComponent* OtherPatterns = OtherEnemy->GetPatternComponent();
	TestTrue(TEXT("Shared config enemy has no first attack delay"), OtherPatterns->CanStartAttack());
	TestEqual(TEXT("Shared config has no shared runtime deadline"), OtherPatterns->GetNextAttackAllowedTime(), 0.0);
	TestEqual(TEXT("Runtime tuning did not mutate shared config"), Config->MinAttackInterval, 2.f);
	int64 OtherRequestId = 0;
	if (!TestTrue(TEXT("Shared config enemy can attack"), OtherPatterns->TryStartPattern(TEXT("Arena"), nullptr, OtherRequestId))
		|| !TestTrue(TEXT("Shared config enemy ends normally"), EnemyCombatTests::FinishTestAbility(OtherEnemy->GetAbilitySystemComponent())))
	{
		return false;
	}
	OtherPatterns->ReleaseRequest(OtherRequestId);
	TestEqual(TEXT("Other enemy reads the original data interval"), OtherPatterns->GetAttackIntervalRemaining(), 2.f);
	TestTrue(TEXT("Other enemy did not overwrite first deadline"), Patterns->GetNextAttackAllowedTime() == FirstDeadline);
	Patterns->StopAndInvalidate();
	TestEqual(TEXT("Full stop resets interval"), Patterns->GetAttackIntervalRemaining(), 0.f);
	TestFalse(TEXT("Stopped component cannot attack"), Patterns->CanStartAttack());
	TestTrue(TEXT("Reinitialization succeeds"), Patterns->InitializePatterns(ASC, Config->PatternSet));
	TestTrue(TEXT("Reinitialization has no first attack delay"), Patterns->CanStartAttack());
	TestTrue(TEXT("Zero interval can disable added delay"), Patterns->ConfigureAttackInterval(0.f, 0.f));
	if (!TestTrue(TEXT("Disabled interval attack starts"), Patterns->TryStartPattern(TEXT("Arena"), nullptr, RequestId))
		|| !TestTrue(TEXT("Disabled interval ability ends"), EnemyCombatTests::FinishTestAbility(ASC)))
	{
		return false;
	}
	Patterns->ReleaseRequest(RequestId);
	TestTrue(TEXT("Zero interval allows immediate next attack"), Patterns->CanStartAttack());
	TestFalse(TEXT("Negative interval rejected"), Patterns->ConfigureAttackInterval(-1.f, 1.f));
	TestFalse(TEXT("Reversed interval range rejected"), Patterns->ConfigureAttackInterval(2.f, 1.f));
	UEnemyConfigData* InvalidConfig = EnemyCombatTests::MakeEnemyConfig(Fixture.World);
	InvalidConfig->MinAttackInterval = 2.f;
	InvalidConfig->MaxAttackInterval = 1.f;
	AEnemyCharacter* InvalidEnemy = Fixture.World->SpawnActor<AEnemyCharacter>();
	EnemyCombatTests::SetObjectSetting(InvalidEnemy, TEXT("EnemyConfig"), InvalidConfig);
	AddExpectedError(TEXT("attack interval configuration failed"), EAutomationExpectedErrorFlags::Contains, 1);
	InvalidEnemy->DispatchBeginPlay();
	TestFalse(TEXT("Invalid data interval blocks initialization"), InvalidEnemy->IsEnemyInitialized());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossAttackIntervalTest, "MyNiceLittleCat.Enemy.AttackIntervalBoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBossAttackIntervalTest::RunTest(const FString& Parameters)
{
	EnemyCombatTests::FFixture Fixture;
	ABossCharacter* Boss = Fixture.World->SpawnActor<ABossCharacter>();
	UEnemyConfigData* Config = EnemyCombatTests::MakeEnemyConfig(Boss);
	Config->PatternSet->Patterns[0].AbilityClass = UGA_Enemy_RecoveryTest::StaticClass();
	Config->MinAttackInterval = 1.25f;
	Config->MaxAttackInterval = 1.25f;
	EnemyCombatTests::SetObjectSetting(Boss, TEXT("EnemyConfig"), Config);
	UBossPhaseConfigData* Phases = NewObject<UBossPhaseConfigData>(Boss);
	Phases->bLogOnlyAfterFirstPhase = false;
	FBossPhaseDefinition First;
	First.PhaseId = TEXT("First");
	First.HealthFloorRatio = 0.7f;
	First.AllowedPatternIds.Add(TEXT("Arena"));
	Phases->Phases.Add(First);
	FBossPhaseDefinition Final;
	Final.PhaseId = TEXT("Final");
	Final.AllowedPatternIds.Add(TEXT("Arena"));
	Phases->Phases.Add(Final);
	EnemyCombatTests::SetObjectSetting(Boss, TEXT("PhaseConfig"), Phases);
	Boss->DispatchBeginPlay();
	UEnemyPatternComponent* Patterns = Boss->GetPatternComponent();
	TestFalse(TEXT("Idle encounter cannot attack"), Patterns->CanStartAttack());
	if (!TestTrue(TEXT("Boss interval encounter starts"), Boss->StartEncounter()))
	{
		return false;
	}
	TestTrue(TEXT("Boss first attack is immediately ready"), Patterns->CanStartAttack());
	int64 RequestId = 0;
	if (!TestTrue(TEXT("Boss interval attack starts"), Patterns->TryStartPattern(TEXT("Arena"), nullptr, RequestId))
		|| !TestTrue(TEXT("Boss interval ability ends"), EnemyCombatTests::FinishTestAbility(Boss->GetAbilitySystemComponent())))
	{
		return false;
	}
	const float Interval = Patterns->GetAttackIntervalRemaining();
	TestEqual(TEXT("Boss also initializes component from enemy config data"), Interval, 1.25f);
	Patterns->ReleaseRequest(RequestId);
	Boss->GetPhaseComponent()->EvaluateHealth(70.f, 100.f);
	TestEqual(TEXT("Phase transition resets previous interval"), Patterns->GetAttackIntervalRemaining(), 0.f);
	TestFalse(TEXT("Transition still blocks requests"), Patterns->CanStartAttack());
	int64 TransitionId = 0;
	TestTrue(TEXT("Interval transition begins"), Boss->GetPhaseComponent()->BeginPendingTransition(TransitionId));
	TestTrue(TEXT("Interval transition completes"), Boss->GetPhaseComponent()->CompleteTransition(TransitionId));
	Boss->FinishPhaseTransition();
	TestTrue(TEXT("New phase does not inherit old interval"), Patterns->CanStartAttack());
	Boss->StopEncounter();
	TestFalse(TEXT("Stopped encounter cannot attack"), Patterns->CanStartAttack());
	TestEqual(TEXT("Stopped encounter clears deadline"), Patterns->GetNextAttackAllowedTime(), 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossHealthGateTest, "MyNiceLittleCat.Enemy.BossHealthGates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 큰 피해의 단계별 제한과 전환 대기 중 추가 피해 차단을 실제 Damage GE로 검사
bool FBossHealthGateTest::RunTest(const FString& Parameters)
{
	EnemyCombatTests::FFixture Fixture;
	ABossCharacter* Boss = Fixture.World->SpawnActor<ABossCharacter>();
	EnemyCombatTests::SetObjectSetting(Boss, TEXT("EnemyConfig"), EnemyCombatTests::MakeEnemyConfig(Boss));
	UBossPhaseConfigData* Config = NewObject<UBossPhaseConfigData>(Boss);
	Config->bLogOnlyAfterFirstPhase = false;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FBossPhaseDefinition Phase;
		Phase.PhaseId = FName(*FString::Printf(TEXT("Combat%d"), Index));
		Phase.HealthFloorRatio = Index == 0 ? 0.7f : Index == 1 ? 0.3f : 0.f;
		Phase.AllowedPatternIds.Add(TEXT("Arena"));
		Config->Phases.Add(Phase);
	}
	EnemyCombatTests::SetObjectSetting(Boss, TEXT("PhaseConfig"), Config);
	Boss->DispatchBeginPlay();
	TestTrue(TEXT("Boss initialized"), Boss->IsEnemyInitialized());
	TestEqual(TEXT("Idle boss ignores damage"), Boss->CalculateHealthAfterDamage(100.f, 100.f, 90.f), 100.f);
	TestTrue(TEXT("Encounter starts"), Boss->StartEncounter());
	UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
	UGameplayEffect* Damage = NewObject<UGameplayEffect>(Boss);
	Damage->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UEnemyAttributeSet::GetDamageAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(90.f);
	Damage->Modifiers.Add(Modifier);
	FGameplayEffectSpec Spec(Damage, ASC->MakeEffectContext(), 1.f);
	ASC->ApplyGameplayEffectSpecToSelf(Spec);
	TestEqual(TEXT("Large hit stops at seventy"), Boss->GetAttributeSet()->GetHealth(), 70.f);
	TestTrue(TEXT("Threshold requests transition"), Boss->GetPhaseComponent()->HasPendingTransition());
	ASC->ApplyGameplayEffectSpecToSelf(Spec);
	TestEqual(TEXT("Pending transition blocks another hit"), Boss->GetAttributeSet()->GetHealth(), 70.f);
	int64 TransitionId = 0;
	TestTrue(TEXT("Transition starts"), Boss->GetPhaseComponent()->BeginPendingTransition(TransitionId));
	TestTrue(TEXT("No-animation transition completes"), Boss->GetPhaseComponent()->CompleteTransition(TransitionId));
	Boss->FinishPhaseTransition();
	ASC->ApplyGameplayEffectSpecToSelf(Spec);
	TestEqual(TEXT("Next hit stops at thirty"), Boss->GetAttributeSet()->GetHealth(), 30.f, 0.0001f);
	TestFalse(TEXT("Old completion cannot advance next phase"), Boss->GetPhaseComponent()->CompleteTransition(TransitionId));
	Boss->GetPhaseComponent()->BeginPendingTransition(TransitionId);
	Boss->GetPhaseComponent()->CompleteTransition(TransitionId);
	Boss->FinishPhaseTransition();
	TestEqual(TEXT("Final phase allows lethal damage"), Boss->CalculateHealthAfterDamage(30.f, 100.f, 90.f), 0.f);
	Boss->StopEncounter();
	TestEqual(TEXT("Stopped boss ignores damage"), Boss->CalculateHealthAfterDamage(30.f, 100.f, 90.f), 30.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossPhaseConfigurationTest, "MyNiceLittleCat.Enemy.PhaseConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 필수 페이즈 누락, 중복 ID 및 역순 체력 하한을 실행 전에 거부하는지 검사
bool FBossPhaseConfigurationTest::RunTest(const FString& Parameters)
{
	// 에디터 설정을 대신할 임시 페이즈 데이터
	UBossPhaseConfigData* Config = NewObject<UBossPhaseConfigData>();
	Config->bLogOnlyAfterFirstPhase = false;
	// 검증 실패 원인 출력용 문자열
	FString Error;
	TestFalse(TEXT("Empty phase list rejected"), Config->ValidateConfiguration(Error));
	// 마지막 전투만 있는 보스는 유효한 구성
	FBossPhaseDefinition Final;
	Final.PhaseId = TEXT("Final");
	Config->Phases.Add(Final);
	TestTrue(TEXT("Single final combat accepted"), Config->ValidateConfiguration(Error));
	Config->Phases.Add(Final);
	TestFalse(TEXT("Duplicate IDs rejected"), Config->ValidateConfiguration(Error));
	Config->Phases[0].PhaseId = TEXT("First");
	Config->Phases[0].HealthFloorRatio = 0.3f;
	Config->Phases[1].HealthFloorRatio = 0.7f;
	TestFalse(TEXT("Increasing health floors rejected"), Config->ValidateConfiguration(Error));
	Config->Phases[0].HealthFloorRatio = 0.7f;
	Config->Phases[1].HealthFloorRatio = 0.f;
	TestTrue(TEXT("Decreasing floors ending at zero accepted"), Config->ValidateConfiguration(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossExternalPlaceholderTest, "MyNiceLittleCat.Enemy.ExternalPlaceholders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 기믹 없이 로그 전용 페이즈에 머물며 수동 완료로 한 단계씩 진행하는지 검사
bool FBossExternalPlaceholderTest::RunTest(const FString& Parameters)
{
	EnemyCombatTests::FFixture Fixture;
	ABossCharacter* Boss = Fixture.World->SpawnActor<ABossCharacter>();
	UEnemyConfigData* EnemyConfig = EnemyCombatTests::MakeEnemyConfig(Boss);
	FEnemyPatternDefinition Placeholder;
	Placeholder.PatternId = TEXT("ExternalShield");
	Placeholder.bLogOnly = true;
	EnemyConfig->PatternSet->Patterns.Add(Placeholder);
	EnemyCombatTests::SetObjectSetting(Boss, TEXT("EnemyConfig"), EnemyConfig);
	UBossPhaseConfigData* Config = NewObject<UBossPhaseConfigData>(Boss);
	Config->bLogOnlyAfterFirstPhase = true;
	FBossPhaseDefinition First;
	First.PhaseId = TEXT("Phase1");
	First.HealthFloorRatio = 0.7f;
	First.AllowedPatternIds.Add(Placeholder.PatternId);
	Config->Phases.Add(First);
	for (int32 Index = 1; Index < 4; ++Index)
	{
		FBossPhaseDefinition External;
		External.PhaseId = FName(*FString::Printf(TEXT("External%d"), Index));
		External.Kind = Index == 1 ? EBossPhaseKind::Barrier : EBossPhaseKind::Combat;
		External.bUseTransitionSequence = true;
		External.AllowedPatternIds.Add(TEXT("UnimplementedPattern"));
		Config->Phases.Add(External);
	}
	EnemyCombatTests::SetObjectSetting(Boss, TEXT("PhaseConfig"), Config);
	Boss->DispatchBeginPlay();
	TestTrue(TEXT("External assets are not needed for initialization"), Boss->IsEnemyInitialized());
	TestTrue(TEXT("Encounter starts"), Boss->StartEncounter());
	int64 RequestId = 0;
	UEnemyPatternComponent* Patterns = Boss->GetPatternComponent();
	TestTrue(TEXT("Placeholder accepts null target and null ability"), Patterns->TryStartPattern(Placeholder.PatternId, nullptr, RequestId));
	TestEqual(TEXT("Placeholder completes immediately"), Patterns->GetRequestResult(RequestId), EEnemyPatternResult::Completed);
	Patterns->ReleaseRequest(RequestId);
	TestTrue(TEXT("No ability remains running"), Patterns->IsIdle());
	UEnemyPhaseComponent* Phases = Boss->GetPhaseComponent();
	Phases->EvaluateHealth(70.f, 100.f);
	int64 TransitionId = 0;
	TestTrue(TEXT("External phase transition starts"), Phases->BeginPendingTransition(TransitionId));
	TestEqual(TEXT("Log-only phase does not wait for a sequence"), Phases->GetTransitionResult(TransitionId), EEnemyPatternResult::Completed);
	TestTrue(TEXT("Log-only phase entry succeeds without gimmicks"), Phases->CompleteTransition(TransitionId));
	Boss->FinishPhaseTransition();
	TestEqual(TEXT("First log-only phase waits instead of skipping"), Phases->GetCurrentPhaseId(), FName(TEXT("External1")));
	TestEqual(TEXT("Health remains unchanged while waiting"), Boss->CalculateHealthAfterDamage(70.f, 100.f, 100.f), 70.f);
	TestFalse(TEXT("Previous phase attacks stay disabled"), Patterns->TryStartPattern(Placeholder.PatternId, nullptr, RequestId));
	TestFalse(TEXT("Stale transition completion ignored"), Phases->CompleteTransition(TransitionId));
	for (int32 Index = 1; Index < 3; ++Index)
	{
		const int64 ExecutionId = Phases->GetCurrentPhaseExecutionId();
		TestFalse(TEXT("Stale manual completion rejected"), Phases->RequestNextPhaseFromGimmick(Phases->GetCurrentPhaseId(), ExecutionId + 1));
		TestTrue(TEXT("Manual completion accepted for log-only barrier or combat"), Phases->RequestNextPhaseFromGimmick(Phases->GetCurrentPhaseId(), ExecutionId));
		TestTrue(TEXT("Next log-only transition starts"), Phases->BeginPendingTransition(TransitionId));
		TestTrue(TEXT("Next log-only phase entered"), Phases->CompleteTransition(TransitionId));
		Boss->FinishPhaseTransition();
		TestEqual(TEXT("Manual completion advances exactly one phase"), Phases->GetCurrentPhaseId(), Config->Phases[Index + 1].PhaseId);
	}
	TestFalse(TEXT("Final log-only phase has no next phase"), Phases->RequestNextPhaseFromGimmick(Phases->GetCurrentPhaseId(), Phases->GetCurrentPhaseExecutionId()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossBarrierPhaseTest, "MyNiceLittleCat.Enemy.BarrierPhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 배리어를 건너뛰지 않고 직전 체력 하한을 유지하며 기믹 완료를 기다리는지 검사
bool FBossBarrierPhaseTest::RunTest(const FString& Parameters)
{
	EnemyCombatTests::FFixture Fixture;
	ABossCharacter* Boss = Fixture.World->SpawnActor<ABossCharacter>();
	EnemyCombatTests::SetObjectSetting(Boss, TEXT("EnemyConfig"), EnemyCombatTests::MakeEnemyConfig(Boss));
	UBossPhaseConfigData* Config = NewObject<UBossPhaseConfigData>(Boss);
	TestFalse(TEXT("Full phase execution is the default"), Config->bLogOnlyAfterFirstPhase);
	FBossPhaseDefinition First;
	First.PhaseId = TEXT("CombatA");
	First.HealthFloorRatio = 0.7f;
	Config->Phases.Add(First);
	FBossPhaseDefinition Barrier;
	Barrier.PhaseId = TEXT("Barrier70");
	Barrier.Kind = EBossPhaseKind::Barrier;
	Barrier.HealthFloorRatio = 0.7f;
	Config->Phases.Add(Barrier);
	FBossPhaseDefinition Final;
	Final.PhaseId = TEXT("CombatC");
	Config->Phases.Add(Final);
	FString Error;
	TestTrue(TEXT("Combat and barrier floors may match"), Config->ValidateConfiguration(Error));
	TestFalse(TEXT("Barrier is executable"), Config->IsLogOnlyPhase(1));
	EnemyCombatTests::SetObjectSetting(Boss, TEXT("PhaseConfig"), Config);
	Boss->DispatchBeginPlay();
	TestTrue(TEXT("Encounter starts"), Boss->StartEncounter());
	UEnemyPhaseComponent* Phases = Boss->GetPhaseComponent();
	// 기믹 액터 대신 페이즈 관리 계약만 검증; 실제 보스는 계약 구현 컴포넌트를 요구
	Phases->ApplyPhase.BindLambda([](const FBossPhaseDefinition&) { return true; });
	Phases->EvaluateHealth(70.f, 100.f);
	int64 TransitionId = 0;
	TestTrue(TEXT("Barrier transition starts"), Phases->BeginPendingTransition(TransitionId));
	TestTrue(TEXT("Barrier entered"), Phases->CompleteTransition(TransitionId));
	Boss->FinishPhaseTransition();
	TestEqual(TEXT("Barrier is current phase"), Phases->GetCurrentPhaseId(), Barrier.PhaseId);
	TestEqual(TEXT("Barrier retains previous floor"), Boss->CalculateHealthAfterDamage(70.f, 100.f, 90.f), 70.f);
	Phases->EvaluateHealth(70.f, 100.f);
	TestFalse(TEXT("Health does not complete barrier"), Phases->HasPendingTransition());
	const int64 FirstExecution = Phases->GetCurrentPhaseExecutionId();
	TestTrue(TEXT("Barrier has an execution ID"), FirstExecution > 0);
	TestFalse(TEXT("Wrong phase rejected"), Phases->RequestNextPhaseFromGimmick(TEXT("Other"), FirstExecution));
	TestFalse(TEXT("Wrong execution rejected"), Phases->RequestNextPhaseFromGimmick(Barrier.PhaseId, FirstExecution + 1));
	TestTrue(TEXT("Matching completion requests next phase"), Phases->RequestNextPhaseFromGimmick(Barrier.PhaseId, FirstExecution));
	TestFalse(TEXT("Duplicate completion rejected"), Phases->RequestNextPhaseFromGimmick(Barrier.PhaseId, FirstExecution));
	Phases->BeginPendingTransition(TransitionId);
	TestTrue(TEXT("Final combat entered"), Phases->CompleteTransition(TransitionId));
	TestFalse(TEXT("Combat cannot finish through gimmick API"), Phases->RequestNextPhaseFromGimmick(Final.PhaseId, Phases->GetCurrentPhaseExecutionId()));
	// 같은 설정으로 재초기화해도 이전 진입 번호는 다시 사용하지 않음
	Phases->InitializePhases(Boss->GetAbilitySystemComponent(), Config);
	Phases->StartPhases();
	Phases->EvaluateHealth(70.f, 100.f);
	Phases->BeginPendingTransition(TransitionId);
	Phases->CompleteTransition(TransitionId);
	TestFalse(TEXT("Previous encounter completion rejected"), Phases->RequestNextPhaseFromGimmick(Barrier.PhaseId, FirstExecution));
	const int64 SecondExecution = Phases->GetCurrentPhaseExecutionId();
	TestTrue(TEXT("Execution IDs increase across resets"), SecondExecution > FirstExecution);
	Phases->StopAndInvalidate();
	TestFalse(TEXT("Stopped phase rejects completion"), Phases->RequestNextPhaseFromGimmick(Barrier.PhaseId, SecondExecution));
	// 진입 함수 안에서 즉시 완료되는 기믹도 활성화 후 전환을 한 번만 요청
	Phases->InitializePhases(Boss->GetAbilitySystemComponent(), Config);
	Phases->ApplyPhase.BindLambda([this, Phases](const FBossPhaseDefinition& Phase)
	{
		if (Phase.Kind == EBossPhaseKind::Barrier)
		{
			TestTrue(TEXT("Synchronous completion accepted"), Phases->RequestNextPhaseFromGimmick(Phase.PhaseId, Phases->GetCurrentPhaseExecutionId()));
			TestFalse(TEXT("Synchronous duplicate rejected"), Phases->RequestNextPhaseFromGimmick(Phase.PhaseId, Phases->GetCurrentPhaseExecutionId()));
		}
		return true;
	});
	Phases->StartPhases();
	Phases->EvaluateHealth(70.f, 100.f);
	Phases->BeginPendingTransition(TransitionId);
	TestTrue(TEXT("Synchronous completion does not break phase application"), Phases->CompleteTransition(TransitionId));
	TestTrue(TEXT("Synchronous completion leaves next transition pending"), Phases->HasPendingTransition());
	Phases->BeginPendingTransition(TransitionId);
	TestTrue(TEXT("Synchronous path reaches final combat"), Phases->CompleteTransition(TransitionId));
	return true;
}

#endif
