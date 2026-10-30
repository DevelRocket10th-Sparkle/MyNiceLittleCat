/// 작성자: 최요환
/// 작성일자: 2026-10-15
///
/// 보스전 배리어입니다.
/// 에디터에서 지정한 생성장치(Generators)를 구독하고, 배리어가 켜져 있는 동안 전부 무력화되면 붕괴합니다.
/// Generators를 비워 두면 BeginPlay에서 GeneratorCollectRadius(수평 거리) 안의 생성장치를 자동으로 모읍니다.
/// 같은 아레나에 배리어가 여러 개면 GeneratorCollectTag로 구분하거나 Generators를 직접 지정합니다.
/// 비활성 상태로 시작하며, 켜질 때 생성장치를 가동하고 붕괴/해제 시 생성장치도 정지합니다.
/// 보스 페이즈(70%, 30%)마다 별도 인스턴스를 배치하고 각각 생성장치를 연결해서 사용합니다.
/// 보스와는 느슨하게 연결합니다: 보스 페이즈가 ActivateBarrier()를 호출하고 OnBarrierBroken을 구독하며,
/// 보스 피격 처리에서 IsBarrierActive()로 피해 무효 여부를 판단합니다. 메시는 시각 표현용입니다.
/// IActivatableInterface도 구현해서 레버 등 다른 기믹으로도 켜고 끌 수 있습니다.
/// 켜고 꺼질 때 메시 머티리얼의 Dissolve 스칼라 파라미터(M_Barrier_Shield_v1)를 시간에 따라 바꿔 나타남·사라짐을 연출합니다.
/// AttachToFollowTarget으로 보스 등에 붙이면 켜져 있는 동안 따라가고(회전·크기는 유지), 다 사라지면 레벨에 놓인 원래 자리로 돌아옵니다.
/// 연결선(TetherEffect): 켜져 있는 동안 살아 있는 생성장치마다 생성장치 → 배리어 표면으로 에너지가 흐르는 이펙트를 이어
/// 생성장치가 배리어를 유지한다는 것을 보여 줍니다. 시작점은 생성장치에서 TetherSocketTag 태그가 붙은 컴포넌트(없으면 바운드 윗면),
/// 끝점은 배리어 메시 표면 중 생성장치 쪽 지점이며 매 틱 갱신합니다(보스를 따라가도 이어짐). 생성장치가 무력화되면 그 선만 끊김

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ActivatableInterface.h"
#include "Barrier.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class UNiagaraComponent;
class ABarrierGenerator;
class ABarrier;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBarrierStateChanged, ABarrier*, Barrier);

// TODO(합의): 보스 담당과 배리어 활성화 / 깨짐 알림 방식 확정 필요 (현재: 보스가 ABarrier를 직접 참조)
UCLASS()
class MYNICELITTLECAT_API ABarrier : public AActor, public IActivatableInterface
{
	GENERATED_BODY()

public:
	ABarrier();

	// IActivatableInterface
	virtual void Activate_Implementation(AActor* Activator) override;
	virtual void Deactivate_Implementation(AActor* Activator) override;

	// 배리어 켜기: 연결된 생성장치 전부 가동 후 표시
	UFUNCTION(BlueprintCallable, Category = "Barrier")
	void ActivateBarrier();

	// 배리어 끄기 (깨짐 이벤트 없이 끔, 페이즈 종료 등 외부 사유). 생성장치도 정지
	UFUNCTION(BlueprintCallable, Category = "Barrier")
	void DeactivateBarrier();

	UFUNCTION(BlueprintPure, Category = "Barrier")
	bool IsBarrierActive() const { return bBarrierActive; }

	// 대상 위치(+ FollowOffset)에 붙여 따라가게 함. 회전·크기는 지금 월드 값 유지
	UFUNCTION(BlueprintCallable, Category = "Barrier|Follow")
	void AttachToFollowTarget(AActor* Target);

