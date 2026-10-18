// 작성자: 우진
// 작성일자: 2026-10-17
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Pet/PetTypes.h"
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

    AActor* FindNearestMagnetTarget() const;
    bool IsMagnetCandidate(const AActor* _actor) const; //! 자성인지 아닌지 나중에 여기서 변경해야 함요

    UFUNCTION()
    void HandleSpecialAction(FGameplayEventData Payload);

    // ========================================================================================
protected:  
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Range", meta = (ClampMin = "1.0", Units = "cm"))
    float RangeLength = 500.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Range", meta = (ClampMin = "1.0", ClampMax = "89.0", Units = "deg"))
    float HalfAngleDegrees = 15.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Range", meta = (ClampMin = "0.0", Units = "cm"))
    float ForwardOffset = 40.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Range", meta = (ClampMin = "1.0", Units = "cm"))
    float TargetHeightTolerance = 150.f;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Magnet")
    EPetMagnetPolarity CurrentPolarity = EPetMagnetPolarity::N;
};
