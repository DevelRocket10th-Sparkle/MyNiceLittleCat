// 작성자: 우진
// 작성일자: 2026-10-17

#pragma once
#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "PetMagnetPositionTask.generated.h"
USTRUCT()
struct MYNICELITTLECAT_API FPetMagnetPositionTaskInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<class APetCharacter> Pet;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "cm"))
    float DistanceFromPlayer = 150.f;  // 플레이어 중심에서 펫까지의 수평 거리

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Units = "cm"))
    float HeightOffset = 0.f; // 플레이어 중심 높이를 기준으로 한 추가 높이

    UPROPERTY()
    TWeakObjectPtr<class UCharacterMovementComponent> Movement;

    // 변경 전 --------------------------------------------------
    UPROPERTY()
    TEnumAsByte<EMovementMode> SavedMovementMode = MOVE_Walking; //이동 방식

    UPROPERTY()
    uint8 SavedCustomMovementMode = 0;

    UPROPERTY()
    bool bSavedOrientRotationToMovement = false; // 이동 방향으로 몸을 회전시키는 세팅값 (임시)

    UPROPERTY()
    bool bHasSavedMovement = false;
};

USTRUCT(meta = (DisplayName = "Pet Magnet Position"))
struct MYNICELITTLECAT_API FPetMagnetPositionTask
    : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FPetMagnetPositionTaskInstanceData;
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};