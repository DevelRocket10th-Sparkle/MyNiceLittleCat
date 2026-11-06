#include "AnimNotify/AnimNotifyState_Lunge.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotifyState_Lunge::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp || TotalDuration <= KINDA_SMALL_NUMBER) return;

	SpeedByMesh.Add(MeshComp, Distance / TotalDuration);
}

void UAnimNotifyState_Lunge::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	const float* Speed = SpeedByMesh.Find(MeshComp);
	ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	if (!Speed || !Character) return;

	// Sweep so walls and enemies stop the lunge
	const FVector Delta = Character->GetActorForwardVector().GetSafeNormal2D() * (*Speed * FrameDeltaTime);
	Character->AddActorWorldOffset(Delta, true);
}

void UAnimNotifyState_Lunge::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	SpeedByMesh.Remove(MeshComp);
}