// 작성자: 우진
// 작성일자: 2026-11-13
#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "PetMournTask.generated.h"

USTRUCT()
struct MYNICELITTLECAT_API FPetMournTaskInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<class APetCharacter> Pet;

    // 사망 반응 몽타주 (루프 섹션 권장: 사망~화면 전환이 2초 넘게 걸려요)
    UPROPERTY(EditAnywhere, Category = "Parameter")
    TObjectPtr<class UAnimMontage> MournMontage;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.1"))
    float PlayRate = 1.f;

    // 리스폰으로 상태를 나갈 때 몽타주를 부드럽게 끊는 시간
    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "s"))
    float BlendOutTime = 0.2f;

    // 반응 시작할 때 머리 위에 띄울 말 (비워두면 말풍선 없음)
    UPROPERTY(EditAnywhere, Category = "Parameter")
    FText MournMessage;

    // 말풍선이 떠 있는 시간 (0 이하면 말풍선 컴포넌트의 기본값)
    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Units = "s"))
    float MessageDuration = 2.5f;
};

// 플레이어 옆에서 사망 반응 몽타주를 재생하는 Task
// 몽타주가 끝나도 Running 유지 → 리스폰으로 Behavior가 바뀔 때까지 그 자리에 머물러요
USTRUCT(meta = (DisplayName = "Pet Mourn"))
struct MYNICELITTLECAT_API FPetMournTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FPetMournTaskInstanceData;
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};