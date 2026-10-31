// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/Title/TitleScreenWidget.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Frameworks/Save/GameProgressSubsystem.h"

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

void UTitleScreenWidget::StartNewGame()
{
	if (UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>())
	{
		Progress->ResetProgress();
	}
	UGameplayStatics::OpenLevel(this, NewGameLevel);
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

	StartNewGame();
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
	StartNewGame();
}

void UTitleScreenWidget::HandleConfirmNo()
{
	ConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);
	NewGameButton->SetKeyboardFocus();
}
