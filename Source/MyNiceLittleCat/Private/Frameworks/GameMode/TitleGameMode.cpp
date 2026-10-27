// Fill out your copyright notice in the Description page of Project Settings.


#include "Frameworks/GameMode/TitleGameMode.h"
#include "WIdgets/Title/TitleScreenWidget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

ATitleGameMode::ATitleGameMode()
{
	DefaultPawnClass = nullptr;
}

void ATitleGameMode::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !TitleWidgetClass) return;

	if (UTitleScreenWidget* Widget = CreateWidget<UTitleScreenWidget>(PC, TitleWidgetClass))
	{
		Widget->AddToViewport();
	}
}
