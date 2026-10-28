// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Frameworks/Save/GameProgressSaveGame.h"
#include "CheckpointMenuWidget.generated.h"

class ACheckpoint;
class UPanelWidget;
class UCheckpointMenuWidget;

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UCheckpointTravelButton : public UButton
{
	GENERATED_BODY()

public:
	int32 DestinationIndex = INDEX_NONE;
	TWeakObjectPtr<UCheckpointMenuWidget> Menu;

	UFUNCTION()
	void HandleClicked();
};

UCLASS()
class MYNICELITTLECAT_API UCheckpointMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open(ACheckpoint* InCheckpoint, AActor* InInstigator);

	UFUNCTION(BlueprintCallable, Category = "Checkpoint Menu")
	void Close();

	void TravelToIndex(int32 Index);

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION() void HandleRest();
	UFUNCTION() void HandleTravel();
	UFUNCTION() void HandleLeave();

	UFUNCTION() void HandlePlayerDied();
	void BuildTravelList();

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> RestButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> TravelButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> LeaveButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> TravelList;

	UPROPERTY(EditDefaultsOnly, Category = "Checkpoint Menu")
	TArray<FKey> CloseKeys = { EKeys::Escape, EKeys::E };


private:
	TWeakObjectPtr<ACheckpoint> Checkpoint;
	TWeakObjectPtr<AActor> InteractInstigator;
	TArray<FCheckpointSaveData> Destinations;
};
