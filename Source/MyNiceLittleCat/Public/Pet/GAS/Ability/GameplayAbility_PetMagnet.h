// 작성자: 우진
// 작성일자: 2026-10-17
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbility_PetMagnet.generated.h"

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UGameplayAbility_PetMagnet : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
    UGameplayAbility_PetMagnet();

protected:
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;
    // ========================================================================================
protected:
    UFUNCTION()
    void HandleMagnetTick(float DeltaTime);

    void DrawMagnetDebugRange() const;
    // ========================================================================================
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Range", meta = (ClampMin = "1.0", Units = "cm"))
    float RangeLength = 500.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Range", meta = (ClampMin = "1.0", ClampMax = "89.0", Units = "deg"))
    float HalfAngleDegrees = 15.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Range", meta = (ClampMin = "0.0", Units = "cm"))
    float ForwardOffset = 40.f;
};
