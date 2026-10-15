#include "Pet/StateTree/PetFollowTask.h"
#include "Pet/PetCharacter.h"
#include "AIController.h"
#include "StateTreeExecutionContext.h"
#include "Navigation/PathFollowingComponent.h"

EStateTreeRunStatus FPetFollowTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    // 현재 실행중인 Task의 데이터를 꺼냄 (InstanceData)
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    // 데이터 초기화
    Data.Controller.Reset();
    Data.MoveTarget.Reset();
    Data.TimeUntilNextCheck = 0.f;

    // 펫이 없다 : Task 실패
    if (false == IsValid(Data.Pet.Get()))
        return EStateTreeRunStatus::Failed;
    
    // 펫의 AI 컨트롤러가 없다 : Task 실패
    AAIController* Controller = Cast<AAIController>(Data.Pet->GetController());
    if (false == IsValid(Controller))
        return EStateTreeRunStatus::Failed;

    // 펫의 타겟이 없다 : Task 실패
    AActor* Target = Data.Pet->GetPartnerPlayer();
    if (false == IsValid(Target))
        return EStateTreeRunStatus::Failed;

    Data.Controller = Controller;

    // Target에게로 이동 요청 실패 : Task 실패
    // MoveToActor(따라갈 대상, 도착 허용 거리, 펫 반경을 허용 거리에 추가할지)
    const EPathFollowingRequestResult::Type Result =
        Controller->MoveToActor(Target, Data.FollowDistance, false);

    if (EPathFollowingRequestResult::Failed == Result)
        return EStateTreeRunStatus::Failed;

    // Task 성공 ~
    Data.MoveTarget = Target;
    return EStateTreeRunStatus::Running;
}

void FPetFollowTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    if (AAIController* Controller = Data.Controller.Get())
        Controller->StopMovement();

    Data.Controller.Reset();
    Data.TimeUntilNextCheck = 0.f;
}

EStateTreeRunStatus FPetFollowTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    Data.TimeUntilNextCheck -= DeltaTime;

    if (0.f < Data.TimeUntilNextCheck)
        return EStateTreeRunStatus::Running;

    Data.TimeUntilNextCheck = 0.2f; //! 이동명령 주기 임시값

    AAIController* Controller = Data.Controller.Get();

    // 대상이 없으면 : Task 실패
    if (false == IsValid(Data.Pet.Get()) || false == IsValid(Controller))
        return EStateTreeRunStatus::Failed;

    AActor* Target = Data.Pet->GetPartnerPlayer();
    if (false == IsValid(Target))
    {
        Controller->StopMovement();
        return EStateTreeRunStatus::Failed;
    }

    const bool bTargetChanged = Data.MoveTarget.Get() != Target;

    if (false == bTargetChanged)
    {
        // 기존 이동 요청이 진행 중이면 다시 요청하지 않아요
        if (Controller->GetMoveStatus() != EPathFollowingStatus::Idle)
            return EStateTreeRunStatus::Running;
  
        const float RestartDistance = FMath::Max(Data.ResumeDistance, Data.FollowDistance);
        const float DistanceSquared = FVector::DistSquared2D(Data.Pet->GetActorLocation(), Target->GetActorLocation());

        if (DistanceSquared <= FMath::Square(RestartDistance))
            return EStateTreeRunStatus::Running;
    }

    // 이동 요청 실패? : Task 실패
    const EPathFollowingRequestResult::Type Result =
        Controller->MoveToActor(Target, Data.FollowDistance, false);
    
    if (Result == EPathFollowingRequestResult::Failed)
        return EStateTreeRunStatus::Failed;

    Data.MoveTarget = Target;
    return EStateTreeRunStatus::Running;
}
