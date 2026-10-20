#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_EnemyPatternHit.generated.h"

// 몽타주의 공격 판정 시점을 실행 중인 GA에 전달
UCLASS()
class MYNICELITTLECAT_API UAnimNotify_EnemyPatternHit : public UAnimNotify
{
	GENERATED_BODY()
public :
	// 서버에서 메시 소유자에게 패턴 판정 이벤트 전송
	// 서버에서 애니메이션 판정 시점을 소유 액터의 GA에 전달
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
