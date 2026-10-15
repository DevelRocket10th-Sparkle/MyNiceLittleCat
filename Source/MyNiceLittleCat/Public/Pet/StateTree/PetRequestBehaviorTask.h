// 작성자: 우진
// 작성일자: 2026-10-15
#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeExecutionContext.h"
#include "Pet/PetTypes.h"
#include "PetRequestBehaviorTask.generated.h"
/**
다음 PetBehavior(FpetBehavior)를 요청하는 Task
//x 필요한 이유는.. Task의 다음 행동을 StateTree에서 연결하려고인데 이걸 어케 제대로 적지
 */

USTRUCT()
struct MYNICELITTLECAT_API FPetRequestBehaviorTaskInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "PetBehavior")
    TObjectPtr<class APetCharacter> Pet; // 어떤 펫을 따라갈건지

    UPROPERTY(EditAnywhere, Category = "PetBehavior")
    EPetBehavior Behavior = EPetBehavior::Idle; // 다음 진행될 PetBehavior
};

USTRUCT(meta = (DisplayName = "Pet Request Behavior"))
struct MYNICELITTLECAT_API FPetRequestBehaviorTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FPetRequestBehaviorTaskInstanceData;
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};