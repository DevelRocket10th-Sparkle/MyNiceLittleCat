/// 작성자: 최요환
/// 작성일자: 2026-11-03
///
/// 잠금 전투 기믹 구현

#include "Gimmicks/Stage/GimmickEncounter.h"
#include "Characters/PlayerCharacter.h"
#include "Enemy/EnemyCharacter.h"
#include "Interfaces/ActivatableInterface.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AGimmickEncounter::AGimmickEncounter()
{
	// 전투는 플레이어 진입으로만 시작, 클리어 상태는 저장
	bReceiveSignals = false;
	bSaveState = true;

	// 감지는 주기 검사로 하므로 콜리전 없음 (레이저·이동을 막지 않음)
	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	SetRootComponent(TriggerVolume);
	TriggerVolume->SetBoxExtent(FVector(500.f, 500.f, 200.f));
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TriggerVolume->SetGenerateOverlapEvents(false);
	TriggerVolume->SetCanEverAffectNavigation(false);
	TriggerVolume->SetHiddenInGame(true);
	TriggerVolume->ShapeColor = FColor(255, 90, 60);
}

void AGimmickEncounter::BeginPlay()
{
	Super::BeginPlay();

	// 저장된 클리어 상태로 시작: 남은 몬스터 정리, 잠금 대상은 처음부터 열린 상태 그대로
	if (IsActivated())
	{
		if (bDestroyEnemiesWhenRestoredCleared)
		{
			for (AEnemyCharacter* Enemy : Enemies)
			{
				if (IsValid(Enemy))
				{
					Enemy->Destroy();
				}
			}
		}
		return;
	}

	if (Enemies.IsEmpty())
	{
		CollectEnemiesInVolume();
	}

	BindEnemies();
	StartWatching();
}

void AGimmickEncounter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopWatching();
	UnbindEnemies();
	UnbindPlayer();

	Super::EndPlay(EndPlayReason);
}

void AGimmickEncounter::CollectEnemiesInVolume()
{
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AEnemyCharacter* Enemy = *It;
		if (IsValid(Enemy) && Enemy->IsAlive() && IsInsideVolume(Enemy->GetActorLocation()))
		{
			Enemies.Add(Enemy);
		}
	}
}

void AGimmickEncounter::BindEnemies()
{
	AliveEnemies.Reset();
	for (AEnemyCharacter* Enemy : Enemies)
	{
		if (IsValid(Enemy) && Enemy->IsAlive())
		{
			Enemy->OnDeathStarted.AddUObject(this, &AGimmickEncounter::HandleEnemyDeathStarted);
			AliveEnemies.Add(Enemy);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[GimmickEncounter] %s: 몬스터 %d마리 등록"), *GetName(), AliveEnemies.Num());
}

void AGimmickEncounter::UnbindEnemies()
{
	for (const TWeakObjectPtr<AEnemyCharacter>& Enemy : AliveEnemies)
	{
		if (Enemy.IsValid())
		{
			Enemy->OnDeathStarted.RemoveAll(this);
		}
	}
	AliveEnemies.Reset();
}

int32 AGimmickEncounter::GetRemainingEnemyCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AEnemyCharacter>& Enemy : AliveEnemies)
	{
		if (Enemy.IsValid() && Enemy->IsAlive())
		{
			++Count;
		}
	}
	return Count;
}

void AGimmickEncounter::StartWatching()
{
	GetWorldTimerManager().SetTimer(WatchTimer, this, &AGimmickEncounter::CheckForPlayer, CheckInterval, true);
}

void AGimmickEncounter::StopWatching()
{
	GetWorldTimerManager().ClearTimer(WatchTimer);
}

void AGimmickEncounter::CheckForPlayer()
{
	if (bEncounterActive || IsActivated())
	{
		StopWatching();
		return;
	}

	for (TActorIterator<APlayerCharacter> It(GetWorld()); It; ++It)
	{
		APlayerCharacter* Player = *It;
		if (IsValid(Player) && IsInsideVolume(Player->GetActorLocation()))
		{
			StartEncounter(Player);
			return;
		}
	}
}

