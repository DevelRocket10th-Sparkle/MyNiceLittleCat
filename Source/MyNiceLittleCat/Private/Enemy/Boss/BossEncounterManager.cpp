#include "Enemy/Boss/BossEncounterManager.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/AI/EnemyAIController.h"
#include "Characters/PlayerCharacter.h"
#include "Components/SceneComponent.h"
#include "BrainComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ABossEncounterManager::ABossEncounterManager()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

// 컴포넌트 초기화 후 실행 준비
void ABossEncounterManager::BeginPlay()
{
	Super::BeginPlay();
	ResetEncounter();
}

// 레벨 기믹 참조를 먼저 전달한 뒤 보스 생성과 초기화 확인
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
	Boss->OnEncounterFailed.AddUniqueDynamic(this, &ABossEncounterManager::HandleBossFailed);
	// 보스에 페이즈에서 사용하는 기믹 관련 요소 설정
	Boss->SetLevelGimmickReferences(PhaseGimmickActors, GimmickAreaCenter);
	// 보스에 페이즈별 이동 경로 설정
	Boss->SetLevelMovementRoutes(MovementRoutes);
	// 보스 스폰 완료 처리
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
	CurrentBoss->OnEncounterFailed.RemoveDynamic(this, &ABossEncounterManager::HandleBossFailed);
	CurrentBoss->StopEncounter();
	CurrentBoss->Destroy();
	CurrentBoss = nullptr;
}

// 보스와 기믹 정리 후 새 보스를 생성하여 체력 및 페이즈 초기화
void ABossEncounterManager::ResetEncounter()
{
	++EncounterGeneration;
	EncounterState = EBossEncounterState::Resetting;
	GetWorld()->GetTimerManager().ClearTimer(PlayerCheckHandle);
	GetWorld()->GetTimerManager().ClearTimer(ResetTimerHandle);
	ClearEncounterPlayer();
	RemoveCurrentBoss();
	if (SpawnBoss())
	{
		EncounterState = EBossEncounterState::Ready;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Boss room initialization failed: %s"), *GetName());
	}
}

// 준비된 보스방의 첫 페이즈와 타깃 선정 시작
bool ABossEncounterManager::StartEncounter(APlayerCharacter* Player)
{
	if (EncounterState != EBossEncounterState::Ready || !IsValid(CurrentBoss)
		|| !IsValid(Player) || Player->GetbIsDead()) return false;
	EncounterPlayer = Player;
	EncounterPlayer->OnPlayerDied.AddUniqueDynamic(this, &ABossEncounterManager::HandlePlayerDied);
	AEnemyAIController* AI = Cast<AEnemyAIController>(CurrentBoss->GetController());
	if (!AI || !CurrentBoss->StartEncounter())
	{
		ResetEncounter();
		return false;
	}
	if (!AI->GetBrainComponent() || !AI->GetBrainComponent()->IsRunning())
	{
		ResetEncounter();
		return false;
	}
	EncounterState = EBossEncounterState::Running;
	AI->SetCombatTarget(EncounterPlayer);
	// 타이머가 만들어진 전투 번호를 저장하여 이전 전투의 호출 무시
	const int64 StartedGeneration = EncounterGeneration;
	FTimerDelegate CheckDelegate;
	CheckDelegate.BindWeakLambda(this, [this, StartedGeneration]()
	{
		if (StartedGeneration == EncounterGeneration)
		{
			CheckPlayer();
		}
	});
	GetWorld()->GetTimerManager().SetTimer(PlayerCheckHandle, CheckDelegate, 0.5f, true);
	return true;
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
	if (IsValid(EncounterPlayer) && !EncounterPlayer->GetbIsDead())
	{
		return;
	}
	EnterWaitingState();
}

void ABossEncounterManager::HandlePlayerDied()
{
	if (EncounterState == EBossEncounterState::Running || EncounterState == EBossEncounterState::Resetting)
		EnterWaitingState();
}

void ABossEncounterManager::ClearEncounterPlayer()
{
	if (IsValid(EncounterPlayer))
		EncounterPlayer->OnPlayerDied.RemoveDynamic(this, &ABossEncounterManager::HandlePlayerDied);
	EncounterPlayer = nullptr;
}

void ABossEncounterManager::EnterWaitingState()
{
	// 사망 연출 중 보스를 삭제하거나 재배치하지 않고 현재 위치에서 전투만 종료
	++EncounterGeneration;
	GetWorld()->GetTimerManager().ClearTimer(PlayerCheckHandle);
	GetWorld()->GetTimerManager().ClearTimer(ResetTimerHandle);
	ClearEncounterPlayer();
	EncounterState = EBossEncounterState::Waiting;
	if (IsValid(CurrentBoss))
	{
		CurrentBoss->StopEncounter();
	}
}

void ABossEncounterManager::HandleBossDefeated(ABossCharacter* Boss)
{
	if (Boss != CurrentBoss || EncounterState != EBossEncounterState::Running)
	{
		return;
	}
	++EncounterGeneration;
	GetWorld()->GetTimerManager().ClearTimer(PlayerCheckHandle);
	ClearEncounterPlayer();
	EncounterState = EBossEncounterState::Cleared;
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
	EncounterState = EBossEncounterState::Resetting;
	GetWorld()->GetTimerManager().ClearTimer(PlayerCheckHandle);
	// 다음 Tick까지 사망 구독을 유지하여 초기화 예약을 취소할 수 있게 함
	// 실패 통지를 실행 중인 보스는 다음 Tick 이후 제거
	const int64 ExpectedGeneration = ++EncounterGeneration;
	ResetTimerHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this, ExpectedGeneration]()
		{
			if (ExpectedGeneration != EncounterGeneration || EncounterState != EBossEncounterState::Resetting)
			{
				return;
			}
			if (!IsValid(EncounterPlayer) || EncounterPlayer->GetbIsDead())
			{
				EnterWaitingState();
			}
			else ResetEncounter();
		}));
}

// 플레이 종료 시 실행, 구독 및 직접 소유한 자원 정리 후 부모 호출
void ABossEncounterManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	++EncounterGeneration;
	GetWorld()->GetTimerManager().ClearTimer(PlayerCheckHandle);
	GetWorld()->GetTimerManager().ClearTimer(ResetTimerHandle);
	ClearEncounterPlayer();
	RemoveCurrentBoss();
	Super::EndPlay(EndPlayReason);
}
