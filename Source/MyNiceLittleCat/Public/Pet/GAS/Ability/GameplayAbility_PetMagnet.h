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
public:
    AActor* GetCarriedActor() const { return bAttachedToCarryPoint ? HeldTarget.Get() : nullptr; }
protected:
    UFUNCTION()
    void HandleMagnetTick(float DeltaTime);

    void DrawMagnetDebugRange() const;

    // 부채꼴을 여러 방향의 광선으로 나눠 쏘고, 방향마다 자기장이 실제로 도달하는 거리를 구함
    // 의도: 디버그 표시 / 실제 이펙트 메시가 같은 결과를 쓰도록 한 곳에서 계산
    void TraceMagnetFan(TArray<FVector>& OutDirections, TArray<float>& OutReachLengths) const;

    void CollectMagnetTargetsInRange(TArray<AActor*>& OutTargets) const;
    
    // 원점과 대상 사이에 World Static(벽)이 있으면 true → 자기장이 차폐됨
    bool IsShieldedByWall(const FVector& Origin, const AActor* Target) const;

    AActor* FindNearestMagnetTarget(const TArray<AActor*>& Candidates) const;
    void ApplyNearbyRepulsion(const TArray<AActor*>& Candidates);

    bool IsMagnetCandidate(const AActor* _actor) const; //! 자성인지 아닌지 나중에 여기서 변경해야 함요

    UFUNCTION()
    void HandleSpecialAction(FGameplayEventData Payload);

    //! 임시 물체 연결부 체크 코드
    bool TryGetTargetPolarity(const AActor* Target, EPetMagnetPolarity& OutPolarity) const;

    void BeginAttractTarget(AActor* Target);
    void UpdateHeldTarget(float DeltaTime);
    FVector CalculateCarryOffset(const UPrimitiveComponent* Body) const;
    void ReleaseHeldTarget(float ReleaseSpeed, float UpwardRatio = 0.f);

    // 들고 있던 물체를 마지막으로 정상이었던 위치에 내려놓음
    // 의도: 펫이 벽 속, 벽 너머로 갔을 때 물체가 따라가서 벽을 통과하는 것을 막기 위함
    void DropHeldTargetAtLastValidLocation();

    // 극변환 시 애니메이션 몽타주
    void PlayPolarityChangeAnimation();
    void StopPolarityChangeAnimation();

    // TraceMagnetFan 결과로 자기장 부채꼴 메시를 다시 만듦 (벽에 막힌 모양 그대로)
    void UpdateMagnetFieldMesh();
    void HideMagnetFieldMesh();

    // 부채꼴 테두리를 따라 위로 솟은 반투명 커튼(섹션 1)을 만듦
    // 의도: 바닥 평면만으로는 납작해 보이므로, 같은 광선 결과로 "벽"을 세워 입체감 + 차폐 지점 표현
    void UpdateMagnetCurtainSection(class UProceduralMeshComponent* FieldMesh, const FVector& WorldOrigin,
        const TArray<FVector>& Directions, const TArray<float>& ReachLengths);

    // ========================================================================================
protected:  
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Range", meta = (ClampMin = "1.0", Units = "cm"))
    float RangeLength = 500.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Range", meta = (ClampMin = "1.0", ClampMax = "89.0", Units = "deg"))
    float HalfAngleDegrees = 15.f;

    // 펫 주변 원형 자기장 반경 (부채꼴과 별개로 이 안의 자성 액터는 방향과 상관없이 반응)
    UPROPERTY(EditDefaultsOnly, Category = "Magnet|Range", meta = (ClampMin = "0.0", Units = "cm"))
    float NearbyFieldRadius = 150.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Range", meta = (ClampMin = "0.0", Units = "cm"))
    float ForwardOffset = 40.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Range", meta = (ClampMin = "1.0", Units = "cm"))
    float TargetHeightTolerance = 150.f;

    // 부채꼴을 몇 등분해서 광선을 쏠지 (얇은 벽이 광선 사이로 빠지면 값을 올리세요)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Range", meta = (ClampMin = "2", ClampMax = "128"))
    int32 FanRayCount = 24;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Magnet")
    EPetMagnetPolarity CurrentPolarity = EPetMagnetPolarity::N;

    UPROPERTY(EditDefaultsOnly, Category = "Magnet|Test", meta = (ClampMin = "1.0", Units = "cm/s"))
    float AttractionSpeed = 1000.f;

    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> HeldTarget;

    UPROPERTY(Transient)
    TWeakObjectPtr<class UPrimitiveComponent> HeldBody;

    // 펫이 정상 위치였을 때 들고 있던 물체의 마지막 위치
    FVector LastValidHeldLocation = FVector::ZeroVector;
    bool bHasLastValidHeldLocation = false;

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magnet|Animation")
    TObjectPtr<class UAnimMontage> PolarityChangeMontage;

    UPROPERTY(Transient)
    TObjectPtr<class UAbilityTask_PlayMontageAndWait> PolarityChangeTask;

    // 극성별 자기장 머티리얼 (BP에서 지정)
    UPROPERTY(EditDefaultsOnly, Category = "Magnet|Visual")
    TObjectPtr<class UMaterialInterface> FieldMaterialN;

    UPROPERTY(EditDefaultsOnly, Category = "Magnet|Visual")
    TObjectPtr<class UMaterialInterface> FieldMaterialS;

    // 바닥과 겹쳐서 깜빡이지(Z-fighting) 않도록 살짝 띄우는 높이
    UPROPERTY(EditDefaultsOnly, Category = "Magnet|Visual", meta = (ClampMin = "0.0", Units = "cm"))
    float FieldMeshHeightOffset = 3.f;

    // 디버그 부채꼴(노란 광선/빨간 윤곽) 표시 여부
    // 의도: 실제 이펙트 메시를 확인할 때는 끄고, 판정 범위를 확인할 때만 켜기 위함
    UPROPERTY(EditDefaultsOnly, Category = "Magnet|Debug")
    bool bDrawMagnetDebug = false;

    // 극성별 테두리 커튼 머티리얼 (BP에서 지정)
    UPROPERTY(EditDefaultsOnly, Category = "Magnet|Visual|Curtain")
    TObjectPtr<class UMaterialInterface> CurtainMaterialN;

    UPROPERTY(EditDefaultsOnly, Category = "Magnet|Visual|Curtain")
    TObjectPtr<class UMaterialInterface> CurtainMaterialS;

    // 커튼 높이 (0이면 커튼을 만들지 않음 → 끄는 스위치로도 사용)
    UPROPERTY(EditDefaultsOnly, Category = "Magnet|Visual|Curtain", meta = (ClampMin = "0.0", Units = "cm"))
    float CurtainHeight = 60.f;

    // 테두리 몇 cm마다 UV가 0→1 한 번 도는지
    // 의도: 부채꼴 크기·벽 차폐로 둘레가 바뀌어도 무늬 밀도가 일정하게 유지되도록
    UPROPERTY(EditDefaultsOnly, Category = "Magnet|Visual|Curtain", meta = (ClampMin = "1.0", Units = "cm"))
    float CurtainUVTileLength = 100.f;

    // 캐릭터를 잡았을 때 원래 이동 모드 (놓을 때 복원)
    TEnumAsByte<EMovementMode> SavedTargetMovementMode = MOVE_Walking;
    bool bSavedTargetMovement = false;
};
