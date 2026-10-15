// 작성자: 우진
// 작성일자: 2026-10-12
#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "StateTreeTaskBase.h"
#include "PetMoveToTask.generated.h"

// FPetMoveToTask에 필요한 데이터
USTRUCT()
struct MYNICELITTLECAT_API FPetMoveToTaskInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<class APetCharacter> Pet;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "cm"))
    float AcceptanceRadius = 50.f;

    UPROPERTY()
    TWeakObjectPtr<class AAIController> Controller;

    FAIRequestID MoveId = FAIRequestID::InvalidRequest;
    FDelegateHandle FinishedHandle;
};

// MoveTo Task
USTRUCT(meta = (DisplayName = "Pet Move To"))
struct MYNICELITTLECAT_API FPetMoveToTask
    : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FPetMoveToTaskInstanceData;
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct();}

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};