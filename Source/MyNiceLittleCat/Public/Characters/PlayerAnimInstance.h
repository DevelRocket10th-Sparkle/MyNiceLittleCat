// 작성자: 강진구
// 작성일자: 2026-10-12
// 
// 플레이어용 애님인스턴스입니다	.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PlayerAnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	

public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    float Speed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    float Direction = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "LockOn")
    bool bIsLockedOn = false;
};
