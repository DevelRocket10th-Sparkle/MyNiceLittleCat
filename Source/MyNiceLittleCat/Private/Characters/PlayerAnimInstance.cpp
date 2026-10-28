// Fill out your copyright notice in the Description page of Projecttings.

#include "Characters/PlayerAnimInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Characters/PlayerCharacter.h"
#include "Characters/PlayerComponents/LockOnComponent.h"

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
    //const FVector LocalVelocity = OwnerPawn->GetActorTransform().InverseTransformVectorNoScale(Velocity);
    const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.0f);

    Speed = HorizontalVelocity.Size2D();

	if (FMath::IsNearlyZero(Speed))
	{
		Speed = 0.0f;
		Direction = 0.0f;
	}
    else
    {
        const FVector Forward = OwnerPawn->GetActorForwardVector();
        const FVector Right = OwnerPawn->GetActorRightVector();

        const float ForwardSpeed =
            FVector::DotProduct(HorizontalVelocity, Forward);

        const float RightSpeed =
            FVector::DotProduct(HorizontalVelocity, Right);

        Direction = FMath::RadiansToDegrees(FMath::Atan2(RightSpeed, ForwardSpeed));
    }

    //Direction = FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X));

    bIsLockedOn = false;
    ULockOnComponent* LockOnComponent = OwnerPawn->FindComponentByClass<ULockOnComponent>();
    const APlayerCharacter* Player = Cast<APlayerCharacter>(OwnerPawn);
    const bool bRunning = Player && Player->GetbIsRunning();

    // While running, use normal locomotion (no strafe, no spine aim)
    bIsLockedOn = LockOnComponent && LockOnComponent->IsLockedOn() && !bRunning;

    float TargetAimYaw = 0.0f;
    float TargetAimPitch = 0.0f;

    AActor* Target = bIsLockedOn ? LockOnComponent->GetLockedTarget() : nullptr;
    if (IsValid(Target))
    {
        const FVector ToTarget = Target->GetActorLocation() - OwnerPawn->GetActorLocation();
        FRotator RelativeRotation = ToTarget.Rotation() - OwnerPawn->GetActorRotation();
        RelativeRotation.Normalize();

        TargetAimYaw = FMath::Clamp(RelativeRotation.Yaw, -360.0f, 360.0f);
        TargetAimPitch = FMath::Clamp(-RelativeRotation.Pitch, -35.0f, 10.0f);
    }

    // Eases back to 0 when not aiming, so the spine doesn't snap
    AimYaw = FMath::FInterpTo(AimYaw, TargetAimYaw, DeltaSeconds, 20.0f);
    AimPitch = FMath::FInterpTo(AimPitch, TargetAimPitch, DeltaSeconds, 20.0f);
}
