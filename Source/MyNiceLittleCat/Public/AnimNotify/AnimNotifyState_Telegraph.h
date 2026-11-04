// 작성자: 우진
// 작성일자: 2026-11-02
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_Telegraph.generated.h"

/**
 * 
 */
UCLASS(meta = (DisplayName = "Tutorial Telegraph"))
class MYNICELITTLECAT_API UAnimNotifyState_Telegraph : public UAnimNotifyState
{
	GENERATED_BODY()
	
public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

private:
	// 태그가 붙은 컴포넌트를 전부 찾아 보이기/숨기기
	void SetTelegraphVisible(USkeletalMeshComponent* MeshComp, bool bVisible) const;

	// 지금 노티파이 구간의 몇 % 지점인지 (0~1). 몽타주 재생 위치 기준이라 노티파이에 상태를 저장할 필요 없음
	float GetNotifyProgress(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) const;

	// 장판 진행도/색/밝기 갱신
	void UpdateTelegraph(USkeletalMeshComponent* MeshComp, float Progress) const;

	// =========================================================
protected:
	UPROPERTY(EditAnywhere, Category = "Telegraph")
	FName ComponentTag = TEXT("Telegraph"); // 켜고 끌 컴포넌트의 태그

	// ---- 색 ----
	UPROPERTY(EditAnywhere, Category = "Telegraph")
	FLinearColor WarnColor = FLinearColor(1.f, 0.35f, 0.f, 1.f);   // 차오르는 동안 (주황)

	UPROPERTY(EditAnywhere, Category = "Telegraph")
	FLinearColor DangerColor = FLinearColor(1.f, 0.03f, 0.f, 1.f); // 끝나기 직전 (빨강)

	// 이 진행도부터 주황 -> 빨강으로 바뀌기 시작 ("곧 온다!")
	UPROPERTY(EditAnywhere, Category = "Telegraph", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DangerStart = 0.7f;

	// ---- 밝기 ----
	UPROPERTY(EditAnywhere, Category = "Telegraph", meta = (ClampMin = "0.0"))
	float BaseEmissive = 3.f;    // 평소 밝기

	UPROPERTY(EditAnywhere, Category = "Telegraph", meta = (ClampMin = "0.0"))
	float DangerEmissive = 8.f;  // 빨강일 때 밝기

	// 다 차기 직전 번쩍 (돌진 출발 신호)
	UPROPERTY(EditAnywhere, Category = "Telegraph", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlashStart = 0.92f;

	UPROPERTY(EditAnywhere, Category = "Telegraph", meta = (ClampMin = "0.0"))
	float FlashEmissive = 20.f;

};
