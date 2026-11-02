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

private:
	// 태그가 붙은 컴포넌트를 전부 찾아 보이기/숨기기
	void SetTelegraphVisible(USkeletalMeshComponent* MeshComp, bool bVisible) const;

	// =========================================================
protected:
	UPROPERTY(EditAnywhere, Category = "Telegraph")
	FName ComponentTag = TEXT("Telegraph"); // 켜고 끌 컴포넌트의 태그

};
