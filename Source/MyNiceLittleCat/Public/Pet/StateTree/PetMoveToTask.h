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

    UPROPERTY()
    FGuid AbilityMoveRequestId;

    // 0이면 기존 속도를 그대로 사용
    UPROPERTY(EditAnywhere, Category = "Parameter",
        meta = (ClampMin = "0.0", Units = "cm/s"))
    float MoveSpeedOverride = 0.f;

    // 0이면 기존 가속도를 그대로 사용
    // 의도: 가속이 낮으면 짧은 거리에서 MoveSpeedOverride까지 올라가지 못하므로 함께 조절
    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "cm/s^2"))
    float MaxAccelerationOverride = 0.f;

    // 0이면 기존 감속도를 그대로 사용
    // 의도: 경로 이동은 도착 전에 이 값 기준으로 미리 브레이크를 거는데, 낮으면 너무 일찍 느려짐
    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "cm/s^2"))
    float BrakingDecelerationOverride = 0.f;

    UPROPERTY()
    TWeakObjectPtr<class UCharacterMovementComponent> Movement;

    float OriginalMaxWalkSpeed = 0.f;
    bool bHasSavedMoveSpeed = false;
    float OriginalMaxAcceleration = 0.f;
    float OriginalBrakingDeceleration = 0.f;
    bool bHasSavedAcceleration = false;
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

private:
    static void ReportAbilityMoveResult(class APetCharacter* Pet, const FGuid& RequestId, bool bSucceeded);
};