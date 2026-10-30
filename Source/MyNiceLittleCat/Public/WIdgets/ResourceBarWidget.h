// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ResourceBarWidget.generated.h"

class UProgressBar;
class UTextBlock;
class USizeBox;

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UResourceBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:

	virtual void NativePreConstruct() override;

#if WITH_EDITOR
	// 에디터에서 프로퍼티 변경 후 실행되는 함수
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:

	UFUNCTION(BlueprintCallable)
	void UpdateResourceBar(float InCurrent, float InMax);

	UFUNCTION(BlueprintCallable)
	void SetResourceBarColor(FLinearColor InColor);

	UFUNCTION(BlueprintCallable, Category = "Resource Bar")
	void SetShowValueText(bool bShow);

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CurrentText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MaxText;

	// CurrentText, MaxText, / 를 담는 크기 박스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> ValueTextSizeBox;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource Bar")
	bool bShowValueText = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor FillColor = FLinearColor(0.f, 1.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor BackgroundColor = FLinearColor(0.f, 1.f, 0.f, 0.2f);

private:

	float BackgroundAlphaValue = 0.5f;

};
