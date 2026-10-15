// 작성자: 강진구
// 작성일자: 2026-10-15
// 
// 애니메이션 Notify에서 게임플레이 이벤트를 전송하는 클래스입니다.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_SendGamePlayEvent.generated.h"

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UAnimNotify_SendGamePlayEvent : public UAnimNotify
{
	GENERATED_BODY()
	

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	FGameplayTag EventTag;

};
