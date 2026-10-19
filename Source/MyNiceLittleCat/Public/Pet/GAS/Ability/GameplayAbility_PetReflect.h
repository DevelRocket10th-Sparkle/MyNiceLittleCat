// 작성자: 우진
// 작성일자: 2026-10-19
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "pet/PetTypes.h"
#include "GameplayAbility_PetReflect.generated.h"

UCLASS()
class MYNICELITTLECAT_API UGameplayAbility_PetReflect : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGameplayAbility_PetReflect();

protected:
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    // =============================================================================

protected:
    UFUNCTION()
    void HandleReflectTick(float DeltaTime);

    bool CanPlaceReflector(const FHitResult& Hit, const FRotator& Rotation) const;

    UFUNCTION()
    void HandleModeUseInput(EPetModeUseInputPhase Phase);

    class APlayerMainController* GetPartnerPlayerController() const;

    bool UpdateDragRotation();
    // =============================================================================
protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reflect")
    TSubclassOf<class APetReflector> ReflectorClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reflect|Preview")
    TObjectPtr<class UMaterialInterface> PreviewMaterial;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reflect|Preview")
    TObjectPtr<class UStaticMesh> PreviewMeshAsset;

    UPROPERTY(Transient)
    TObjectPtr<class UStaticMeshComponent> PreviewMesh;

    UPROPERTY(Transient)
    TObjectPtr<class UMaterialInstanceDynamic> PreviewMID;

    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Placement", meta = (ClampMin = "0.0", ClampMax = "45.0", Units = "deg"))
    float MaxPlacementSlopeDegrees = 5.f;

    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Placement", meta = (ClampMin = "0.0", Units = "cm"))
    float PlacementGroundClearance = 2.f;

    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Preview")
    FLinearColor ValidPreviewColor = FLinearColor(0.05f, 0.4f, 1.f);

    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Preview")
    FLinearColor InvalidPreviewColor = FLinearColor(1.f, 0.05f, 0.05f);

    UPROPERTY(Transient, VisibleInstanceOnly, Category = "Reflect|Placement")
    bool bCanPlaceReflector = false;

    UPROPERTY(Transient)
    bool bDraggingPlacement = false;

    UPROPERTY(Transient)
    FHitResult DragStartHit;

    UPROPERTY(Transient)
    FRotator PlacementRotation = FRotator::ZeroRotator;

    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Placement", meta = (ClampMin = "1.0", Units = "cm"))
    float MinDragDistance = 15.f;

    UPROPERTY(Transient, VisibleInstanceOnly, Category = "Reflect|Placement")
    bool bHasPendingPlacement = false;

    UPROPERTY(Transient, VisibleInstanceOnly, Category = "Reflect|Placement")
    FTransform PendingPlacementTransform = FTransform::Identity;
};
