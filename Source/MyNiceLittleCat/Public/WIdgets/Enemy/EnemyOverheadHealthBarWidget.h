// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyOverheadHealthBarWidget.generated.h"

class UResourceBarWidget;

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UEnemyOverheadHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativePreConstruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Health Bar", meta = (DisplayName = "Show Health Text"))
	bool bShowHealthText = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UResourceBarWidget> HealthBar = nullptr;

public:
	UFUNCTION(BlueprintCallable, Category = "Enemy|Health Bar")
	void SetShowHealthText(bool bShow);

	virtual void InitializeOverheadHealthBar(AActor* InActor);
	
};
