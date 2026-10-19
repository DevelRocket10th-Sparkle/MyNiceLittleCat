// 작성자: 우진
// 작성일자: 2026-10-19
#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "StateTreeTaskBase.h"
#include "Pet/PetTypes.h"
#include "PetSelectedModeAbilityTask.generated.h"

USTRUCT()
struct MYNICELITTLECAT_API FPetSelectedModeAbilityTaskInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<class APetCharacter> Pet;

    // 어떤 모드를 선택했을 때? 이 Task가 능력을 켜둘 것인지...
    UPROPERTY(EditAnywhere, Category = "Parameter")
    TArray<EPetMode> Modes = { EPetMode::Reflect };

    UPROPERTY()
    TWeakObjectPtr<class UAbilitySystemComponent> AbilitySystem;

    UPROPERTY()
    FGameplayAbilitySpecHandle ActivatedHandle;

    UPROPERTY()
    EPetMode ObservedMode = EPetMode::Basic; // PrevMode

    UPROPERTY()
    bool bHasObservedMode = false;
};

USTRUCT(meta = (DisplayName = "Pet Selected Mode Ability"))
struct MYNICELITTLECAT_API FPetSelectedModeAbilityTask
    : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    FPetSelectedModeAbilityTask() { bShouldCallTick = true; }

    using FInstanceDataType = FPetSelectedModeAbilityTaskInstanceData;
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
    void CancelActivatedAbility(FInstanceDataType& Data) const;
};