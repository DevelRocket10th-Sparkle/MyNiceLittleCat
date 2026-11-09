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

    // 드래그 중 반사판에 맞고 반사될 레이저 경로를 흐르는 점선으로 표시 (매 틱)
    // 의도: 디버그 선은 Shipping 빌드에서 사라지므로 인게임에서도 보이는 메시로 그림
    void UpdateLaserReflectionPreview();

    // 점선 구간 메시 풀: 모자라면 만들고, 남는 건 숨겨서 재사용 (매 틱 생성/삭제 방지)
    bool ShowLaserPreviewSegment(int32 Index, const FVector& Start, const FVector& End);
    void HideLaserPreviewFrom(int32 Index);

    // 설치 프리뷰 겸 설치 대기 표시: 실제 반사판 메시를 반투명하게 보여주는 고스트
    // 의도: 마우스로 위치를 고를 때부터 펫이 설치할 때까지 같은 모양으로 이어지게
    bool CreatePlacementGhost(class APetCharacter* Pet); // 못 만들면 false (머티리얼 미설정 등)
    void ShowPlacementGhost(const FTransform& ReflectorTransform, bool bPlaceable);
    void HidePlacementGhost();

    // 드래그로 방향을 정하는 동안 반사판 앞면 쪽 바닥에 띄우는 방향 화살표
    // 의도: 고스트만으로는 앞/뒷면 구분이 어려울 수 있어서, 거울이 바라보는 쪽을 확실히 보여주기 위함
    void CreateFacingArrow(class APetCharacter* Pet);
    void ShowFacingArrow(const FVector& GroundLocation);
    void HideFacingArrow();

    // 철거: 커서가 가리키는 설치된 반사판 갱신 (매 틱)
    void UpdateRemoveHover();

    // 커서 아래에 있는, 펫이 설치한 반사판 (벽에 가려진 건 제외)
    class APetReflector* FindHoveredReflector() const;

    // 하이라이트·Q 안내 UI를 새 대상으로 교체
    void SetHoveredReflector(class APetReflector* NewReflector);

    // Q(Pet.SpecialAction) 입력 → 가리킨 반사판 철거
    UFUNCTION()
    void HandleSpecialAction(FGameplayEventData Payload);
    // =============================================================================
protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reflect")
    TSubclassOf<class APetReflector> ReflectorClass;

    // 고스트에 씌울 반투명 머티리얼 (반사판 메시 슬롯 순서대로)
    // 의도: 원래 반사판의 돌/금/보석/거울 느낌을 슬롯별로 그대로 살리기 위함
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reflect|Preview")
    TArray<TObjectPtr<class UMaterialInterface>> PlacementGhostMaterials;

    UPROPERTY(Transient)
    TObjectPtr<class UStaticMeshComponent> PlacementGhostMesh;

    // 설치 가능/불가 색을 바꾸기 위한 슬롯별 동적 머티리얼
    UPROPERTY(Transient)
    TArray<TObjectPtr<class UMaterialInstanceDynamic>> PlacementGhostMIDs;

    // ===== 방향 화살표 =====
    // 바닥에 눕힐 평면 메시 (엔진 기본 Plane 권장, 100x100 XY 평면 기준으로 화살표를 그려요)
    // 메시나 머티리얼이 비어 있으면 화살표 없이 동작해요
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reflect|Preview|Arrow")
    TObjectPtr<class UStaticMesh> FacingArrowMesh;

    // 화살표 모양을 그리는 반투명 머티리얼 (M_ReflectorFacingArrow)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reflect|Preview|Arrow")
    TObjectPtr<class UMaterialInterface> FacingArrowMaterial;

    // 화살표 길이 (앞 방향)
    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Preview|Arrow", meta = (ClampMin = "1.0", Units = "cm"))
    float FacingArrowLength = 90.f;

    // 화살표 폭 (옆 방향)
    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Preview|Arrow", meta = (ClampMin = "1.0", Units = "cm"))
    float FacingArrowWidth = 60.f;

    // 반사판 앞면과 화살표 꼬리 사이 간격
    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Preview|Arrow", meta = (ClampMin = "0.0", Units = "cm"))
    float FacingArrowGap = 10.f;

    // 바닥에서 살짝 띄우는 높이 (바닥과 겹쳐 깜빡이는 것 방지)
    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Preview|Arrow", meta = (ClampMin = "0.0", Units = "cm"))
    float FacingArrowHeight = 1.f;

    UPROPERTY(Transient)
    TObjectPtr<class UStaticMeshComponent> FacingArrowComponent;

    // ===== 레이저 반사 경로 미리보기 =====
    // 구간마다 늘려서 깔 평면 메시 (엔진 기본 Plane 권장)
    // 메시나 머티리얼이 비어 있으면 미리보기 없이 동작해요
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reflect|Preview|Laser")
    TObjectPtr<class UStaticMesh> LaserPreviewMesh;

    // 흐르는 점선 머티리얼 (M_ReflectorLaserPreview, SegmentLength 파라미터를 코드가 채워요)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reflect|Preview|Laser")
    TObjectPtr<class UMaterialInterface> LaserPreviewMaterial;

    // 점선 폭
    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Preview|Laser", meta = (ClampMin = "1.0", Units = "cm"))
    float LaserPreviewWidth = 12.f;

    // 한 번에 그릴 수 있는 최대 구간 수 (레이저가 많거나 반사가 많을 때 안전장치)
    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Preview|Laser", meta = (ClampMin = "1"))
    int32 MaxLaserPreviewSegments = 32;

    UPROPERTY(Transient)
    TArray<TObjectPtr<class UStaticMeshComponent>> LaserPreviewSegments;

    // LaserPreviewSegments와 같은 순서의 동적 머티리얼 (구간 길이 전달용)
    UPROPERTY(Transient)
    TArray<TObjectPtr<class UMaterialInstanceDynamic>> LaserPreviewMIDs;

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

    // 고스트 발광에 더해지는 색 (머티리얼의 GhostTint)
    // 설치 가능: 검정 = 원래 반사판 색 그대로 / 설치 불가: 빨갛게 빛남
    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Preview")
    FLinearColor ValidPreviewColor = FLinearColor::Black;

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

    // 철거 안내 키 UI
    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Remove")
    TSubclassOf<class UInteractionPromptWidget> RemovePromptWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Remove")
    FText RemovePromptKeyText = FText::FromString(TEXT("Q"));

    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Remove")
    FText RemovePromptActionText = FText::FromString(TEXT("철거"));

    // 반사판 중심 기준 안내 UI 위치
    UPROPERTY(EditDefaultsOnly, Category = "Reflect|Remove")
    FVector RemovePromptOffset = FVector(0.f, 0.f, 120.f);

    UPROPERTY(Transient)
    TObjectPtr<class UWidgetComponent> RemovePrompt;

    // 지금 철거 대상으로 가리키는 반사판
    TWeakObjectPtr<class APetReflector> HoveredReflector;
};
