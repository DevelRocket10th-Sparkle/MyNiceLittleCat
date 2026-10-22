#include "WIdgets/MainHUD.h"

#include "WIdgets/MainWidget.h"
#include "Characters/PlayerCharacter.h"
#include "GameFramework/PlayerController.h"

void AMainHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !MainWidgetClass) return;

	MainWidget = CreateWidget<UMainWidget>(PC, MainWidgetClass);
	if (!MainWidget) return;

	MainWidget->AddToViewport();

	PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &AMainHUD::HandlePawnChanged);
	HandlePawnChanged(nullptr, PC->GetPawn());
}

void AMainHUD::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (MainWidget)
	{
		MainWidget->InitForPlayer(Cast<APlayerCharacter>(NewPawn));
	}
}
