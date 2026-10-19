#include "Pet/StateTree/PetModeAbilityTask.h"
#include "Pet/PetCharacter.h"
#include "AbilitySystemComponent.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FPetModeAbilityTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    Data.AbilitySystem.Reset();
    Data.ActivatedHandle = FGameplayAbilitySpecHandle();

    if (false == IsValid(Data.Pet.Get()))
        return EStateTreeRunStatus::Failed;

    UAbilitySystemComponent* ASC = Data.Pet->GetAbilitySystemComponent();
    if (false == IsValid(ASC))
        return EStateTreeRunStatus::Failed;

    FGameplayAbilitySpecHandle Handle;
    if (false == Data.Pet->TryGetModeAbilityHandle(Data.Pet->GetCurrentMode(), Handle))
        return EStateTreeRunStatus::Failed;

    if (false == ASC->TryActivateAbility(Handle))
        return EStateTreeRunStatus::Failed;

    Data.AbilitySystem = ASC;
    Data.ActivatedHandle = Handle;

    return EStateTreeRunStatus::Running;
}

void FPetModeAbilityTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    if (UAbilitySystemComponent* ASC = Data.AbilitySystem.Get())
    {
        if (true == Data.ActivatedHandle.IsValid())
            ASC->CancelAbilityHandle(Data.ActivatedHandle);
    }

    Data.ActivatedHandle = FGameplayAbilitySpecHandle();
    Data.AbilitySystem.Reset();
}
