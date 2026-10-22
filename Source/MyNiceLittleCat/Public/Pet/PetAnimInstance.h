// 작성자: 우진
// 작성일자: 2026-10-21
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Pet/PetTypes.h"
#include "PetAnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UPetAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
    // 실제 위치 변화로 계산한 수평 이동 속도
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Pet|Animation")
    float Speed = 0.f;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Pet|Animation")
    bool bIsMoving = false;

    // 작은 위치 흔들림을 이동으로 판단하지 않도록 설정
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pet|Animation",
        meta = (ClampMin = "0.0", Units = "cm/s"))
    float MovingSpeedThreshold = 2.f;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Pet|Animation")
    EPetMovementStyle MovementStyle = EPetMovementStyle::Normal;

    // 실제 자성 능력 사용 중인지
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Pet|Animation")
    bool bIsUsingMagnet = false;

private:
    TWeakObjectPtr<class APetCharacter> Pet;

    FVector PrevLocation = FVector::ZeroVector;
    bool bHasPrevLocation = false;
};
