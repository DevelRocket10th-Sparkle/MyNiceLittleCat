// 작성자: 우진
// 작성일자: 2026-10-13
#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "PetFollowTask.generated.h"

// FPetFollowTask에 필요한 데이터 (각 펫의 Task가 사용할 데이터)
USTRUCT()
struct MYNICELITTLECAT_API FPetFollowTaskInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<class APetCharacter> Pet; // 어떤 펫을 따라갈건지

    UPROPERTY()
    TWeakObjectPtr<class AAIController> Controller;  // 작업 종료 시 이동을 취소할 컨트롤러

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "cm"))
    float FollowDistance = 200.f; // 얼마나 떨어져 따라갈지

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "cm"))
    float ResumeDistance = 300.f; // 도착 거리보다 조금 더 멀어졌을 때 재출발 기준 거리

    UPROPERTY()
    TWeakObjectPtr<AActor> MoveTarget;

    UPROPERTY()
    float TimeUntilNextCheck = 0.f; // 다음 추종 판단까지 남은 시간 (추종을 매 틱마다 진행하지 않음)

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
    float PlayerSpeedMultiplier = 1.f;  // 플레이어 속도를 얼마나 반영할지

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
    float CatchUpSpeedPerCm = 1.f;  // 추종 거리보다 1cm 뒤처질 때 추가할 속도

   
    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "cm/s"))
    float MaxFollowSpeed = 1400.f;  // 추종 중 허용할 최대 속도

    UPROPERTY()
    float OriginalMaxWalkSpeed = 0.f;  // 추종 종료 시 복원할 기존 속도

    UPROPERTY()
    bool bHasSavedMoveSpeed = false;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.1", Units = "s"))
    float MoveRetryInterval = 1.f;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0"))
    int32 MaxMoveRetries = 3;

    int32 MoveRetryCount = 0;
    float RetryTimeRemaining = 0.f;

    bool bWaitingToRetry = false;
    bool bMoveRequestFailed = false;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "1.0", Units = "s"))
    float StuckDetectionTime = 4.f;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "1.0", Units = "cm"))
    float ProgressDistance = 30.f;

    FVector ProgressAnchor = FVector::ZeroVector;
    float NoProgressTime = 0.f;
};

// 추종 Task (StateTree에서 실행하는 Task)
// 상태가 아니라.. 상태에 넣는 작업이에요
USTRUCT(meta = (DisplayName = "Pet Follow"))
struct MYNICELITTLECAT_API FPetFollowTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FPetFollowTaskInstanceData;
    
    // 데이터의 구조를 StateTree에게 알려주는 함수 (윗줄에 using과 같이 쓰여요)
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

    // Task가 속한 State가 활성화될 때 State에 들은 Task 각각에게 호출되는 함수
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

    // Task가 속한 State가 비활성화 될 때 State에 들어있고 활성화 되어 있던 Task들에게 호출되는 함수
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

    // 틱이요   
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};