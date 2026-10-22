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

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ActivatableInterface.h"
#include "Barrier.generated.h"

class UStaticMeshComponent;
class USphereComponent;
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

	// 수집 반경(수평)·태그 조건에 맞는 월드의 생성장치, 가까운 순
	TArray<ABarrierGenerator*> FindNearbyGenerators() const;

	UFUNCTION()
	void HandleGeneratorDisabled(ABarrierGenerator* Generator);

	bool AreAllGeneratorsDisabled() const;

	void DeactivateGenerators();

	void BreakBarrier();

	// 켜짐/꺼짐 시각 처리 (메시 표시 전환). BP에서 연출 추가 가능
	void ApplyVisualState();

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

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Barrier")
	bool bBarrierActive = false;
};
