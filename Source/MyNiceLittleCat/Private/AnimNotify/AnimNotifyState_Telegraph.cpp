#include "AnimNotify/AnimNotifyState_Telegraph.h"
#include "Components/SceneComponent.h"

void UAnimNotifyState_Telegraph::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	
	SetTelegraphVisible(MeshComp, true); // 준비 동작 시작 -> 예고 장판 표시
}

void UAnimNotifyState_Telegraph::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	
	// 돌진 직전에 숨김. 몽타주가 중간에 끊겨도(보스 사망 등) End 가 불려서 장판이 남지 않음
	SetTelegraphVisible(MeshComp, false);
}

void UAnimNotifyState_Telegraph::SetTelegraphVisible(USkeletalMeshComponent* MeshComp, bool bVisible) const
{
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (nullptr == Owner) return;

	for (UActorComponent* Comp : Owner->GetComponentsByTag(USceneComponent::StaticClass(), ComponentTag))
	{
		if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
			SceneComp->SetVisibility(bVisible);
	}
}
