// Fill out your copyright notice in the Description page of Project Settings.


#include "WIdgets/Checkpoint/CheckpointMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Gimmicks/Checkpoint/Checkpoint.h"

void UCheckpointTravelButton::HandleClicked()
{
	if (UCheckpointMenuWidget* MenuPtr = Menu.Get())
	{
		MenuPtr->TravelToIndex(DestinationIndex);
	}
}

void UCheckpointMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	RestButton->OnClicked.AddDynamic(this, &UCheckpointMenuWidget::HandleRest);
	TravelButton->OnClicked.AddDynamic(this, &UCheckpointMenuWidget::HandleTravel);
	LeaveButton->OnClicked.AddDynamic(this, &UCheckpointMenuWidget::HandleLeave);
	SetIsFocusable(true);
}

FReply UCheckpointMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		Close();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UCheckpointMenuWidget::Open(ACheckpoint* InCheckpoint, AActor* InInstigator)
{
	Checkpoint = InCheckpoint;
	InteractInstigator = InInstigator;
	TravelList->SetVisibility(ESlateVisibility::Collapsed);
	AddToViewport(10);

	// 인풋 막고 커서 보여주기
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
	}
}

void UCheckpointMenuWidget::Close()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

void UCheckpointMenuWidget::TravelToIndex(int32 Index)
{
	if (!Destinations.IsValidIndex(Index))
	{
		return;
	}

	const FCheckpointSaveData Destination = Destinations[Index];   // copy before Close() clears the widget
	ACheckpoint* CheckpointPtr = Checkpoint.Get();
	AActor* InstigatorPtr = InteractInstigator.Get();
	Close();

	if (CheckpointPtr)
	{
		CheckpointPtr->TravelTo(InstigatorPtr, Destination);
	}
}


void UCheckpointMenuWidget::HandleRest()
{
	ACheckpoint* CheckpointPtr = Checkpoint.Get();
	AActor* InstigatorPtr = InteractInstigator.Get();
	Close();

	if (CheckpointPtr)
	{
		CheckpointPtr->Rest(InstigatorPtr);
	}
}

void UCheckpointMenuWidget::HandleTravel()
{
	const bool bShow = TravelList->GetVisibility() == ESlateVisibility::Collapsed;
	if (bShow)
	{
		BuildTravelList();
	}
	TravelList->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UCheckpointMenuWidget::HandleLeave()
{
	Close();
}

void UCheckpointMenuWidget::BuildTravelList()
{
	TravelList->ClearChildren();
	Destinations.Reset();

	const UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>();
	const ACheckpoint* CheckpointPtr = Checkpoint.Get();
	if (!Progress || !CheckpointPtr)
	{
		return;
	}

	const FString CurrentLevel = UGameplayStatics::GetCurrentLevelName(this, true);
	for (const FCheckpointSaveData& Data : Progress->GetDiscoveredCheckpoints())
	{
		// Skip the checkpoint we're standing at
		if (Data.LevelName == CurrentLevel && Data.CheckpointId == CheckpointPtr->GetCheckpointId())
		{
			continue;
		}

		UCheckpointTravelButton* Button = WidgetTree->ConstructWidget<UCheckpointTravelButton>();
		Button->DestinationIndex = Destinations.Add(Data);
		Button->Menu = this;
		Button->OnClicked.AddDynamic(Button, &UCheckpointTravelButton::HandleClicked);

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(Data.DisplayName.IsEmpty() ? FText::FromName(Data.CheckpointId) : Data.DisplayName);
		Button->AddChild(Label);

		TravelList->AddChild(Button);
	}

	if (Destinations.IsEmpty())
	{
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>();
		Empty->SetText(NSLOCTEXT("Checkpoint", "NoDestinations", "No other checkpoints discovered"));
		TravelList->AddChild(Empty);
	}
}