	// 대상에서 떼고 BeginPlay 때의 자리로 복귀 (다 사라지면 자동 호출)
	UFUNCTION(BlueprintCallable, Category = "Barrier|Follow")
	void ReturnToHome();

	// 수집 반경 안의 생성장치로 Generators를 채움 (에디터 디테일 버튼으로 미리 확인·고정용)
	UFUNCTION(CallInEditor, Category = "Barrier")
	void CollectNearbyGenerators();

public:
	UPROPERTY(BlueprintAssignable, Category = "Barrier")
	FOnBarrierStateChanged OnBarrierActivated;

	// 생성장치 전부 무력화로 붕괴
	UPROPERTY(BlueprintAssignable, Category = "Barrier")
	FOnBarrierStateChanged OnBarrierBroken;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// 수집 반경(수평)·태그 조건에 맞는 월드의 생성장치, 가까운 순
	TArray<ABarrierGenerator*> FindNearbyGenerators() const;

	UFUNCTION()
	void HandleGeneratorDisabled(ABarrierGenerator* Generator);

	bool AreAllGeneratorsDisabled() const;

	void DeactivateGenerators();

	void BreakBarrier();

	// 켜짐/꺼짐 시각 처리 (메시 표시 전환 + Dissolve 연출). BP에서 연출 추가 가능
	// Duration이 0 이하면 즉시 전환 (BeginPlay 초기 상태 등)
	void ApplyVisualState(float DissolveDuration = 0.f);

	// Dissolve 값을 현재 값에서 Target으로 Duration 동안 변경, 끝나면 Target이 1일 때 메시 숨김
	void StartDissolve(float Target, float Duration);

	void SetDissolveValue(float Value);

	// Dissolve가 목표에 도달했을 때 (1이면 숨기고 원래 자리로 복귀)
	void FinishDissolve();

	UFUNCTION()
	void HandleFollowTargetEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

	// 살아 있는 생성장치마다 연결선 켜기
	void ShowTethers();

	// 생성장치 하나의 연결선 끄기 (bBroken이면 끊김 이펙트)
	void HideTether(ABarrierGenerator* Generator, bool bBroken);

	void HideAllTethers();

	// 연결선 시작·끝 위치 갱신
	void UpdateTethers();

	FVector GetTetherStart(const ABarrierGenerator* Generator) const;
	FVector GetTetherEnd(const FVector& TetherStart) const;

	// Dissolve 연출 중이거나 연결선이 있을 때만 틱
	void RefreshTickEnabled();

	UFUNCTION(BlueprintImplementableEvent, Category = "Barrier", meta = (DisplayName = "On Barrier State Changed"))
	void K2_OnBarrierStateChanged(bool bActive, bool bBroken);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// 수집 반경 표시용 (에디터에서만 보임, 콜리전 없음)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollectRadiusPreview;

	// 이 배리어를 유지하는 생성장치들 (레벨에서 지정, 비우면 자동 수집)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Barrier")
	TArray<TObjectPtr<ABarrierGenerator>> Generators;

	// Generators가 비어 있으면 BeginPlay에서 근처 생성장치를 자동 수집
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|AutoCollect")
	bool bAutoCollectGenerators = true;

	// 자동 수집 반경 (배리어 위치 기준 수평 거리, cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|AutoCollect", meta = (ClampMin = "0.0", Units = "cm"))
	float GeneratorCollectRadius = 1500.f;

	// 비어 있지 않으면 이 액터 태그를 가진 생성장치만 수집 (배리어 여러 개를 한 아레나에 둘 때 구분용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|AutoCollect")
	FName GeneratorCollectTag;

	// 시작하자마자 켤지 여부 (보스전에선 보통 false, 보스 페이즈가 켬)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier")
	bool bActivateOnBeginPlay = false;

	// 메시 머티리얼의 사라짐 정도 파라미터 (0 = 보임, 1 = 완전히 사라짐). 없는 머티리얼이면 즉시 표시/숨김
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Visual")
	FName DissolveParamName = TEXT("Dissolve");

