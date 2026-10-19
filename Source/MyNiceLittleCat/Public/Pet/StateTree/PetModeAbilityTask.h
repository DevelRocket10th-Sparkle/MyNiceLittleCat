// 작성자: 우진
// 작성일자: 2026-10-18
#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "StateTreeTaskBase.h"
#include "PetModeAbilityTask.generated.h"

USTRUCT()
struct MYNICELITTLECAT_API FPetModeAbilityTaskInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<class APetCharacter> Pet;

    UPROPERTY()
    TWeakObjectPtr<class UAbilitySystemComponent> AbilitySystem;

    UPROPERTY()
    FGameplayAbilitySpecHandle ActivatedHandle;
};

USTRUCT(meta = (DisplayName = "Pet Mode Ability"))
struct MYNICELITTLECAT_API FPetModeAbilityTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    FPetModeAbilityTask()
    {
        bShouldCallTick = false; // 시작과 종료만 연결해서 틱은 읎음
    }

    using FInstanceDataType = FPetModeAbilityTaskInstanceData;
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};