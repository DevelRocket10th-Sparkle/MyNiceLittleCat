// 작성자: 우진
// 작성일자: 2026-10-14
#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "PetIdleTask.generated.h"

USTRUCT()
struct MYNICELITTLECAT_API FPetIdleTaskInstanceData
{
    GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "Pet Idle"))
struct MYNICELITTLECAT_API FPetIdleTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FPetIdleTaskInstanceData;
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override
    { return EStateTreeRunStatus::Running; }

    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override
    { return EStateTreeRunStatus::Running; }
};