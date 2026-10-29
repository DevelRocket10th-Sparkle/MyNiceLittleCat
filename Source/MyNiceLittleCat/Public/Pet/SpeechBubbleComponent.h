// 작성자: 우진
// 작성일자: 2026-10-27
#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "SpeechBubbleComponent.generated.h"

UCLASS(ClassGroup = (Pet), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API USpeechBubbleComponent : public UWidgetComponent
{
	GENERATED_BODY()
	
public:
	USpeechBubbleComponent();
	//=====================================================================

public:
	// 말풍선 띄우기. _Duration 초 뒤 자동으로 숨김 
	// (0 이하면 DefaultDuration)
	UFUNCTION(BlueprintCallable, Category = "Pet|SpeechBubble")
	void Say(const FText& _Message, float _Duration = 0.f);

	// 말풍선 바로 숨기기
	UFUNCTION(BlueprintCallable, Category = "Pet|SpeechBubble")
	void HideBubble();
	//=====================================================================
protected:
	UPROPERTY(EditAnywhere, Category = "Pet|SpeechBubble", meta = (ClampMin = "0.1"))
	float DefaultDuration = 2.5f; // 기본적으로 말풍선이 떠있는 초

private:
	FTimerHandle HideTimerHandle; // 말풍선 숨김 타이머
	
};
