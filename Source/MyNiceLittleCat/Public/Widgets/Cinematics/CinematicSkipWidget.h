#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CinematicSkipWidget.generated.h"

// Native paint avoids material/texture asset dependencies in cooked builds.
UCLASS()
class MYNICELITTLECAT_API UCinematicSkipWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
		const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId,
		const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	float Fade = 0.f;
	float Progress = 0.f;
	float CompletionGlow = 0.f;
};
