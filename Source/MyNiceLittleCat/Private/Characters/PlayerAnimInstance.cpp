// Fill out your copyright notice in the Description page of Project Settings.

#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Characters/PlayerComponents/LockOnComponent.h"
#include "Characters/PlayerAnimInstance.h"

void UPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    APawn* OwnerPawn = TryGetPawnOwner();
    if (!IsValid(OwnerPawn))
    {
        Speed = 0.0f;
        Direction = 0.0f;
        bIsLockedOn = false;
        return;
    }

    const FVector Velocity = OwnerPawn->GetVelocity();
    Speed = Velocity.Size2D();

    const FVector LocalVelocity =
        OwnerPawn->GetActorTransform().InverseTransformVectorNoScale(Velocity);
    Direction = FMath::RadiansToDegrees(
        FMath::Atan2(LocalVelocity.Y, LocalVelocity.X)
    );

    bIsLockedOn = false;

    if (ULockOnComponent* LockOnComponent = OwnerPawn->FindComponentByClass<ULockOnComponent>())
    {
        bIsLockedOn = LockOnComponent->IsLockedOn();
    }
}
