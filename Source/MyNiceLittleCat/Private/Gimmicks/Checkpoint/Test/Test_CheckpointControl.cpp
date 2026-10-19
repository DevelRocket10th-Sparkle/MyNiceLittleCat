/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 체크포인트 테스트용 조작 액터 구현

#include "Gimmicks/Checkpoint/Test/Test_CheckpointControl.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Frameworks/GameMode/MainGameMode.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

ATest_CheckpointControl::ATest_CheckpointControl()
{
	PrimaryActorTick.bCanEverTick = true;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ATest_CheckpointControl::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController || !bBindTestKeys)
	{
		return;
	}

	EnableInput(PlayerController);
	if (InputComponent)
	{
		InputComponent->BindKey(RespawnKey, IE_Pressed, this, &ATest_CheckpointControl::RespawnPlayer);
		InputComponent->BindKey(ResetProgressKey, IE_Pressed, this, &ATest_CheckpointControl::ResetProgress);
	}
}

void ATest_CheckpointControl::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bShowDebugText || !GEngine)
	{
		return;
	}

	const UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>();
	if (!Progress)
	{
		return;
	}

	const FCheckpointSaveData& Data = Progress->GetCheckpointData();
	const FString Text = FString::Printf(TEXT("[Checkpoint Test] %s: Die -> Respawn  %s: Reset save\nCheckpoint: %s / %s  |  Save slot: %s"),
		*RespawnKey.GetDisplayName().ToString(), *ResetProgressKey.GetDisplayName().ToString(),
		Data.bValid ? *Data.LevelName : TEXT("-"), Data.bValid ? *Data.CheckpointId.ToString() : TEXT("-"),
		Progress->HasSaveSlot() ? TEXT("exists") : TEXT("none"));
	GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this), 0.f, FColor::Cyan, Text);
}

void ATest_CheckpointControl::RespawnPlayer()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	// 사망 흉내: 게임 모드의 사망 처리 → RespawnDelay 뒤 리스폰
	AMainGameMode* GameMode = World->GetAuthGameMode<AMainGameMode>();
	if (!GameMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Checkpoint Test] GameMode is not AMainGameMode"));
		return;
	}
	GameMode->OnPlayerDeath(World->GetFirstPlayerController());
}

void ATest_CheckpointControl::ResetProgress()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	if (UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>())
	{
		Progress->ResetProgress();
	}
}
