#include "Pet/StateTree/PetMagnetPositionTask.h"
#include "Pet/PetCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "StateTreeExecutionContext.h"
#include "AIController.h"
EStateTreeRunStatus FPetMagnetPositionTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    Data.Movement.Reset();
    Data.bHasSavedMovement = false;

    if (false == IsValid(Data.Pet.Get()) || false == IsValid(Data.Pet->GetPartnerPlayer()))
        return EStateTreeRunStatus::Failed;

    UCharacterMovementComponent* Movement = Data.Pet->GetCharacterMovement();
    if (false == IsValid(Movement)) return EStateTreeRunStatus::Failed;

    // 종료할 때 되돌릴 설정을 기억
    Data.Movement = Movement;
    Data.SavedMovementMode = Movement->MovementMode;
    Data.SavedCustomMovementMode = Movement->CustomMovementMode;
    Data.bSavedOrientRotationToMovement = Movement->bOrientRotationToMovement;
    Data.bHasSavedMovement = true;

    // 이전 AI 이동 요청을 종료
    if (AAIController* Controller = Cast<AAIController>(Data.Pet->GetController()))
        Controller->StopMovement();

    // 이 Task가 직접 위치와 회전을 관리
    Movement->DisableMovement();
    Movement->bOrientRotationToMovement = false;

    // 진입한 프레임부터 위치를 적용!
    return Tick(Context, 0.f);
}

EStateTreeRunStatus FPetMagnetPositionTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    if (false == IsValid(Data.Pet.Get())) return EStateTreeRunStatus::Failed;

    APawn* Player = Data.Pet->GetPartnerPlayer();
    if (false == IsValid(Player)) return EStateTreeRunStatus::Failed;

    const FVector Direction = Data.Pet->GetAimDirection(); // 펫의 컨트롤러가 구한 마우스 커서가 향하는 방향을 가져와요
    const FVector TargetLocation = Player->GetActorLocation() + Direction * Data.DistanceFromPlayer + FVector(0.f, 0.f, Data.HeightOffset);
    const FRotator TargetRotation = Direction.Rotation();

    Data.Pet->SetActorLocationAndRotation(TargetLocation, TargetRotation, false);

    return EStateTreeRunStatus::Running;

}

void FPetMagnetPositionTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    if (true == Data.bHasSavedMovement)
    {
        if (UCharacterMovementComponent* Movement = Data.Movement.Get())
        {
            Movement->bOrientRotationToMovement = Data.bSavedOrientRotationToMovement;
            Movement->SetMovementMode(Data.SavedMovementMode, Data.SavedCustomMovementMode);
        }
    }

    Data.bHasSavedMovement = false;
    Data.Movement.Reset();
}