bool AGimmickEncounter::IsInsideVolume(const FVector& WorldLocation) const
{
	if (!TriggerVolume)
	{
		return false;
	}

	// 회전·스케일을 반영해 박스 로컬 공간에서 비교
	const FVector Local = TriggerVolume->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = TriggerVolume->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

void AGimmickEncounter::StartEncounter(AActor* EncounterInstigator)
{
	if (bEncounterActive || IsActivated())
	{
		return;
	}

	StopWatching();

	if (GetRemainingEnemyCount() == 0)
	{
		ClearEncounter();
		return;
	}

	bEncounterActive = true;
	SetLockActorsOpen(false);
	BindPlayer(Cast<APlayerCharacter>(EncounterInstigator));

	UE_LOG(LogTemp, Log, TEXT("[GimmickEncounter] %s: 전투 시작 (남은 몬스터 %d)"), *GetName(), GetRemainingEnemyCount());
	K2_OnEncounterStarted();
}

void AGimmickEncounter::HandleEnemyDeathStarted(AEnemyCharacter* Enemy)
{
	if (Enemy)
	{
		Enemy->OnDeathStarted.RemoveAll(this);
	}
	AliveEnemies.RemoveAll([Enemy](const TWeakObjectPtr<AEnemyCharacter>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Enemy;
	});

	// 전투 전에 끌려 나와 잡힌 몬스터는 목록에서만 빠지고, 진입하면 남은 수로 판단
	if (bEncounterActive && GetRemainingEnemyCount() == 0)
	{
		ClearEncounter();
	}
}

void AGimmickEncounter::ClearEncounter()
{
	bEncounterActive = false;
	StopWatching();
	UnbindPlayer();
	SetLockActorsOpen(true);

	UE_LOG(LogTemp, Log, TEXT("[GimmickEncounter] %s: 전투 클리어"), *GetName());

	// 켜짐 = 클리어 (LinkedActors 신호, 세이브)
	SetActivated(true, this);
	K2_OnEncounterCleared();
}

void AGimmickEncounter::ResetEncounter()
{
	if (!bEncounterActive)
	{
		return;
	}

	bEncounterActive = false;
	UnbindPlayer();
	SetLockActorsOpen(true);

	UE_LOG(LogTemp, Log, TEXT("[GimmickEncounter] %s: 전투 중단 (남은 몬스터 %d)"), *GetName(), GetRemainingEnemyCount());
	K2_OnEncounterReset();

	StartWatching();
}

void AGimmickEncounter::SetLockActorsOpen(bool bOpen)
{
	for (AActor* LockActor : LockActors)
	{
		if (!IsValid(LockActor) || !LockActor->Implements<UActivatableInterface>())
		{
			continue;
		}

		if (bOpen)
		{
			IActivatableInterface::Execute_Activate(LockActor, this);
		}
		else
		{
			IActivatableInterface::Execute_Deactivate(LockActor, this);
		}
	}
}

void AGimmickEncounter::BindPlayer(APlayerCharacter* Player)
{
	UnbindPlayer();

	if (bResetOnPlayerDeath && IsValid(Player))
	{
		Player->OnPlayerDied.AddUniqueDynamic(this, &AGimmickEncounter::HandlePlayerDied);
		BoundPlayer = Player;
	}
}

void AGimmickEncounter::UnbindPlayer()
{
	if (BoundPlayer.IsValid())
	{
		BoundPlayer->OnPlayerDied.RemoveDynamic(this, &AGimmickEncounter::HandlePlayerDied);
	}
	BoundPlayer.Reset();
}

void AGimmickEncounter::HandlePlayerDied()
{
	ResetEncounter();
}
