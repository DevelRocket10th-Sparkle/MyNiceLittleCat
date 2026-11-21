// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.29.
* 체력 등 자원 값을 보여 주는 바 위젯
* - 값이 줄면 잔상(트레일)이 천천히 따라옴
*/

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
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

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

	// 데미지 잔상을 현재 값으로 즉시 맞춤 (보스 교체, 리스폰 등)
	UFUNCTION(BlueprintCallable, Category = "Resource Bar")
	void ResetTrail();

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> Bar;

	// 데미지 잔상 바: Overlay 계층에서 Bar보다 위(=뒤에 그려짐)에 배치. 없으면 잔상 기능 비활성
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> TrailBar;

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

	// 잔상 색
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Bar|Trail")
	FLinearColor TrailColor = FLinearColor(1.f, 0.768f, 0.922f);

	// 피격 후 잔상이 줄어들기 시작할 때까지 대기 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Bar|Trail", meta = (ClampMin = "0.0", Units = "s"))
	float TrailDelay = 0.35f;

	// 잔상이 따라가는 속도 (FInterpTo 속도)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Bar|Trail", meta = (ClampMin = "0.1"))
	float TrailInterpSpeed = 4.f;

private:

	void ApplyTrailPercent(float InPercent);

	float BackgroundAlphaValue = 0.5f;

	// 현재 목표 비율 및 잔상 표시 비율
	float TargetPercent = 1.f;
	float TrailPercent = 1.f;
	float TrailHoldTime = 0.f;
	bool bHasValue = false;

};
