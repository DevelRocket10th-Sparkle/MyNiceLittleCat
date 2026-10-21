#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_EnemyMeleeTrace.generated.h"

// 에셋에 실행 상태를 저장하지 않고 소유 Enemy의 GA에 구간만 전달
UCLASS(meta = (DisplayName = "Enemy Melee Trace"))
class MYNICELITTLECAT_API UAnimNotifyState_EnemyMeleeTrace : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	UAnimNotifyState_EnemyMeleeTrace();
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Enemy Melee Trace"); }
};
