#include "Enemy/Animations/AnimNotify_EnemyPatternHit.h"
#include "Enemy/GAS/EnemyCombatTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"

// 서버에서 애니메이션 판정 시점을 소유 액터의 GA에 전달
void UAnimNotify_EnemyPatternHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	// 에디터 미리보기 또는 제거된 액터에서는 이벤트를 보내지 않음
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(Owner) || !Owner->HasAuthority()) return;
	FGameplayEventData Payload;
	Payload.Instigator = Owner;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EnemyCombatTags::PatternHit, Payload);
}
