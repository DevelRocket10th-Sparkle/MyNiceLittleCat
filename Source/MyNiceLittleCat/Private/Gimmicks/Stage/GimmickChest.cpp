/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 보물 상자 구현

#include "Gimmicks/Stage/GimmickChest.h"
#include "Gimmicks/GimmickMoverComponent.h"
#include "Items/InventorySubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameInstance.h"
#include "WIdgets/Item/ItemAcquireWidget.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"

AGimmickChest::AGimmickChest()
{
	// 한 번 열면 끝, 연 상태 저장, 신호로는 열리지 않음
	bShowToggleableOption = false;
	bToggleable = false;
	bSaveState = true;
	bReceiveSignals = false;

	// 몸통(100cm 큐브 기준) 뒤쪽 윗모서리 = 경첩, 뚜껑이 위로 젖혀짐
	LidMover = CreateDefaultSubobject<UGimmickMoverComponent>(TEXT("LidMover"));
	LidMover->SetupAttachment(Mesh);
	LidMover->SetRelativeLocation(FVector(-50.f, 0.f, 50.f));
	LidMover->OpenRotation = FRotator(100.f, 0.f, 0.f);
	LidMover->OpenDuration = 0.6f;

	LidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LidMesh"));
	LidMesh->SetupAttachment(LidMover);
	LidMesh->SetMobility(EComponentMobility::Movable);
	LidMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

void AGimmickChest::BeginPlay()
{
	Super::BeginPlay();

	if (LidMover)
	{
		LidMover->OnMoveFinished.AddDynamic(this, &AGimmickChest::HandleLidMoveFinished);
	}
}

void AGimmickChest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 연출 도중 사라지면 일시정지·입력을 남기지 않음
	if (bSequenceActive)
	{
		FinishAcquireSequence();
	}
	FTSTicker::GetCoreTicker().RemoveTicker(RevealTickerHandle);

	Super::EndPlay(EndPlayReason);
}

void AGimmickChest::HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator)
{
	Super::HandleActivationChanged(bNewActivated, ChangeInstigator);

	// 저장된 열린 상태로 시작할 때는 아이템을 다시 주지 않고 연출도 없음
	if (!bNewActivated || IsApplyingInitialState())
	{
		return;
	}

	// 아이템은 여는 순간 지급, UI는 연출 순서에 맞춰 따로 표시
	TArray<FItemStack> Acquired;
	UGameInstance* GameInstance = GetGameInstance();
	if (UInventorySubsystem* Inventory = GameInstance ? GameInstance->GetSubsystem<UInventorySubsystem>() : nullptr)
	{
		Acquired = Inventory->AddItems(this, Contents, false);
	}

	UE_LOG(LogTemp, Log, TEXT("[GimmickChest] %s opened by %s, acquired %d kind(s)"), *GetName(), *GetNameSafe(ChangeInstigator), Acquired.Num());
	K2_OnChestOpened(ChangeInstigator, Acquired);

	StartAcquireSequence(ChangeInstigator, Acquired);
}

void AGimmickChest::StartAcquireSequence(AActor* Opener, const TArray<FItemStack>& Acquired)
{
	PendingAcquired = Acquired;
	PendingOpener = Opener;
	bSequenceActive = true;

	// 일시정지 중에도 뚜껑은 움직이게
	if (bPauseGameDuringAcquire && !UGameplayStatics::IsGamePaused(this) && UGameplayStatics::SetGamePaused(this, true))
	{
		bPausedGame = true;
		if (LidMover)
		{
			LidMover->SetTickableWhenPaused(true);
		}
	}

	const bool bLidStillOpening = LidMover && LidMover->IsMoving();
	if (bShowAcquireUI && bWaitLidOpenForAcquireUI && bLidStillOpening)
	{
		bWaitingLidOpen = true;
		return;
	}

	RevealAcquiredItems();
}

void AGimmickChest::HandleLidMoveFinished(UGimmickMoverComponent* Mover, bool bOpen)
{
	if (!bOpen || !bWaitingLidOpen)
	{
		return;
	}
	bWaitingLidOpen = false;

	if (AcquireUIDelay <= 0.f)
	{
		RevealAcquiredItems();
		return;
	}

	// 일시정지 중엔 월드 타이머가 멈추므로 실제 시간 기준 티커 사용
	RevealTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		RevealTickerHandle.Reset();
		RevealAcquiredItems();
		return false;
	}), AcquireUIDelay);
}

void AGimmickChest::RevealAcquiredItems()
{
	if (!bSequenceActive)
	{
		return;
	}

	UItemAcquireWidget* Widget = nullptr;
	if (bShowAcquireUI && PendingAcquired.Num() > 0)
	{
		UGameInstance* GameInstance = GetGameInstance();
		if (UInventorySubsystem* Inventory = GameInstance ? GameInstance->GetSubsystem<UInventorySubsystem>() : nullptr)
		{
			Widget = Inventory->ShowAcquireUI(this, PendingAcquired, bCloseAcquireUIOnAnyKey);
		}
	}

	K2_OnAcquireRevealed(PendingOpener.Get(), PendingAcquired);

	// 키 입력으로 닫을 UI가 없으면 바로 끝
	if (!Widget || !bCloseAcquireUIOnAnyKey)
	{
		FinishAcquireSequence();
		return;
	}

	ShownWidget = Widget;
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController)
	{
		FinishAcquireSequence();
		return;
	}

	// 아무 키 (일시정지 중에도 받음, 다른 입력은 막음)
	CloseInput = NewObject<UInputComponent>(this, TEXT("ChestAcquireCloseInput"));
	CloseInput->Priority = 1000;
	CloseInput->bBlockInput = true;
	FInputKeyBinding& Binding = CloseInput->BindKey(EKeys::AnyKey, IE_Pressed, this, &AGimmickChest::HandleCloseKeyPressed);
	Binding.bExecuteWhenPaused = true;
	PlayerController->PushInputComponent(CloseInput);
	CloseInputEnableTime = FPlatformTime::Seconds() + CloseInputDelay;
}

void AGimmickChest::HandleCloseKeyPressed()
{
	if (FPlatformTime::Seconds() < CloseInputEnableTime)
	{
		return;
	}

	if (ShownWidget)
	{
		ShownWidget->HideItems();
	}
	FinishAcquireSequence();
}

void AGimmickChest::ClearCloseInput()
{
	if (!CloseInput)
	{
		return;
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		PlayerController->PopInputComponent(CloseInput);
	}
	CloseInput = nullptr;
}

void AGimmickChest::FinishAcquireSequence()
{
	ClearCloseInput();
	ShownWidget = nullptr;
	bWaitingLidOpen = false;

	if (bPausedGame)
	{
		bPausedGame = false;
		UGameplayStatics::SetGamePaused(this, false);
	}
	if (LidMover)
	{
		LidMover->SetTickableWhenPaused(false);
	}

	const bool bWasActive = bSequenceActive;
	bSequenceActive = false;
	PendingAcquired.Reset();

	if (bWasActive)
	{
		K2_OnAcquireFinished(PendingOpener.Get());
	}
	PendingOpener.Reset();
}
