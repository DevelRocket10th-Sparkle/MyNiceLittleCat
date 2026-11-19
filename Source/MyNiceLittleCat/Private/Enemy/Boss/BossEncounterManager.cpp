// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Boss/BossEncounterManager.h"

#include "BrainComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#include "Characters/PlayerCharacter.h"
#include "Enemy/AI/EnemyAIController.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Interfaces/BossEncounterDirector.h"
#include "Enemy/Interfaces/BossPhaseTransitionHandler.h"
#include "Frameworks/GameMode/MainGameMode.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Frameworks/Sound/BGMSubSystem.h"
#include "Engine/GameInstance.h"
#include "Widgets/MainHUD.h"

namespace
{
	// 전투 중 플레이어 생존 확인 간격 (사망 이벤트 누락 대비)
	constexpr float PlayerCheckInterval = 0.5f;
}

ABossEncounterManager::ABossEncounterManager()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ABossEncounterManager::BeginPlay()
{
	Super::BeginPlay();
	ResetEncounter();
}

void ABossEncounterManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	++EncounterGeneration;
	GetWorldTimerManager().ClearTimer(PlayerCheckHandle);
	GetWorldTimerManager().ClearTimer(ResetTimerHandle);

	StopWaitForRespawn();
	ClearEncounterPlayer();
	RemoveCurrentBoss();
	HideEncounterHealthBar();

	// 매니저가 사라지는데 스택에 남아 있으면 보스 곡이 계속 재생되므로 정리
	PopBossBGM();

	EncounterDirector.Reset();

	Super::EndPlay(EndPlayReason);
}

/* 전투 진행 */

bool ABossEncounterManager::RequestEncounterStart(APlayerCharacter* Player)
{
	if (!HasAuthority() || EncounterState != EBossEncounterState::Ready
		|| !IsValid(Player) || Player->GetbIsDead())
	{
		return false;
	}

	// 연출 담당자가 있으면 등장 연출이 끝난 뒤 담당자가 StartEncounter를 호출
	if (IBossEncounterDirector* Director = GetDirector())
	{
		return Director->TryBeginEncounterIntro(*this, Player);
	}

	return StartEncounter(Player);
}

// 준비된 보스방의 첫 페이즈와 타깃 선정 시작
bool ABossEncounterManager::StartEncounter(APlayerCharacter* Player)
{
	if (EncounterState != EBossEncounterState::Ready || !IsValid(CurrentBoss)
		|| !IsValid(Player) || Player->GetbIsDead())
	{
		return false;
	}

	EncounterPlayer = Player;
	EncounterPlayer->OnPlayerDied.AddUniqueDynamic(this, &ABossEncounterManager::HandlePlayerDied);

	AEnemyAIController* AI = Cast<AEnemyAIController>(CurrentBoss->GetController());
	if (!AI || !CurrentBoss->StartEncounter())
	{
		ResetEncounter();
		return false;
	}

	// BT가 실제로 실행되지 않으면(필수 BB 키 누락 등) 전투를 진행할 수 없음
	if (!AI->GetBrainComponent() || !AI->GetBrainComponent()->IsRunning())
	{
		UE_LOG(LogTemp, Error, TEXT("[BossEncounter] %s: boss AI did not start"), *GetName());
		ResetEncounter();
		return false;
	}

	EncounterState = EBossEncounterState::Running;
	AI->SetCombatTarget(EncounterPlayer);

	// 타이머가 만들어진 전투 번호를 저장하여 이전 전투의 호출 무시
	const int64 StartedGeneration = EncounterGeneration;
	GetWorldTimerManager().SetTimer(PlayerCheckHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, StartedGeneration]()
		{
			if (StartedGeneration == EncounterGeneration)
			{
				CheckPlayer();
			}
		}),
		PlayerCheckInterval, true);

	// 등장 연출 종료 후 담당자가 StartEncounter를 호출하므로, 이 시점에 보스 곡으로 전환
	PushBossBGM();

	ShowEncounterHealthBar();
	return true;
}

