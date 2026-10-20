/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 실제 게임용 게임 모드 구현

#include "Frameworks/GameMode/MainGameMode.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Pet/PetCharacter.h"
#include "Characters/PlayerMainController.h"
#include "Characters/PlayerCharacter.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"

AMainGameMode::AMainGameMode()
{
	// 부활 전환: 사망 장면에서 소리도 함께 줄이고, 어두운 상태는 짧게
	RespawnTransition.HoldTime = 0.5f;
	RespawnTransition.bFadeAudio = true;
}

void AMainGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!IsValid(NewPlayer))
	{
		return;
	}

	FTransform SpawnTransform;
	const bool bUseCheckpoint = (bRespawning || bSpawnAtCheckpointOnStart)
		&& NewPlayer->GetPawn() == nullptr
		&& GetCheckpointSpawnTransform(NewPlayer, SpawnTransform);

	if (bUseCheckpoint)
	{
		RestartPlayerAtTransform(NewPlayer, SpawnTransform);
	}
	else
	{
		Super::RestartPlayer(NewPlayer);
	}

	SetupPet(NewPlayer);

	// 새 폰의 사망 알림 구독 (이전 폰은 리스폰 때 제거되므로 해제 불필요)
	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(NewPlayer->GetPawn()))
	{
		RespawnController = NewPlayer;
		PlayerCharacter->OnPlayerDied.AddUniqueDynamic(this, &AMainGameMode::HandlePlayerDied);
	}
}

void AMainGameMode::HandlePlayerDied()
{
	// OnPlayerDied는 파라미터가 없어 구독할 때 저장한 컨트롤러 사용 (싱글 플레이어 기준)
	OnPlayerDeath(RespawnController.Get());
}

void AMainGameMode::OnPlayerDeath(AController* DeadController)
{
	if (!bAutoRespawn || !IsValid(DeadController))
	{
		return;
	}

	FTimerDelegate RespawnDelegate = FTimerDelegate::CreateWeakLambda(this, [this, WeakController = TWeakObjectPtr<AController>(DeadController)]()
	{
		BeginRespawn(WeakController.Get());
	});

	if (RespawnDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(RespawnTimerHandle, RespawnDelegate, RespawnDelay, false);
	}
	else
	{
		RespawnDelegate.Execute();
	}
}

void AMainGameMode::BeginRespawn(AController* Controller)
{
	if (!IsValid(Controller))
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	UScreenTransitionSubsystem* Transition = GetWorld()->GetSubsystem<UScreenTransitionSubsystem>();
	if (!bUseRespawnTransition || !PlayerController || !Transition)
	{
		UE_LOG(LogTemp, Log, TEXT("[MainGameMode] Respawn without transition (Use %d, PlayerController %d, Subsystem %d)"),
			bUseRespawnTransition ? 1 : 0, PlayerController ? 1 : 0, Transition ? 1 : 0);
		RespawnPlayer(Controller);
		return;
	}

	// 체크포인트 휴식 등 다른 전환 중에 죽으면 부활을 우선
	if (Transition->IsTransitionPlaying())
	{
		Transition->CancelTransition();
	}

	// 화면이 완전히 어두워진 순간 폰 교체 (폰 제거·체크포인트 스폰·카메라 이동이 보이지 않게)
	TWeakObjectPtr<AMainGameMode> WeakThis(this);
	TWeakObjectPtr<AController> WeakController(Controller);
	const bool bStarted = Transition->PlayTransition(PlayerController, RespawnTransition,
		[WeakThis, WeakController]()
		{
			if (AMainGameMode* GameMode = WeakThis.Get())
			{
				GameMode->RespawnPlayer(WeakController.Get());
			}
		});

	// 전환을 시작하지 못하면 부활이 막히지 않게 바로 부활
	if (!bStarted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MainGameMode] Respawn transition failed to start, respawning immediately"));
		RespawnPlayer(Controller);
	}
}

bool AMainGameMode::RespawnPlayer(AController* Controller)
{
	if (!IsValid(Controller))
	{
		return false;
	}

	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);

	// 기존 폰이 남아 있으면 RestartPlayer가 새로 스폰하지 않으므로 먼저 제거
	if (APawn* OldPawn = Controller->GetPawn())
	{
		OldPawn->DetachFromControllerPendingDestroy();
		OldPawn->Destroy();
	}

	bRespawning = true;
	RestartPlayer(Controller);
	bRespawning = false;

	APawn* NewPawn = Controller->GetPawn();
	if (!NewPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MainGameMode] RespawnPlayer: no pawn spawned"));
		return false;
	}

	OnPlayerRespawned.Broadcast(Controller, NewPawn);
	return true;
}

bool AMainGameMode::GetCheckpointSpawnTransform(AController* Controller, FTransform& OutTransform)
{
	const UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>();
	FTransform RespawnTransform;
	if (!Progress || !Progress->GetRespawnTransform(this, RespawnTransform))
	{
		return false;
	}

	// 부활 지점은 발 위치 → 스폰할 캐릭터의 캡슐 절반 높이만큼 올림
	FVector Location = RespawnTransform.GetLocation();
	if (const UClass* PawnClass = GetDefaultPawnClassForController(Controller))
	{
		if (const ACharacter* CharacterCDO = Cast<ACharacter>(PawnClass->GetDefaultObject()))
		{
			Location.Z += CharacterCDO->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}
	}

	OutTransform = FTransform(FRotator(0.f, RespawnTransform.Rotator().Yaw, 0.f), Location);
	return true;
}

void AMainGameMode::SetupPet(AController* Controller)
{
	// ATestPetGameMode::RestartPlayer에서 이식 (펫 담당 코드 기준)
	APawn* PlayerPawn = Controller->GetPawn();
	if (!IsValid(PlayerPawn))
	{
		return;
	}

	if (!IsValid(Pet))
	{
		if (!PetClass)
		{
			return;
		}

		// 펫 스폰하기
		const FVector SpawnLocation = PlayerPawn->GetActorLocation() + PlayerPawn->GetActorRightVector() * 300.f;

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

		Pet = GetWorld()->SpawnActor<APetCharacter>(PetClass, SpawnLocation, PlayerPawn->GetActorRotation(), SpawnParams);
	}

	if (!IsValid(Pet))
	{
		return;
	}

	if (!Pet->GetController())
	{
		Pet->SpawnDefaultController();
	}

	// 펫 초기화 데이터로 새 플레이어 폰과 연결 (리스폰 시에도 다시 호출)
	FPetInitializationData InitData;
	InitData.PartnerPlayer = PlayerPawn;
	InitData.InitialBehavior = PetInitialBehavior;
	Pet->InitializePet(InitData);

	if (APlayerMainController* PlayerController = Cast<APlayerMainController>(Controller))
	{
		PlayerController->SetPartnerPet(Pet);
	}
}
