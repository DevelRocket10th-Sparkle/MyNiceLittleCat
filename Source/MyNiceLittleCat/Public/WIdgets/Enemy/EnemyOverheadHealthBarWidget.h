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

public:

	virtual void InitializeOverheadHealthBar(AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "Health Bar")
	void SetShowHealthText(bool bShow);

protected:

	virtual void NativePreConstruct() override;
	
protected :

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar", meta = (DisplayName = "Show Health Text"))
	bool bShowHealthText = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UResourceBarWidget> HealthBar = nullptr;
	
};
