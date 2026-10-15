#include "Pet/StateTree/PetMoveToTask.h"
#include "Pet/PetCharacter.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeAsyncExecutionContext.h"

EStateTreeRunStatus FPetMoveToTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    Data.Controller.Reset();
    Data.MoveId = FAIRequestID::InvalidRequest;
    Data.FinishedHandle.Reset();

    if (false == IsValid(Data.Pet.Get()))
        return EStateTreeRunStatus::Failed;

    AAIController* Controller = Cast<AAIController>(Data.Pet->GetController());

    if (false == IsValid(Controller))
        return EStateTreeRunStatus::Failed;

    UPathFollowingComponent* PathFollowing = Controller->GetPathFollowingComponent();

    if (false == IsValid(PathFollowing))
        return EStateTreeRunStatus::Failed;

    Data.Controller = Controller;

    FAIMoveRequest MoveRequest;
    MoveRequest.SetGoalLocation(Data.Pet->GetMoveDestination());
    MoveRequest.SetAcceptanceRadius(Data.AcceptanceRadius);
    MoveRequest.SetUsePathfinding(true);
    MoveRequest.SetAllowPartialPath(false);
    MoveRequest.SetProjectGoalLocation(false);
    MoveRequest.SetReachTestIncludesAgentRadius(false);

    const FPathFollowingRequestResult RequestResult = Controller->MoveTo(MoveRequest);

    if (RequestResult.Code == EPathFollowingRequestResult::Failed)
        return EStateTreeRunStatus::Failed;

    if (RequestResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
        return EStateTreeRunStatus::Succeeded;

    Data.MoveId = RequestResult.MoveId;

    const FStateTreeWeakExecutionContext WeakContext = Context.MakeWeakExecutionContext();
    const FAIRequestID ExpectedMoveId = Data.MoveId;

    Data.FinishedHandle = PathFollowing->OnRequestFinished.AddLambda(
        [WeakContext, ExpectedMoveId](FAIRequestID RequestId, const FPathFollowingResult& Result)
        {
            // 다른 이동 요청의 완료 알림은 무시합니다.
            if (RequestId != ExpectedMoveId) return;

            WeakContext.FinishTask(Result.IsSuccess() ? EStateTreeFinishTaskType::Succeeded : EStateTreeFinishTaskType::Failed);
        });

    return EStateTreeRunStatus::Running;
}

void FPetMoveToTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    if (AAIController* Controller = Data.Controller.Get())
    {
        if (UPathFollowingComponent* PathFollowing = Controller->GetPathFollowingComponent())
        {
            // 이동 취소 전에 완료 알림 연결부터 해제합니다.
            PathFollowing->OnRequestFinished.Remove(Data.FinishedHandle);

            // 아직 이 Task의 이동이 진행 중일 때만 취소합니다.
            if (Data.MoveId.IsValid() && PathFollowing->GetCurrentRequestId() == Data.MoveId)
                Controller->StopMovement();
        }
    }

    Data.FinishedHandle.Reset();
    Data.MoveId = FAIRequestID::InvalidRequest;
    Data.Controller.Reset();
}
