// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/Title/TitleScreenWidget.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Widgets/Title/DifficultySelectWidget.h"

void UTitleScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	NewGameButton->OnClicked.AddDynamic(this, &UTitleScreenWidget::HandleNewGame);
	ContinueButton->OnClicked.AddDynamic(this, &UTitleScreenWidget::HandleContinue);
	ExitButton->OnClicked.AddDynamic(this, &UTitleScreenWidget::HandleExit);
	ConfirmYesButton->OnClicked.AddDynamic(this, &UTitleScreenWidget::HandleConfirmYes);
	ConfirmNoButton->OnClicked.AddDynamic(this, &UTitleScreenWidget::HandleConfirmNo);
}

void UTitleScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>();
	const bool bHasSave = Progress && Progress->HasSaveSlot();

	ContinueButton->SetIsEnabled(bHasSave);
	ConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);

	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
	}

	(bHasSave ? ContinueButton : NewGameButton)->SetKeyboardFocus();
}

void UTitleScreenWidget::StartNewGame(bool bEasyMode)
{
	if (UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>())
	{
		Progress->ResetProgress();
		// 쉬움 모드는 진행 플래그로 기록 (첫 체크포인트 저장 때 세이브에 같이 들어감)
		Progress->SetProgressFlag(TEXT("EasyMode"), bEasyMode);
	}
	UGameplayStatics::OpenLevel(this, NewGameLevel);
}

void UTitleScreenWidget::ShowDifficultySelect()
{
	ConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);

	if (!DifficultySelect)
	{
		// WBP_DifficultySelect가 지정 안 돼 있으면 보통 모드로 바로 시작
		DifficultySelect = DifficultySelectClass ? CreateWidget<UDifficultySelectWidget>(GetOwningPlayer(), DifficultySelectClass) : nullptr;
		if (!DifficultySelect)
		{
			StartNewGame(false);
			return;
		}
		DifficultySelect->OnChosen.BindUObject(this, &UTitleScreenWidget::StartNewGame);
		DifficultySelect->OnCancelled.BindUObject(this, &UTitleScreenWidget::CloseDifficultySelect);
	}

	if (!DifficultySelect->IsInViewport())
	{
		DifficultySelect->AddToViewport(10);
	}
	DifficultySelect->FocusDefault();
}

void UTitleScreenWidget::CloseDifficultySelect()
{
	if (DifficultySelect)
	{
		DifficultySelect->RemoveFromParent();
	}
	NewGameButton->SetKeyboardFocus();
}

void UTitleScreenWidget::HandleNewGame()
{
	const UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>();
	if (Progress && Progress->HasSaveSlot())
	{
		ConfirmPanel->SetVisibility(ESlateVisibility::Visible);
		ConfirmNoButton->SetKeyboardFocus();
		return;
	}

	ShowDifficultySelect();
}

void UTitleScreenWidget::HandleContinue()
{
	const UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>();
	if (!Progress) return;

	// Last checkpoint's map; MainGameMode spawns at the checkpoint (bSpawnAtCheckpointOnStart)
	const FCheckpointSaveData& Last = Progress->GetCheckpointData();
	UGameplayStatics::OpenLevel(this, Last.bValid ? FName(*Last.LevelName) : NewGameLevel);
}

void UTitleScreenWidget::HandleExit()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UTitleScreenWidget::HandleConfirmYes()
{
	ShowDifficultySelect();
}

void UTitleScreenWidget::HandleConfirmNo()
{
	ConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);
	NewGameButton->SetKeyboardFocus();
}
