// 일시정지 메뉴 구현

#include "Widgets/Pause/PauseMenuWidget.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UPauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ResumeButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleResume);
	QuitButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleQuit);
	SetIsFocusable(true);
}

void UPauseMenuWidget::Open()
{
	if (bOpen)
	{
		return;
	}
	bOpen = true;

	AddToViewport(100);
	UGameplayStatics::SetGamePaused(this, true);

	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
	}
	ResumeButton->SetKeyboardFocus();
}

void UPauseMenuWidget::Close()
{
	if (!bOpen)
	{
		return;
	}
	bOpen = false;

	UGameplayStatics::SetGamePaused(this, false);

	// APlayerMainController::BeginPlay와 같은 입력 모드로 되돌림 (마우스 커서로 펫 조작)
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
	}
	RemoveFromParent();
}

FReply UPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!InKeyEvent.IsRepeat() && CloseKeys.Contains(InKeyEvent.GetKey()))
	{
		Close();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPauseMenuWidget::HandleResume()
{
	Close();
}

void UPauseMenuWidget::HandleQuit()
{
	APlayerController* PC = GetOwningPlayer();
	UGameplayStatics::SetGamePaused(this, false);

	if (!QuitToLevel.IsNone())
	{
		bOpen = false;
		RemoveFromParent();
		UGameplayStatics::OpenLevel(this, QuitToLevel);
		return;
	}
	UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
}
