// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/EnemyAnimInstance.h"

#include "Enemy/Combat/FeedbackVisual.h"
#include "Enemy/EnemyCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

void UEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	if (!Owner)
	{
		Owner = Cast<AEnemyCharacter>(TryGetPawnOwner());
	}

	bShouldMove = IsValid(Owner) && Owner->GetWantsToMove();
}

bool UEnemyAnimInstance::HandleNotify(const FAnimNotifyEvent& AnimNotifyEvent)
{
	if (Super::HandleNotify(AnimNotifyEvent))
	{
		return true;
	}

	static const FName GroundSlamName(TEXT("NS_Enemy_GroundSlam_v1"));
	USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	AActor* Enemy = Mesh ? Mesh->GetOwner() : nullptr;
	UWorld* World = Enemy ? Enemy->GetWorld() : nullptr;
	if (!AnimNotifyEvent.Notify || !World || !World->IsGameWorld())
	{
		return false;
	}
	static const FName NiagaraNotifyClass(TEXT("AnimNotify_PlayNiagaraEffect"));
	if (AnimNotifyEvent.Notify->GetClass()->GetFName() != NiagaraNotifyClass
		|| FName(AnimNotifyEvent.Notify->GetNotifyName()) != GroundSlamName)
	{
		return false;
	}

	// Use the authored impact frame and keep the ripple on the floor under the strike.
	const FVector Impact = Mesh->GetComponentLocation() + Enemy->GetActorForwardVector() * 80.f;
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemyGroundSlam), false, Enemy);
	FHitResult Floor;
	const bool bFoundFloor = World->LineTraceSingleByObjectType(Floor, Impact + FVector(0, 0, 50),
		Impact - FVector(0, 0, 180), FCollisionObjectQueryParams(ECC_WorldStatic), Query);
	const FVector Location = bFoundFloor ? FVector(Floor.ImpactPoint) : Impact;
	return AFeedbackVisual::Play(Mesh, EFeedbackVisual::Landing,
		FTransform(FRotator(0, Enemy->GetActorRotation().Yaw, 0), Location, FVector(0.65))) != nullptr;
}
