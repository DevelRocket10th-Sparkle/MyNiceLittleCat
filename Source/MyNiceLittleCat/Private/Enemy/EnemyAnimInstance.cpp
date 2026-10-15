// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/EnemyAnimInstance.h"

#include "GameFramework/PawnMovementComponent.h"

void UEnemyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (APawn* OwnerPawn = TryGetPawnOwner())
	{
		OwnerMovementComponent = OwnerPawn->GetMovementComponent();
	}
}

void UEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	if (OwnerMovementComponent.IsValid())
	{
		MoveSpeed = OwnerMovementComponent->Velocity.Size();
	}
	else
	{
		// NativeInitializeAnimation()에서 Owner를 못 찾았을 때를 대비한 코드
		{
			if (APawn* OwnerPawn = TryGetPawnOwner())
			{
				OwnerMovementComponent = OwnerPawn->GetMovementComponent();
			}
		}
	}
}
