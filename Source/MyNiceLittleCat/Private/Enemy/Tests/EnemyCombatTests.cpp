#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/Boss/Components/BossPhaseComponent.h"
#include "Enemy/Boss/Datas/BossPhaseConfigData.h"
#include "Enemy/Datas/EnemyConfigData.h"
#include "Enemy/Datas/EnemyPatternSetData.h"
#include "Enemy/GAS/EnemyAttributeSet.h"

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
	UBossPhaseComponent* Phases = Boss->GetPhaseComponent();
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
	UBossPhaseComponent* Phases = Boss->GetPhaseComponent();
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
