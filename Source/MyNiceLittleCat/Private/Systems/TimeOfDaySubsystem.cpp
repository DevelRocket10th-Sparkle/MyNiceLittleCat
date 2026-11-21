// Fill out your copyright notice in the Description page of Project Settings.


#include "Systems/TimeOfDaySubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Systems/TimeOfDayController.h"

namespace
{
	UGameProgressSubsystem* GetProgress(const UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UGameProgressSubsystem>() : nullptr;
	}
}

void UTimeOfDaySubsystem::Deinitialize()
{
	// 게임 인스턴스 서브시스템이 월드보다 오래 살아 있으므로 구독을 남기지 않음
	if (UGameProgressSubsystem* Progress = GetProgress(GetWorld()); Progress && SaveReplacedHandle.IsValid())
	{
		Progress->OnSaveDataReplaced.Remove(SaveReplacedHandle);
	}
	SaveReplacedHandle.Reset();

	Super::Deinitialize();
}

/* 컨트롤러 */

void UTimeOfDaySubsystem::RegisterController(ATimeOfDayController* Controller)
{
	if (!IsValid(Controller))
	{
		return;
	}
	ActiveController = Controller;

	// 저장을 다시 불러오면 체크포인트 시간대로 되돌림 (한 번만 구독)
	if (UGameProgressSubsystem* Progress = GetProgress(GetWorld()); Progress && !SaveReplacedHandle.IsValid())
	{
		SaveReplacedHandle = Progress->OnSaveDataReplaced.AddUObject(this, &UTimeOfDaySubsystem::RestoreFromCheckpoint);
	}

	RestoreFromCheckpoint();
}

void UTimeOfDaySubsystem::UnregisterController(ATimeOfDayController* Controller)
{
	if (ActiveController.Get() == Controller)
	{
		ActiveController.Reset();
	}
}

/* 시간대 */

void UTimeOfDaySubsystem::SetTimeOfDay(FGameplayTag TimeOfDay)
{
	CurrentTimeOfDay = TimeOfDay;

	if (ATimeOfDayController* Controller = ActiveController.Get())
	{
		Controller->ApplyTimeOfDay(CurrentTimeOfDay);
	}
}

void UTimeOfDaySubsystem::RestoreFromCheckpoint()
{
	// 같은 레벨에서 저장한 체크포인트일 때만 그 시간대 (다른 레벨이거나 없으면 기본)
	const UGameProgressSubsystem* Progress = GetProgress(GetWorld());
	const FCheckpointSaveData* Checkpoint = Progress ? &Progress->GetCheckpointData() : nullptr;
	const bool bSameLevel = Checkpoint && Checkpoint->bValid
		&& Checkpoint->LevelName == UGameplayStatics::GetCurrentLevelName(this, true);

	SetTimeOfDay(bSameLevel ? Checkpoint->TimeOfDay : FGameplayTag());
}
