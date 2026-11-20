// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Animations/AnimNotify_EnemyFeedbackCue.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NiagaraSystem.h"

void UAnimNotify_EnemyFeedbackCue::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// 에디터 몽타주 미리보기에서는 재생하지 않음 (게임 월드에서만)
	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld() || !Cue.IsSet())
	{
		return;
	}

	// 메시 위치에서 캐릭터 앞쪽
	FVector Location = MeshComp->GetComponentLocation() + Owner->GetActorForwardVector() * ForwardOffset;

	// 내려찍는 지점의 바닥에 깔리도록 바닥 높이에 맞춤 (바닥이 없으면 계산한 위치 그대로)
	if (bSnapToFloor)
	{
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemyFeedbackCueFloor), false, Owner);
		FHitResult Floor;
		if (World->LineTraceSingleByObjectType(Floor, Location + FVector(0, 0, FloorTraceUp),
			Location - FVector(0, 0, FloorTraceDown), FCollisionObjectQueryParams(ECC_WorldStatic), Query))
		{
			Location = Floor.ImpactPoint;
		}
	}

	EnemyFeedback::PlayAtLocation(Owner, Cue, Location, FRotator(0, Owner->GetActorRotation().Yaw, 0));
}

#if WITH_EDITOR
FString UAnimNotify_EnemyFeedbackCue::GetNotifyName_Implementation() const
{
	if (Cue.Effect)
	{
		return FString::Printf(TEXT("Feedback: %s"), *Cue.Effect->GetName());
	}
	return TEXT("Enemy Feedback Cue");
}
#endif
