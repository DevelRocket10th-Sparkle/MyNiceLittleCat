#pragma once
// 작성자: 우진
// 작성일자: 2026-10-29

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "PetRecoverFollowTask.generated.h"

USTRUCT()
struct MYNICELITTLECAT_API FPetRecoverFollowTaskInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<class APetCharacter> Pet;

    // 날아서 복귀하는 속도
    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "1.0", Units = "cm/s"))
    float FlightSpeed = 900.f;

    // 비행 경로가 위로 부풀어 오르는 높이
    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "cm"))
    float FlightArcHeight = 150.f;

    FVector StartLocation = FVector::ZeroVector;
    FVector LandingLocation = FVector::ZeroVector;

    float ElapsedTime = 0.f;
    float FlightDuration = 0.f;

    // 비행이 끝나거나 중단될 때 복원할 설정
    ECollisionEnabled::Type SavedCollisionEnabled = ECollisionEnabled::QueryAndPhysics;

    EMovementMode SavedMovementMode = MOVE_Walking;
    uint8 SavedCustomMovementMode = 0;

    bool bSavedOrientRotationToMovement = false;
    bool bHasSavedSettings = false;

    // 플레이어가 계속 이동해 복귀가 무한히 이어지는 것 방지
    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "1.0", Units = "s"))
    float MaxRecoveryTime = 8.f;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "1.0", Units = "cm"))
    float MaxLandingDistanceFromPlayer = 500.f;

    float TotalElapsedTime = 0.f;

    // 실제 설정을 변경한 펫을 기억
    UPROPERTY()
    TWeakObjectPtr<class APetCharacter> RecoveryPet;
};

USTRUCT(meta = (DisplayName = "Pet Recover Follow"))
struct MYNICELITTLECAT_API FPetRecoverFollowTask
    : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FPetRecoverFollowTaskInstanceData;
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
    static EStateTreeRunStatus FailRecovery(APetCharacter* Pet);
};