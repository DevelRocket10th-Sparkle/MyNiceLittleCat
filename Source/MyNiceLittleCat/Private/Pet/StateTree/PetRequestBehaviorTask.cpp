#include "Pet/StateTree/PetRequestBehaviorTask.h"
#include "Pet/PetCharacter.h"

EStateTreeRunStatus FPetRequestBehaviorTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
       FInstanceDataType& Data = Context.GetInstanceData(*this);

       if (false == IsValid(Data.Pet.Get()))
           return EStateTreeRunStatus::Failed;

       // 펫에게 다음 행동을 요청해요
       return Data.Pet->RequestBehavior(Data.Behavior) ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}