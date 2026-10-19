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
    Data.AbilityMoveRequestId.Invalidate();

    if (false == IsValid(Data.Pet.Get()))
        return EStateTreeRunStatus::Failed;

    // 이 이동을 시작하게 한 능력 요청 번호 저장
    if (Data.Pet->GetRequestedBehavior() == EPetBehavior::AbilityMoveTo)
    {
        Data.AbilityMoveRequestId = Data.Pet->GetActiveAbilityMoveRequestId();
        if (false == Data.AbilityMoveRequestId.IsValid())
            return EStateTreeRunStatus::Failed;
    }

    AAIController* Controller = Cast<AAIController>(Data.Pet->GetController());
    if (false == IsValid(Controller))
    {
        ReportAbilityMoveResult(Data.Pet.Get(), Data.AbilityMoveRequestId, false);
        return EStateTreeRunStatus::Failed;
    }

    UPathFollowingComponent* PathFollowing = Controller->GetPathFollowingComponent();

    if (false == IsValid(PathFollowing))
    {
        ReportAbilityMoveResult(Data.Pet.Get(), Data.AbilityMoveRequestId, false);
        return EStateTreeRunStatus::Failed;
    }

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
    {
        ReportAbilityMoveResult(Data.Pet.Get(), Data.AbilityMoveRequestId, false);
        return EStateTreeRunStatus::Failed;
    }

    if (RequestResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
    {
        ReportAbilityMoveResult(Data.Pet.Get(), Data.AbilityMoveRequestId, true);
        return EStateTreeRunStatus::Succeeded;
    }

    Data.MoveId = RequestResult.MoveId;

    const FStateTreeWeakExecutionContext WeakContext = Context.MakeWeakExecutionContext();
    const FAIRequestID ExpectedMoveId = Data.MoveId;
    const FGuid AbilityRequestId = Data.AbilityMoveRequestId;
    const TWeakObjectPtr<APetCharacter> WeakPet = Data.Pet.Get();

    Data.FinishedHandle = PathFollowing->OnRequestFinished.AddLambda(
        [WeakContext, ExpectedMoveId, AbilityRequestId, WeakPet]
        (FAIRequestID RequestId, const FPathFollowingResult& Result)
        {
            if (RequestId != ExpectedMoveId) return;

            ReportAbilityMoveResult(WeakPet.Get(), AbilityRequestId, Result.IsSuccess());
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
            // 이동 취소 전에 완료 알림 연결부터 해제
            PathFollowing->OnRequestFinished.Remove(Data.FinishedHandle);

            // 아직 이 Task의 이동이 진행 중일 때만 취소
            if (Data.MoveId.IsValid() && PathFollowing->GetCurrentRequestId() == Data.MoveId)
                Controller->StopMovement();
        }
    }

    const FGuid AbilityRequestId = Data.AbilityMoveRequestId;
    Data.FinishedHandle.Reset();
    Data.MoveId = FAIRequestID::InvalidRequest;
    Data.Controller.Reset();
    Data.AbilityMoveRequestId.Invalidate();

    // 완료 전에 상태를 떠났다면 취소 결과 전달
    ReportAbilityMoveResult(Data.Pet.Get(), AbilityRequestId, false);
}

void FPetMoveToTask::ReportAbilityMoveResult(APetCharacter* Pet, const FGuid& RequestId, bool bSucceeded)
{
    if (true == IsValid(Pet) && true == RequestId.IsValid())
        Pet->CompleteAbilityMove(RequestId, bSucceeded);
}
