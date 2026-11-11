// 타이틀·메뉴용 버튼입니다. (동구 요청으로 추가)
// UButton을 상속하므로 기존 BindWidget(TObjectPtr<UButton>) 자리에 그대로 들어갑니다 (C++ 수정 없음).
// - 둥근 알약 모양 크림색 버튼 (텍스처 없이 RoundedBox 브러시)
// - 마우스를 올리거나 키보드/패드 포커스가 오면 살짝 커지고 흔들흔들, 누르면 쏙 눌림
// - 자식이 TextBlock이면 글자 색·크기·그림자도 같이 맞춤

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Containers/Ticker.h"
#include "CozyMenuButton.generated.h"

UCLASS(meta = (DisplayName = "Cozy Menu Button"))
class MYNICELITTLECAT_API UCozyMenuButton : public UButton
{
	GENERATED_BODY()

public:
	UCozyMenuButton(const FObjectInitializer& ObjectInitializer);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	bool TickAnim(float DeltaTime);
	void ApplyCozyStyle();
	void ApplyLabelStyle(bool bHot, bool bDisabled);

protected:
	/* 색 */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Color")
	FLinearColor FillColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Color")
	FLinearColor HoverFillColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Color")
	FLinearColor PressedFillColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Color")
	FLinearColor DisabledFillColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Color")
	FLinearColor OutlineColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Color")
	FLinearColor HoverOutlineColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Color", meta = (ClampMin = "0"))
	float OutlineWidth = 4.f;

	/* 글자 (자식이 TextBlock일 때) */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Label")
	bool bStyleLabel = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Label", meta = (EditCondition = "bStyleLabel"))
	FLinearColor TextColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Label", meta = (EditCondition = "bStyleLabel"))
	FLinearColor HoverTextColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Label", meta = (EditCondition = "bStyleLabel"))
	FLinearColor DisabledTextColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Label", meta = (EditCondition = "bStyleLabel", ClampMin = "8"))
	int32 FontSize = 30;

	/* 크기·여백 */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Layout")
	FMargin ContentPadding = FMargin(48.f, 14.f);

	// 눌렸을 때 글자가 아래로 내려가는 정도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Layout", meta = (ClampMin = "0"))
	float PressedSink = 3.f;

	/* 움직임 */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Motion", meta = (ClampMin = "0.5", ClampMax = "2"))
	float HoverScale = 1.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Motion", meta = (ClampMin = "0.5", ClampMax = "2"))
	float PressedScale = 0.94f;

	// 클수록 빠르게 따라감
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Motion", meta = (ClampMin = "1"))
	float ScaleSpeed = 16.f;

	// 올려둔 동안 좌우로 흔들리는 각도 (0 = 안 흔들림)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Motion", meta = (ClampMin = "0", Units = "deg"))
	float WobbleAngle = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy|Motion", meta = (ClampMin = "0"))
	float WobbleSpeed = 7.f;

private:
	FTSTicker::FDelegateHandle TickHandle;
	float CurrentScale = 1.f;
	float CurrentAngle = 0.f;
	float WobbleTime = 0.f;
	int8 LastLabelState = -1;
};