// 보스와 기믹 정리 후 새 보스를 생성하여 체력 및 페이즈 초기화
void ABossEncounterManager::ResetEncounter()
{
	// 연출 담당자가 레벨 상태(문, 데칼 등)를 먼저 되돌림
	if (IBossEncounterDirector* Director = GetDirector())
	{
		Director->OnEncounterReset();
	}

	// 재도전/실패로 리셋되면 보스 곡을 빼서 존 곡으로 복귀 (다음 시도 때 StartEncounter에서 다시 Push)
	PopBossBGM();

	HideEncounterHealthBar();

	++EncounterGeneration;
	EncounterState = EBossEncounterState::Resetting;
	GetWorldTimerManager().ClearTimer(PlayerCheckHandle);
	GetWorldTimerManager().ClearTimer(ResetTimerHandle);

	StopWaitForRespawn();
	ClearEncounterPlayer();
	RemoveCurrentBoss();

	if (SpawnBoss())
	{
		EncounterState = EBossEncounterState::Ready;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BossEncounter] %s: boss room initialization failed"), *GetName());
	}
}

/* 연출 담당자 */

void ABossEncounterManager::SetEncounterDirector(UObject* Director)
{
	if (Director && !Director->GetClass()->ImplementsInterface(UBossEncounterDirector::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("[BossEncounter] %s does not implement IBossEncounterDirector"), *GetNameSafe(Director));
		return;
	}

	EncounterDirector = Director;

	// 담당자보다 매니저 BeginPlay가 먼저 실행되어 보스가 이미 있는 경우에도 연결
	if (IsValid(CurrentBoss))
	{
		CurrentBoss->SetPhaseTransitionHandler(
			Director && Director->GetClass()->ImplementsInterface(UBossPhaseTransitionHandler::StaticClass()) ? Director : nullptr);

		if (IBossEncounterDirector* NewDirector = GetDirector())
		{
			NewDirector->OnBossSpawned(CurrentBoss);
		}
	}
}

IBossEncounterDirector* ABossEncounterManager::GetDirector() const
{
	return Cast<IBossEncounterDirector>(EncounterDirector.Get());
}

/* 보스 이벤트 */

void ABossEncounterManager::HandleBossDefeated(ABossCharacter* Boss)
{
	if (Boss != CurrentBoss || EncounterState != EBossEncounterState::Running)
	{
		return;
	}

	++EncounterGeneration;
	GetWorldTimerManager().ClearTimer(PlayerCheckHandle);
	ClearEncounterPlayer();
	EncounterState = EBossEncounterState::Cleared;

	// 진행 기록: 게임 흐름(트리거 제거 등)은 플래그를 구독하는 쪽이 처리
	if (!DefeatedProgressFlag.IsNone())
	{
		const UGameInstance* GameInstance = GetGameInstance();
		if (UGameProgressSubsystem* Progress = GameInstance ? GameInstance->GetSubsystem<UGameProgressSubsystem>() : nullptr)
		{
			Progress->SetProgressFlag(DefeatedProgressFlag);
		}
	}

	if (IBossEncounterDirector* Director = GetDirector())
	{
		Director->OnEncounterCleared();
	}

	// 처치 시 보스 곡 종료 → 존 곡으로 크로스페이드
	PopBossBGM();

	HideEncounterHealthBar();
}

void ABossEncounterManager::TryOpenLevelAfterDefeat(ABossCharacter* Boss)
{
	if (!HasAuthority() || !IsValid(Boss) || Boss != CurrentBoss
		|| EncounterState != EBossEncounterState::Cleared
		|| !Boss->HasFinishedDeath() || LevelAfterDefeat.IsNull())
	{
		return;
	}

	// 엔딩 연출 등으로 연출 담당자가 이동을 미루면 담당자가 연출 뒤 OpenLevelAfterDefeat 호출
	if (const IBossEncounterDirector* Director = GetDirector(); Director && Director->HoldsLevelAfterDefeat())
	{
		return;
	}

	OpenLevelAfterDefeat();
}

void ABossEncounterManager::OpenLevelAfterDefeat()
{
	if (!HasAuthority() || EncounterState != EBossEncounterState::Cleared || LevelAfterDefeat.IsNull())
	{
		return;
	}

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, LevelAfterDefeat);
}

void ABossEncounterManager::HandleBossFailed(ABossCharacter* Boss)
{
	if (Boss != CurrentBoss || EncounterState != EBossEncounterState::Running)
	{
		return;
	}

	// 서비스나 Task의 실패 통지보다 플레이어 사망을 우선 처리
	if (!IsValid(EncounterPlayer) || EncounterPlayer->GetbIsDead())
	{
		EnterWaitingState();
		return;
	}

	HideEncounterHealthBar();
	EncounterState = EBossEncounterState::Resetting;
	GetWorldTimerManager().ClearTimer(PlayerCheckHandle);

	// 실패 통지를 실행 중인 보스는 다음 Tick 이후 제거
	ScheduleReset();
}

