#include "Pet/StateTree/PetSelectedModeAbilityTask.h"
#include "StateTreeExecutionContext.h"
#include "Pet/PetCharacter.h"
#include "AbilitySystemComponent.h"

EStateTreeRunStatus FPetSelectedModeAbilityTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    Data.AbilitySystem.Reset();
    Data.ActivatedHandle = FGameplayAbilitySpecHandle();
    Data.bHasObservedMode = false;

    return Tick(Context, 0.f);
}

EStateTreeRunStatus FPetSelectedModeAbilityTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    if (false == IsValid(Data.Pet.Get()))
    {
        CancelActivatedAbility(Data);
        return EStateTreeRunStatus::Failed;
    }

    if (false == Data.Pet->IsPetInitialized())
        return EStateTreeRunStatus::Running;

    const EPetMode CurrentMode = Data.Pet->GetCurrentMode();

    // 같은 모드라면 유지
    if (true == Data.bHasObservedMode && CurrentMode == Data.ObservedMode)
        return EStateTreeRunStatus::Running;

    // 모드가 바뀌면 이전에 실행했던 능력 취소
    CancelActivatedAbility(Data);

    Data.ObservedMode = CurrentMode;
    Data.bHasObservedMode = true;

    // 이 Task에서 관리하는 모드인지 확인
    if (false == Data.Modes.Contains(CurrentMode))
        return EStateTreeRunStatus::Running;

    UAbilitySystemComponent* ASC = Data.Pet->GetAbilitySystemComponent();
    if (false == IsValid(ASC)) return EStateTreeRunStatus::Running;

    FGameplayAbilitySpecHandle Handle;

    if (false == Data.Pet->TryGetModeAbilityHandle(CurrentMode, Handle))
        return EStateTreeRunStatus::Running;

    if (true == ASC->TryActivateAbility(Handle))
    {
        Data.AbilitySystem = ASC;
        Data.ActivatedHandle = Handle;
    }

    return EStateTreeRunStatus::Running;
}

void FPetSelectedModeAbilityTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    CancelActivatedAbility(Data);
    Data.bHasObservedMode = false;
}

void FPetSelectedModeAbilityTask::CancelActivatedAbility(FInstanceDataType& Data) const
{
    UAbilitySystemComponent* ASC = Data.AbilitySystem.Get();
    const FGameplayAbilitySpecHandle Handle = Data.ActivatedHandle;

    Data.AbilitySystem.Reset();
    Data.ActivatedHandle = FGameplayAbilitySpecHandle();

    if (true == IsValid(ASC) && true == Handle.IsValid())
        ASC->CancelAbilityHandle(Handle);
}
