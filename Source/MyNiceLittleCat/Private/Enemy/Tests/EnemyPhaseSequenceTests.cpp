#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "EngineGlobals.h"
#include "AbilitySystemComponent.h"
#include "Enemy/Boss/Components/BossPhaseComponent.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace EnemyPhaseSequenceTests
{
	// 맵과 에셋 없이 전환 시퀀스를 생성할 임시 월드 및 페이즈 컴포넌트
	struct FFixture
	{
		UWorld* World = nullptr;
		AActor* Owner = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
		UBossPhaseComponent* Phase = nullptr;
		UBossPhaseConfigData* Config = nullptr;
		FFixture()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
				.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			Owner = World->SpawnActor<AActor>();
			ASC = NewObject<UAbilitySystemComponent>(Owner);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Owner, Owner);
			Phase = NewObject<UBossPhaseComponent>(Owner);
			Phase->RegisterComponent();
			Phase->ApplyPhase.BindLambda([](const FBossPhaseDefinition&) { return true; });
			Config = NewObject<UBossPhaseConfigData>(Owner);
			for (int32 Index = 0; Index < 3; ++Index)
			{
				FBossPhaseDefinition Entry;
				Entry.PhaseId = FName(*FString::Printf(TEXT("Combat%d"), Index));
				Entry.HealthFloorRatio = Index == 0 ? 0.7f : Index == 1 ? 0.3f : 0.f;
				Config->Phases.Add(Entry);
			}
		}
		~FFixture() { Phase->StopAndInvalidate(); World->DestroyWorld(false); }
		// 동일 설정으로 재시작해도 새 전환 번호 발급
		int64 Begin(bool bEnabled, ULevelSequence* Sequence = nullptr, float Timeout = 15.f)
		{
			Config->Phases[1].bUseTransitionSequence = bEnabled;
			Config->Phases[1].TransitionSequence = Sequence;
			Config->Phases[1].TransitionTimeout = Timeout;
			check(Phase->InitializePhases(ASC, Config));
			check(Phase->StartPhases());
			Phase->EvaluateHealth(70.f, 100.f);
			int64 Id = 0;
			check(Phase->BeginPendingTransition(Id));
			return Id;
		}
		// 비공개 실행 플레이어를 검사하되 제품 API에는 테스트용 접근자를 추가하지 않음
		ULevelSequencePlayer* Player() const
		{
			const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Phase->GetClass(), TEXT("TransitionSequencePlayer"));
			return Cast<ULevelSequencePlayer>(Property->GetObjectPropertyValue_InContainer(Phase));
		}
		// 10초 길이의 빈 시퀀스로 실제 플레이어 생성과 이벤트 구독 검사
		ULevelSequence* MakeSequence()
		{
			ULevelSequence* Sequence = NewObject<ULevelSequence>(Owner);
			Sequence->Initialize();
			Sequence->GetMovieScene()->SetTickResolutionDirectly(FFrameRate(30, 1));
			Sequence->GetMovieScene()->SetDisplayRate(FFrameRate(30, 1));
			Sequence->GetMovieScene()->SetPlaybackRange(0, 300);
			return Sequence;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyPhaseSequenceTest, "MyNiceLittleCat.Enemy.PhaseSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 연출 생략과 실패·취소·타임아웃 진행 및 초기화 후 재생 대기 정책 검사
bool FEnemyPhaseSequenceTest::RunTest(const FString& Parameters)
{
	EnemyPhaseSequenceTests::FFixture Fixture;
	UBossPhaseComponent* Phase = Fixture.Phase;
	int64 Id = Fixture.Begin(false);
	TestEqual(TEXT("Disabled sequence completes immediately"), Phase->GetTransitionResult(Id), EEnemyPatternResult::Completed);
	TestTrue(TEXT("Disabled sequence enters next phase"), Phase->CompleteTransition(Id));
	Id = Fixture.Begin(true);
	TestEqual(TEXT("Missing sequence skips the animation"), Phase->GetTransitionResult(Id), EEnemyPatternResult::Completed);
	TestTrue(TEXT("Missing sequence still advances"), Phase->CompleteTransition(Id));
	ULevelSequence* Sequence = Fixture.MakeSequence();
	Id = Fixture.Begin(true, Sequence, 0.f);
	TestEqual(TEXT("Invalid timeout skips animation"), Phase->GetTransitionResult(Id), EEnemyPatternResult::Completed);
	Id = Fixture.Begin(true, Sequence);
	ULevelSequencePlayer* Player = Fixture.Player();
	TestNotNull(TEXT("Transition owns a sequence player"), Player);
	if (!Player) return false;
	TestEqual(TEXT("Playing sequence waits"), Phase->GetTransitionResult(Id), EEnemyPatternResult::Running);
	// 실제 재생 시간을 끝까지 진행하여 엔진의 종료 이벤트와 정리 경로 검사
	Player->Update(0.f);
	Player->Update(11.f);
	TestEqual(TEXT("Finished event permits advance"), Phase->GetTransitionResult(Id), EEnemyPatternResult::Completed);
	TestTrue(TEXT("Finished sequence advances"), Phase->CompleteTransition(Id));
	Id = Fixture.Begin(true, Sequence);
	TestFalse(TEXT("Wrong cancellation ID ignored"), Phase->CancelTransitionSequence(Id + 1));
	TestTrue(TEXT("Cancellation accepted"), Phase->CancelTransitionSequence(Id));
	TestTrue(TEXT("Cancelled sequence advances"), Phase->CompleteTransition(Id));
	Id = Fixture.Begin(true, Sequence);
	Fixture.Player()->Stop();
	TestEqual(TEXT("External stop permits advance"), Phase->GetTransitionResult(Id), EEnemyPatternResult::Completed);
	TestTrue(TEXT("External stop advances"), Phase->CompleteTransition(Id));
	Id = Fixture.Begin(true, Sequence, 0.05f);
	Fixture.World->GetTimerManager().Tick(0.01f);
	++GFrameCounter;
	Fixture.World->GetTimerManager().Tick(0.1f);
	TestEqual(TEXT("Timeout permits advance"), Phase->GetTransitionResult(Id), EEnemyPatternResult::Completed);
	TestTrue(TEXT("Timed-out sequence advances"), Phase->CompleteTransition(Id));
	Id = Fixture.Begin(true, Sequence);
	ULevelSequencePlayer* PreviousPlayer = Fixture.Player();
	Phase->StopAndInvalidate();
	TestEqual(TEXT("Reset invalidates old transition"), Phase->GetTransitionResult(Id), EEnemyPatternResult::Invalid);
	TestFalse(TEXT("Reset never advances the old transition"), Phase->CompleteTransition(Id));
	TestNull(TEXT("Reset releases the old player"), Fixture.Player());
	const int64 NewId = Fixture.Begin(true, Sequence);
	TestTrue(TEXT("New transition has a fresh ID"), NewId > Id);
	TestTrue(TEXT("New transition owns a fresh player"), Fixture.Player() != PreviousPlayer);
	PreviousPlayer->OnFinished.Broadcast();
	TestEqual(TEXT("Old sequence callback cannot complete new transition"), Phase->GetTransitionResult(NewId), EEnemyPatternResult::Running);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyManualBarrierPhaseTest, "MyNiceLittleCat.Enemy.ManualBarrierPhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyManualBarrierPhaseTest::RunTest(const FString& Parameters)
{
	EnemyPhaseSequenceTests::FFixture Fixture;
	Fixture.Config->Phases[1].Kind = EBossPhaseKind::Barrier;
	UBossPhaseComponent* Phase = Fixture.Phase;
	const int64 Id = Fixture.Begin(false);
	TestTrue(TEXT("Barrier entry succeeds without automatic completion"), Phase->CompleteTransition(Id));
	TestTrue(TEXT("Barrier remains active waiting for manual completion"), Phase->IsActive());
	TestEqual(TEXT("Barrier is not skipped"), Phase->GetCurrentPhaseId(), Fixture.Config->Phases[1].PhaseId);
	const FName BarrierId = Phase->GetCurrentPhaseId();
	const int64 ExecutionId = Phase->GetCurrentPhaseExecutionId();
	TestFalse(TEXT("Stale completion rejected"), Phase->RequestNextPhaseFromGimmick(BarrierId, ExecutionId + 1));
	TestTrue(TEXT("Manual completion accepted"), Phase->RequestNextPhaseFromGimmick(BarrierId, ExecutionId));
	TestFalse(TEXT("Duplicate completion rejected"), Phase->RequestNextPhaseFromGimmick(BarrierId, ExecutionId));
	int64 NextId = 0;
	TestTrue(TEXT("Manual completion requests next transition"), Phase->BeginPendingTransition(NextId));
	TestTrue(TEXT("Next combat phase entered"), Phase->CompleteTransition(NextId));
	TestEqual(TEXT("Final combat is current"), Phase->GetCurrentPhaseId(), Fixture.Config->Phases[2].PhaseId);
	return true;
}

#endif