	// 켜질 때 나타나는 시간 (0이면 즉시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Visual", meta = (ClampMin = "0.0", Units = "s"))
	float AppearDuration = 0.5f;

	// 생성장치가 전부 무력화돼 붕괴할 때 사라지는 시간 (0이면 즉시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Visual", meta = (ClampMin = "0.0", Units = "s"))
	float BreakDissolveDuration = 1.0f;

	// 외부에서 끌 때(페이즈 정지·리셋 등) 사라지는 시간 (0이면 즉시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Visual", meta = (ClampMin = "0.0", Units = "s"))
	float DeactivateDissolveDuration = 0.5f;

	// 따라갈 때 대상 위치에서 띄울 거리 (대상 기준 로컬, 구 중심 높이 조정용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Follow")
	FVector FollowOffset = FVector::ZeroVector;

	// 생성장치 → 배리어 연결선 이펙트 (비어 있으면 연결선 없음). 시스템 위치 = 시작점
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Tether")
	TObjectPtr<UNiagaraSystem> TetherEffect;

	// 생성장치가 무력화돼 연결선이 끊길 때 시작점에 한 번 재생 (선택)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Tether")
	TObjectPtr<UNiagaraSystem> TetherBreakEffect;

	// 연결선 색 (배리어 종류별로 다르게: 자성 코인용·반사 레이저용 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Tether")
	FLinearColor TetherColor = FLinearColor(0.25f, 0.75f, 1.6f);

	// 연결선이 위로 휘는 높이 (직선이면 바닥·보스에 가려지기 쉬움)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Tether", meta = (Units = "cm"))
	float TetherArcHeight = 200.f;

	// 생성장치에서 이 컴포넌트 태그가 붙은 컴포넌트 위치를 시작점으로 사용 (없으면 생성장치 바운드 윗면 중앙)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Tether")
	FName TetherSocketTag = TEXT("BarrierTether");

	// 시작점에 더할 월드 오프셋
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Tether")
	FVector TetherStartOffset = FVector::ZeroVector;

	// 끝점 = 배리어 중심에서 끝점 방향으로 메시 반경 × 이 값 (1 = 표면, 작으면 안쪽)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Tether", meta = (ClampMin = "0.0"))
	float TetherEndRadiusScale = 1.f;

	// 끝점을 구 표면 위쪽으로 올리는 정도 (0 = 시작점을 바라보는 방향 그대로, 1 = 구 꼭대기)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Tether", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TetherEndUpBias = 0.5f;

	// 끝점에 더할 월드 오프셋
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Tether")
	FVector TetherEndOffset = FVector::ZeroVector;

	// Niagara User 파라미터 이름 (Vector, 월드 좌표)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Tether|Parameters")
	FName TetherEndParam = TEXT("TetherEnd");

	// Niagara User 파라미터 이름 (Linear Color)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Tether|Parameters")
	FName TetherColorParam = TEXT("Color");

	// Niagara User 파라미터 이름 (Float)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier|Tether|Parameters")
	FName TetherArcHeightParam = TEXT("ArcHeight");

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Barrier")
	bool bBarrierActive = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MeshMID;

	float DissolveValue = 0.f;
	float DissolveFrom = 0.f;
	float DissolveTarget = 0.f;
	float DissolveDurationCurrent = 0.f;
	float DissolveElapsed = 0.f;
	bool bDissolving = false;

	// 생성장치별 연결선 이펙트 (생성장치에 붙여 둠)
	UPROPERTY(Transient)
	TMap<TObjectPtr<ABarrierGenerator>, TObjectPtr<UNiagaraComponent>> TetherComponents;

	// 레벨에 놓인 자리 (따라가기 후 복귀용)
	FTransform HomeTransform;

	TWeakObjectPtr<AActor> FollowTarget;
};
