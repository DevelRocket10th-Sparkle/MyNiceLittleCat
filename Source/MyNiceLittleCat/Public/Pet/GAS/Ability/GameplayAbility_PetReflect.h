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

    // 플레이어가 밟은 높이 기준으로 커서 아래 바닥을 찾음 (프리뷰·설치 위치용)
    bool TryGetPlacementCursorHit(FHitResult& OutHit);

    // 플레이어 바닥 높이 (점프 중에는 마지막으로 서 있던 높이)
    bool TryGetPlayerFloorZ(float& OutFloorZ);

    bool BeginPlacementMove();

    UFUNCTION()
    void HandlePlacementMoveFinished(FGuid RequestId, bool bSucceeded);

    int32 GetActiveReflectorCount() const;

    bool CanPlaceReflectorAtTransform(const FTransform& PlacementTransform) const;

    bool TryPlaceReflector();

    void CachePreviewLasers();

    void DrawLaserReflectionPreview() const;
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

    // 플레이어 발 높이와 이만큼까지 차이 나는 바닥에만 프리뷰·설치 가능
// 의도: 테이블 등 다른 높이에 프리뷰가 떠서 헷갈리는 것 방지, 작은 턱 정도는 허용
    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Placement", meta = (ClampMin = "1.0", Units = "cm"))
    float MaxPlacementHeightDifference = 20.f;

    // 점프·낙하 중에 쓸 마지막 바닥 높이
    float LastPlayerFloorZ = 0.f;
    bool bHasPlayerFloorZ = false;

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

    UPROPERTY(Transient)
    FGuid PlacementMoveRequestId;

    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Placement",
        meta = (ClampMin = "0.0", Units = "cm"))
    float PlacementStandOffDistance = 60.f; // 반사판과 펫 몸체 사이에 추가로 둘 여유

    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Placement", meta = (ClampMin = "1"))
    int32 MaxReflectorCount = 3;

    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<class APetReflector>> PlacedReflectors;

    // 이번 드래그에서 확인할 레이저 목록
    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<class ULaserBeamComponent>> CachedPreviewLasers;
};
