/*
* 작성자: 전선현
* 작성 일자: 2026.11.19.
* 수정 일자: 2026.11.20. (입력 컴포넌트에서 상태를 읽음, 문구·색 설정, 안 보일 때 접어서 Tick 정지)
* 컷신 스킵 안내 (원형 홀드 진행도)
* - 쿠킹된 빌드에서 머티리얼·텍스처 에셋 의존이 생기지 않도록 C++로 직접 그림
* - UCinematicInputComponent가 컷신마다 Wake로 깨우고, 다 사라지면 스스로 접힘
*/
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CinematicSkipWidget.generated.h"

class UCinematicInputComponent;

UCLASS()
class MYNICELITTLECAT_API UCinematicSkipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 컷신 시작 시 표시 대상 입력 컴포넌트를 지정하고 다시 보이게 함
	void Wake(const UCinematicInputComponent* InInput);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
		const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId,
		const FWidgetStyle& Style, bool bParentEnabled) const override;

public:
	// 안내 문구: {Key}에 스킵 키 이름이 들어감
	UPROPERTY(EditDefaultsOnly, Category = "Skip Prompt")
	FText HintFormat = NSLOCTEXT("Cinematic", "HoldKeyToSkip", "{Key}를 길게 눌러 넘어가기");

	UPROPERTY(EditDefaultsOnly, Category = "Skip Prompt", meta = (ClampMin = "8"))
	int32 FontSize = 17;

	UPROPERTY(EditDefaultsOnly, Category = "Skip Prompt")
	FLinearColor PanelColor = FLinearColor(0.025f, 0.03f, 0.045f, 0.78f);

	UPROPERTY(EditDefaultsOnly, Category = "Skip Prompt")
	FLinearColor TextColor = FLinearColor(0.94f, 0.93f, 0.89f);

	UPROPERTY(EditDefaultsOnly, Category = "Skip Prompt")
	FLinearColor ProgressColor = FLinearColor(1.f, 0.83f, 0.48f);

private:
	TWeakObjectPtr<const UCinematicInputComponent> Input;
	float Fade = 0.f;
	float Progress = 0.f;
	float CompletionGlow = 0.f;
};
