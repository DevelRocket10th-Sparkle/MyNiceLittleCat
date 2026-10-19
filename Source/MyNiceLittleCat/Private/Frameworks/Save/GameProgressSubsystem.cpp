/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 게임 진행상태 서브시스템 구현

#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

void UGameProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!LoadProgress())
	{
		CurrentSave = Cast<UGameProgressSaveGame>(UGameplayStatics::CreateSaveGameObject(UGameProgressSaveGame::StaticClass()));
	}
}

FString UGameProgressSubsystem::GetLevelName(const UObject* WorldContextObject)
{
	// PIE 접두사(UEDPIE_0_)를 뺀 맵 이름
	return UGameplayStatics::GetCurrentLevelName(WorldContextObject, true);
}

void UGameProgressSubsystem::SetCheckpoint(const UObject* WorldContextObject, FName CheckpointId, const FTransform& RespawnTransform, bool bSave)
{
	FCheckpointSaveData& Data = CurrentSave->LastCheckpoint;
	Data.bValid = true;
	Data.LevelName = GetLevelName(WorldContextObject);
	Data.CheckpointId = CheckpointId;
	Data.RespawnTransform = RespawnTransform;

	OnCheckpointChanged.Broadcast(CheckpointId);

	if (bSave)
	{
		SaveProgress();
	}
}

bool UGameProgressSubsystem::IsCurrentCheckpoint(const UObject* WorldContextObject, FName CheckpointId) const
{
	const FCheckpointSaveData& Data = CurrentSave->LastCheckpoint;
	return Data.bValid && Data.CheckpointId == CheckpointId && Data.LevelName == GetLevelName(WorldContextObject);
}

bool UGameProgressSubsystem::GetRespawnTransform(const UObject* WorldContextObject, FTransform& OutTransform) const
{
	const FCheckpointSaveData& Data = CurrentSave->LastCheckpoint;
	if (!Data.bValid || Data.LevelName != GetLevelName(WorldContextObject))
	{
		return false;
	}

	OutTransform = Data.RespawnTransform;
	return true;
}

bool UGameProgressSubsystem::RespawnPlayer(AController* Controller)
{
	if (!IsValid(Controller))
	{
		return false;
	}

	UWorld* World = Controller->GetWorld();
	AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	if (!GameMode)
	{
		return false;
	}

	// 기존 폰이 남아 있으면 RestartPlayer가 새로 스폰하지 않으므로 먼저 제거
	if (APawn* OldPawn = Controller->GetPawn())
	{
		OldPawn->DetachFromControllerPendingDestroy();
		OldPawn->Destroy();
	}

	// 게임 모드별 처리(펫 재연결 등)를 그대로 쓰기 위해 RestartPlayer 사용 → PlayerStart에 스폰됨
	GameMode->RestartPlayer(Controller);

	APawn* NewPawn = Controller->GetPawn();
	if (!NewPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameProgress] RespawnPlayer: RestartPlayer did not spawn a pawn"));
		return false;
	}

	// 체크포인트가 있으면 그 위치로 이동 (부활 지점은 발 위치 → 캡슐 절반 높이만큼 올림)
	FTransform RespawnTransform;
	if (GetRespawnTransform(Controller, RespawnTransform))
	{
		FVector Location = RespawnTransform.GetLocation();
		if (const ACharacter* Character = Cast<ACharacter>(NewPawn))
		{
			Location.Z += Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}
		const FRotator Rotation(0.f, RespawnTransform.Rotator().Yaw, 0.f);

		if (!NewPawn->TeleportTo(Location, Rotation))
		{
			UE_LOG(LogTemp, Warning, TEXT("[GameProgress] RespawnPlayer: teleport to checkpoint %s failed"), *CurrentSave->LastCheckpoint.CheckpointId.ToString());
		}
		Controller->SetControlRotation(Rotation);
	}

	OnPlayerRespawned.Broadcast(Controller, NewPawn);
	return true;
}

void UGameProgressSubsystem::SetProgressFlag(FName Flag, bool bEnabled)
{
	if (bEnabled)
	{
		CurrentSave->ProgressFlags.Add(Flag);
	}
	else
	{
		CurrentSave->ProgressFlags.Remove(Flag);
	}
}

bool UGameProgressSubsystem::HasProgressFlag(FName Flag) const
{
	return CurrentSave->ProgressFlags.Contains(Flag);
}

bool UGameProgressSubsystem::SaveProgress()
{
	CurrentSave->SavedTime = FDateTime::Now();

	const bool bSuccess = UGameplayStatics::SaveGameToSlot(CurrentSave, SaveSlotName, SaveUserIndex);
	UE_LOG(LogTemp, Log, TEXT("[GameProgress] Save %s (checkpoint %s / %s)"),
		bSuccess ? TEXT("succeeded") : TEXT("failed"),
		*CurrentSave->LastCheckpoint.LevelName, *CurrentSave->LastCheckpoint.CheckpointId.ToString());

	OnProgressSaved.Broadcast(bSuccess);
	return bSuccess;
}

bool UGameProgressSubsystem::LoadProgress()
{
	if (!HasSaveSlot())
	{
		return false;
	}

	UGameProgressSaveGame* Loaded = Cast<UGameProgressSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));
	if (!Loaded)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameProgress] Load failed: %s"), *SaveSlotName);
		return false;
	}

	CurrentSave = Loaded;
	OnCheckpointChanged.Broadcast(CurrentSave->LastCheckpoint.CheckpointId);
	return true;
}

void UGameProgressSubsystem::ResetProgress()
{
	if (HasSaveSlot())
	{
		UGameplayStatics::DeleteGameInSlot(SaveSlotName, SaveUserIndex);
	}

	CurrentSave = Cast<UGameProgressSaveGame>(UGameplayStatics::CreateSaveGameObject(UGameProgressSaveGame::StaticClass()));
	OnCheckpointChanged.Broadcast(NAME_None);
}

bool UGameProgressSubsystem::HasSaveSlot() const
{
	return UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex);
}