/* 플레이어 이벤트 */

void ABossEncounterManager::HandlePlayerDied()
{
	if (EncounterState == EBossEncounterState::Running || EncounterState == EBossEncounterState::Resetting)
	{
		EnterWaitingState();
	}
}

void ABossEncounterManager::HandlePlayerRespawned(AController* Controller, APawn* NewPawn)
{
	if (EncounterState != EBossEncounterState::Waiting)
	{
		return;
	}

	// 살아 있는 플레이어 캐릭터로 부활했을 때만 초기화
	const APlayerCharacter* Player = Cast<APlayerCharacter>(NewPawn);
	if (!Player || Player->GetbIsDead())
	{
		return;
	}

	StopWaitForRespawn();
	ScheduleReset();
}

/* 내부 처리 */

// 레벨 참조를 먼저 전달한 뒤 보스 생성과 초기화 확인
bool ABossEncounterManager::SpawnBoss()
{
	if (!BossClass)
	{
		return false;
	}

	// 보스의 초기 위치는 재시도 때마다 같은 설정 사용
	const FTransform SpawnTransform = GetActorTransform();
	ABossCharacter* Boss = GetWorld()->SpawnActorDeferred<ABossCharacter>(BossClass, SpawnTransform,
		this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Boss)
	{
		return false;
	}

	CurrentBoss = Boss;
	Boss->OnDefeated.AddUniqueDynamic(this, &ABossEncounterManager::HandleBossDefeated);
	Boss->OnDeathFinished.AddUniqueDynamic(this, &ABossEncounterManager::TryOpenLevelAfterDefeat);
	Boss->OnEncounterFailed.AddUniqueDynamic(this, &ABossEncounterManager::HandleBossFailed);

	// FinishSpawning(BeginPlay) 전에 레벨 참조 전달: 보스 초기화 시 검사에 사용
	Boss->SetLevelGimmickReferences(PhaseGimmickActors, GimmickAreaCenter);
	Boss->SetLevelMovementRoutes(MovementRoutes);

	// 연출 담당자가 전환 연출도 맡으면 보스에 연결
	UObject* Director = EncounterDirector.Get();
	const bool bDirectorHandlesTransitions = Director
		&& Director->GetClass()->ImplementsInterface(UBossPhaseTransitionHandler::StaticClass());
	Boss->SetPhaseTransitionHandler(bDirectorHandlesTransitions ? Director : nullptr);

	Boss->FinishSpawning(SpawnTransform);

	if (!Boss->GetController())
	{
		Boss->SpawnDefaultController();
	}

	if (!Boss->IsEnemyInitialized() || !Cast<AEnemyAIController>(Boss->GetController()))
	{
		RemoveCurrentBoss();
		return false;
	}

	if (IBossEncounterDirector* EncounterDirectorInterface = GetDirector())
	{
		EncounterDirectorInterface->OnBossSpawned(Boss);
	}

	return true;
}

// 이벤트 해제 및 전투 정리 후 현재 보스 제거
void ABossEncounterManager::RemoveCurrentBoss()
{
	if (!IsValid(CurrentBoss))
	{
		CurrentBoss = nullptr;
		return;
	}

	CurrentBoss->OnDefeated.RemoveDynamic(this, &ABossEncounterManager::HandleBossDefeated);
	CurrentBoss->OnDeathFinished.RemoveDynamic(this, &ABossEncounterManager::TryOpenLevelAfterDefeat);
	CurrentBoss->OnEncounterFailed.RemoveDynamic(this, &ABossEncounterManager::HandleBossFailed);
	CurrentBoss->StopEncounter();
	CurrentBoss->Destroy();
	CurrentBoss = nullptr;
}

// 단일 플레이어의 생존 여부만 확인
void ABossEncounterManager::CheckPlayer()
{
	if (EncounterState != EBossEncounterState::Running)
	{
		return;
	}

	if (!IsValid(CurrentBoss))
	{
		ResetEncounter();
		return;
	}

	if (!IsValid(EncounterPlayer) || EncounterPlayer->GetbIsDead())
	{
		EnterWaitingState();
	}
}

