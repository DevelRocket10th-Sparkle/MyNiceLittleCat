#include "Enemy/Animations/AnimNotifyState_EnemyMeleeTrace.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/GAS/EnemyCombatTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	void SendWindowEvent(USkeletalMeshComponent* Mesh, const UObject* Notify, FGameplayTag Tag)
	{
		AEnemyCharacter* Enemy = Mesh ? Cast<AEnemyCharacter>(Mesh->GetOwner()) : nullptr;
		if (!IsValid(Enemy) || !Enemy->HasAuthority() || Mesh != Enemy->GetMesh()) return;
		FGameplayEventData Payload;
		Payload.Instigator = Enemy;
		Payload.OptionalObject = Notify;
		Payload.OptionalObject2 = Mesh;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Enemy, Tag, Payload);
	}
}

UAnimNotifyState_EnemyMeleeTrace::UAnimNotifyState_EnemyMeleeTrace()
{
#if WITH_EDITORONLY_DATA
	bShouldFireInEditor = false;
#endif
}

void UAnimNotifyState_EnemyMeleeTrace::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	SendWindowEvent(MeshComp, this, EnemyCombatTags::MeleeWindowBegin);
}

void UAnimNotifyState_EnemyMeleeTrace::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	SendWindowEvent(MeshComp, this, EnemyCombatTags::MeleeWindowEnd);
}
