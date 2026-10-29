/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 게임 진행상태 서브시스템 구현

#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Kismet/GameplayStatics.h"
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

bool UGameProgressSubsystem::DiscoverCheckpoint(const UObject* WorldContextObject, FName CheckpointId, const FText& DisplayName, const FTransform& RespawnTransform)
{
	const FString LevelName = GetLevelName(WorldContextObject);
	FCheckpointSaveData* Found = CurrentSave->DiscoveredCheckpoints.FindByPredicate([&](const FCheckpointSaveData& Data)
	{
		return Data.LevelName == LevelName && Data.CheckpointId == CheckpointId;
	});

	FCheckpointSaveData& Data = Found ? *Found : CurrentSave->DiscoveredCheckpoints.AddDefaulted_GetRef();
	Data.bValid = true;
	Data.LevelName = LevelName;
	Data.CheckpointId = CheckpointId;
	Data.DisplayName = DisplayName;
	Data.RespawnTransform = RespawnTransform;
	return Found == nullptr;
}

void UGameProgressSubsystem::UnlockCheckpoint(FName LevelName, FName CheckpointId, const FText& DisplayName, bool bSave)
{
	const FString Level = LevelName.ToString();
	const bool bAlreadyKnown = CurrentSave->DiscoveredCheckpoints.ContainsByPredicate([&](const FCheckpointSaveData& D)
		{
			return D.LevelName == Level && D.CheckpointId == CheckpointId;
		});
	if (bAlreadyKnown) return;

	FCheckpointSaveData& Data = CurrentSave->DiscoveredCheckpoints.AddDefaulted_GetRef();
	Data.bValid = true;
	Data.LevelName = Level;
	Data.CheckpointId = CheckpointId;
	Data.DisplayName = DisplayName;  

	if (bSave) SaveProgress();
}

void UGameProgressSubsystem::SetCheckpointData(const FCheckpointSaveData& Data, bool bSave)
{
	CurrentSave->LastCheckpoint = Data;
	OnCheckpointChanged.Broadcast(Data.CheckpointId);

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

FName UGameProgressSubsystem::MakeGimmickKey(const UObject* WorldContextObject, FName GimmickId)
{
	return FName(*FString::Printf(TEXT("%s/%s"), *GetLevelName(WorldContextObject), *GimmickId.ToString()));
}

void UGameProgressSubsystem::SetGimmickState(const UObject* WorldContextObject, FName GimmickId, bool bActivated)
{
	if (GimmickId.IsNone())
	{
		return;
	}
	CurrentSave->GimmickStates.Add(MakeGimmickKey(WorldContextObject, GimmickId), bActivated);
}

bool UGameProgressSubsystem::GetGimmickState(const UObject* WorldContextObject, FName GimmickId, bool& bOutActivated) const
{
	bOutActivated = false;
	if (GimmickId.IsNone())
	{
		return false;
	}

	const bool* Found = CurrentSave->GimmickStates.Find(MakeGimmickKey(WorldContextObject, GimmickId));
	if (!Found)
	{
		return false;
	}

	bOutActivated = *Found;
	return true;
}

bool UGameProgressSubsystem::SaveProgress()
{
	// 맵 기믹 등이 현재 상태를 기록
	OnCollectSaveData.Broadcast(CurrentSave);

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