void ABossEncounterManager::EnterWaitingState()
{
	HideEncounterHealthBar();

	// 사망 연출 중 보스를 삭제하거나 재배치하지 않고 현재 위치에서 전투만 종료
	++EncounterGeneration;
	GetWorldTimerManager().ClearTimer(PlayerCheckHandle);
	GetWorldTimerManager().ClearTimer(ResetTimerHandle);

	ClearEncounterPlayer();
	EncounterState = EBossEncounterState::Waiting;

	if (IsValid(CurrentBoss))
	{
		CurrentBoss->StopEncounter();
	}

	BeginWaitForRespawn();
}

void ABossEncounterManager::ClearEncounterPlayer()
{
	if (IsValid(EncounterPlayer))
	{
		EncounterPlayer->OnPlayerDied.RemoveDynamic(this, &ABossEncounterManager::HandlePlayerDied);
	}

	EncounterPlayer = nullptr;
}

void ABossEncounterManager::BeginWaitForRespawn()
{
	StopWaitForRespawn();

	if (!bResetWhenPlayerRespawns)
	{
		return;
	}

	// 부활은 게임모드가 담당하므로 게임모드의 부활 이벤트를 구독 (TutorialManager와 같은 방식, 매 Tick 확인 없음)
	AMainGameMode* GameMode = GetWorld()->GetAuthGameMode<AMainGameMode>();
	if (!GameMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossEncounter] %s: GameMode is not AMainGameMode. Auto reset on respawn is disabled."), *GetName());
		return;
	}

	RespawnGameMode = GameMode;
	GameMode->OnPlayerRespawned.AddUniqueDynamic(this, &ABossEncounterManager::HandlePlayerRespawned);

	// 이미 살아 있는 새 Pawn이 있다면(사망 이벤트보다 부활이 먼저 처리된 경우) 바로 초기화
	const APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (Player && !Player->GetbIsDead())
	{
		StopWaitForRespawn();
		ScheduleReset();
	}
}

void ABossEncounterManager::StopWaitForRespawn()
{
	if (AMainGameMode* GameMode = RespawnGameMode.Get())
	{
		GameMode->OnPlayerRespawned.RemoveDynamic(this, &ABossEncounterManager::HandlePlayerRespawned);
	}

	RespawnGameMode.Reset();
}

void ABossEncounterManager::ScheduleReset()
{
	const int64 ExpectedGeneration = ++EncounterGeneration;

	ResetTimerHandle = GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this, ExpectedGeneration]()
		{
			if (ExpectedGeneration != EncounterGeneration)
			{
				return;
			}

			// 초기화 직전에 플레이어가 다시 사망했다면 대기로 전환
			if (EncounterState == EBossEncounterState::Resetting
				&& IsValid(EncounterPlayer) && EncounterPlayer->GetbIsDead())
			{
				EnterWaitingState();
				return;
			}

			ResetEncounter();
		}));
}

/* 체력바 */

void ABossEncounterManager::ShowEncounterHealthBar()
{
	// 이전 전투의 UI가 남아 있으면 먼저 정리
	HideEncounterHealthBar();

	if (!IsValid(EncounterPlayer) || !IsValid(CurrentBoss))
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(EncounterPlayer->GetController());
	AMainHUD* HUD = PlayerController && PlayerController->IsLocalController() ? Cast<AMainHUD>(PlayerController->GetHUD()) : nullptr;
	if (!IsValid(HUD))
	{
		return;
	}

	EncounterHUD = HUD;
	HUD->ShowBossHealthBar(CurrentBoss);
}

void ABossEncounterManager::HideEncounterHealthBar()
{
	if (AMainHUD* HUD = EncounterHUD.Get())
	{
		HUD->HideBossHealthBar();
	}

	EncounterHUD.Reset();
}

/* BGM */

void ABossEncounterManager::PushBossBGM()
{
	if (nullptr == BossBGM)
	{
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UBGMSubSystem* BGM = GI->GetSubsystem<UBGMSubSystem>())
		{
			// Owner = 이 매니저: 같은 Owner로 다시 Push해도 중복 없이 교체됨
			BGM->PushEventBGM(this, BossBGM, 0.5f); // 마지막 인자는 크로스페이드 초 (추후로 변수로 빼야함)
		}
	}
}

void ABossEncounterManager::PopBossBGM()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UBGMSubSystem* BGM = GI->GetSubsystem<UBGMSubSystem>())
		{
			// 스택에 없으면 아무 일도 안 일어나므로 여러 번 불러도 안전
			BGM->PopEventBGM(this, 0.5f); // 마지막 인자는 크로스페이드 초 (추후로 변수로 빼야함)
		}
	}
}
