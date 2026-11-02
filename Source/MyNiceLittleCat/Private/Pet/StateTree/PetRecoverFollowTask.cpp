#include "Pet/StateTree/PetRecoverFollowTask.h"
#include "Pet/PetCharacter.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FPetRecoverFollowTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    Data.RecoveryPet.Reset();
    Data.bHasSavedSettings = false;
    Data.ElapsedTime = 0.f;
    Data.TotalElapsedTime = 0.f;

    APetCharacter* Pet = Data.Pet.Get();
    if (false == IsValid(Pet) || false == IsValid(Pet->GetPartnerPlayer()))
        return FailRecovery(Data.Pet.Get());

    UCharacterMovementComponent* Movement = Pet->GetCharacterMovement();
    UCapsuleComponent* Capsule = Pet->GetCapsuleComponent();

    if (false == IsValid(Movement) || false == IsValid(Capsule))
        return FailRecovery(Data.Pet.Get());

    // 착지할 곳이 없으면 비행을 시작하지 않음
    if (false == Pet->TryFindSafeRecoveryLocation(Pet->GetActorLocation(), Data.LandingLocation))
        return FailRecovery(Data.Pet.Get());

    // 기존 위치 복구 타이머와 비행이 동시에 움직이지 않게 함
    Pet->CancelNavigationRecovery();

    Data.RecoveryPet = Pet;
    Data.SavedCollisionEnabled = Capsule->GetCollisionEnabled();
    Data.SavedMovementMode = Movement->MovementMode;
    Data.SavedCustomMovementMode = Movement->CustomMovementMode;
    Data.bSavedOrientRotationToMovement =
        Movement->bOrientRotationToMovement;
    Data.bHasSavedSettings = true;

    if (AAIController* Controller = Cast<AAIController>(Pet->GetController()))
        Controller->StopMovement();

    Movement->StopMovementImmediately();
    Movement->DisableMovement();
    Movement->bOrientRotationToMovement = false;

    // 장애물에 막혀 복귀하는 것이므로 비행 중 충돌은 끔
    Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Pet->SetAnimationMovementStyle(EPetMovementStyle::FollowRecovery);

    Data.StartLocation = Pet->GetActorLocation();
    const float Distance = FVector::Distance(Data.StartLocation, Data.LandingLocation);
    Data.FlightDuration = FMath::Max(Distance / FMath::Max(Data.FlightSpeed, 1.f), 0.2f);
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FPetRecoverFollowTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    APetCharacter* Pet = Data.RecoveryPet.Get();
    if (false == IsValid(Pet)) return FailRecovery(Data.Pet.Get());

    APawn* Player = Pet->GetPartnerPlayer();
    if (false == IsValid(Player))return FailRecovery(Data.Pet.Get());

    Data.ElapsedTime += DeltaTime;
    Data.TotalElapsedTime += DeltaTime;

    if (Data.TotalElapsedTime >= Data.MaxRecoveryTime) return FailRecovery(Data.Pet.Get());

    const float Alpha = FMath::Clamp(Data.ElapsedTime / Data.FlightDuration, 0.f, 1.f);

    // 출발점 → 착지점으로 이동하면서 중간에 위로 솟는 곡선
    FVector FlightLocation = FMath::Lerp(Data.StartLocation, Data.LandingLocation, Alpha);

    FlightLocation.Z += 4.f * Alpha * (1.f - Alpha) * Data.FlightArcHeight;

    const FVector Direction = (Data.LandingLocation - Pet->GetActorLocation()).GetSafeNormal2D();

    if (false == Direction.IsNearlyZero()) Pet->SetActorRotation(Direction.Rotation());

    if (Alpha < 1.f)
    {
        if (false == Pet->SetActorLocation(
            FlightLocation, false, nullptr,
            ETeleportType::TeleportPhysics))
            return FailRecovery(Data.Pet.Get());

        return EStateTreeRunStatus::Running;
    }

    // 비행 중 플레이어가 멀어졌거나 착지점이 막혔는지 검사
    const bool bPlayerMovedAway = FVector::DistSquared2D(Data.LandingLocation, Player->GetActorLocation()) > FMath::Square(Data.MaxLandingDistanceFromPlayer);

    const bool bCanLand = false == bPlayerMovedAway && Pet->CanResumeFollowingAt(Data.LandingLocation);

    if (false == bCanLand)
    {
        FVector NewLandingLocation;

        if (false == Pet->TryFindSafeRecoveryLocation(Pet->GetActorLocation(), NewLandingLocation))
            return FailRecovery(Data.Pet.Get());

        // 새 착지점까지 현재 위치에서 다시 비행
        Data.StartLocation = Pet->GetActorLocation();
        Data.LandingLocation = NewLandingLocation;
        Data.ElapsedTime = 0.f;

        Data.FlightDuration = FMath::Max(FVector::Distance(Data.StartLocation, Data.LandingLocation) / FMath::Max(Data.FlightSpeed, 1.f), 0.2f);
        return EStateTreeRunStatus::Running;
    }

    if (false == Pet->SetActorLocation(Data.LandingLocation, false, nullptr, ETeleportType::TeleportPhysics))
        return FailRecovery(Data.Pet.Get());

    Pet->RequestBehavior(EPetBehavior::Follow);
    return EStateTreeRunStatus::Succeeded;
}

void FPetRecoverFollowTask::ExitState(FStateTreeExecutionContext& Context,const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);
    
    APetCharacter* Pet = Data.RecoveryPet.Get();

    if (Data.bHasSavedSettings && IsValid(Pet) && false == Pet->IsActorBeingDestroyed())
    {
        if (UCapsuleComponent* Capsule = Pet->GetCapsuleComponent())
        {
            Capsule->SetCollisionEnabled(Data.SavedCollisionEnabled);
        }

        if (UCharacterMovementComponent* Movement = Pet->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
            Movement->bOrientRotationToMovement = Data.bSavedOrientRotationToMovement;

            // 중간에 취소되어도 허공이나 장애물 안에서
            // 그대로 걷기 이동을 재개하지 않도록 검사
            Pet->ResumeNavigationSafely(Data.SavedMovementMode, Data.SavedCustomMovementMode);
        }

        if (Pet->GetAnmationMovementStyle() == EPetMovementStyle::FollowRecovery)
            Pet->SetAnimationMovementStyle(EPetMovementStyle::Normal);
    }

    Data.bHasSavedSettings = false;
    Data.RecoveryPet.Reset();
}

EStateTreeRunStatus FPetRecoverFollowTask::FailRecovery(APetCharacter* Pet)
{
    if (true == IsValid(Pet) && Pet->GetRequestedBehavior() == EPetBehavior::RecoverFollow)
    {
        const EPetBehavior NextBehavior = IsValid(Pet->GetPartnerPlayer()) ? EPetBehavior::Follow : EPetBehavior::Idle;
        Pet->RequestBehavior(NextBehavior);
    }

    return EStateTreeRunStatus::Failed;
}
