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
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    // ========================================================================================
protected:
    UFUNCTION()
    void HandleMagnetTick(float DeltaTime);

    void DrawMagnetDebugRange() const;

    void CollectMagnetTargetsInRange(TArray<AActor*>& OutTargets) const;
    AActor* FindNearestMagnetTarget(const TArray<AActor*>& Candidates) const;
    void ApplyNearbyRepulsion(const TArray<AActor*>& Candidates);

    bool IsMagnetCandidate(const AActor* _actor) const; //! 자성인지 아닌지 나중에 여기서 변경해야 함요

    UFUNCTION()
    void HandleSpecialAction(FGameplayEventData Payload);

    //! 임시 물체 연결부 체크 코드
    bool TryGetTargetPolarity(const AActor* Target, EPetMagnetPolarity& OutPolarity) const;

    void BeginAttractTarget(AActor* Target);
    void UpdateHeldTarget(float DeltaTime);
    void ReleaseHeldTarget(float ReleaseSpeed, float UpwardRatio = 0.f);

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

    UPROPERTY(EditDefaultsOnly, Category = "Magnet|Test", meta = (ClampMin = "1.0", Units = "cm/s"))
    float AttractionSpeed = 1000.f;

    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> HeldTarget;

    UPROPERTY(Transient)
    TWeakObjectPtr<class UPrimitiveComponent> HeldBody;

    bool bAttachedToCarryPoint = false;
    bool bSavedSimulatePhysics = false;
    bool bSavedActorCollision = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Test", meta = (ClampMin = "0.0", Units = "cm/s"))
    float RepelSpeed = 1800.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Test", meta = (ClampMin = "0.0"))
    float RepelUpwardRatio = 0.45f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Test", meta = (ClampMin = "0.0", Units = "cm/s"))
    float NearbyRepelSpeed = 150.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Test", meta = (ClampMin = "0.0", Units = "cm/s^2"))
    float NearbyRepelMaxAcceleration = 1200.f;
};
