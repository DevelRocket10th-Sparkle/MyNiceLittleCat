// 작성자: 우진
// 작성일자: 2026-11-07
//
// 플레이어가 닿으면 능력 해금 아이템을 주는 임시 구슬
// 의도: 보물상자(GimmickChest)와 같은 알림 순서(AddItems → NotifyAcquireFinished)를 따라서
//       나중에 구슬을 상자 등으로 바꿔도 펫/튜토리얼 코드는 그대로 동작하게

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/ItemTypes.h"
#include "AbilityPickup.generated.h"

UCLASS()
class MYNICELITTLECAT_API AAbilityPickup : public AActor
{
	GENERATED_BODY()

public:
	AAbilityPickup();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override; // 둥실거림 + 회전
	// =============================================================================
protected:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void FinishAcquire(); // 획득 UI가 끝나는 시점 → "능력 획득 연출 끝" 알림 후 제거
	void ApplyLook();     // 아이콘 그림/색을 머티리얼과 조명에 적용 (에디터 미리보기 + 플레이 공통)

#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override; // 레벨에 놓았을 때도 바로 모양이 보이게
#endif
	// =============================================================================
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<class USphereComponent> PickupSphere; // 먹는 판정

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<class UStaticMeshComponent> Mesh; // 반투명 구슬 껍질 (메시/머티리얼은 BP에서, 예: M_AbilityOrb_Shell)

	// 구슬 안에 떠 있는 모드 그림 (항상 카메라를 바라보는 판)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<class UMaterialBillboardComponent> IconBillboard;

	// 구슬 주변 바닥/벽을 은은하게 물들이는 빛 (어두운 곳에서도 눈에 띄게)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<class UPointLightComponent> GlowLight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<class UItemData> AbilityItem; // 줄 능력 아이템 (예: DA_Item_Ability_PetMagnet)

	// ---- 겉모양 (조이 주문 파편 느낌: 반투명 구슬 + 안에 빛나는 모드 그림) ----
	// 아이콘 판에 쓸 머티리얼 (Icon 텍스처 / GlowColor / IconColor 파라미터 필요, 예: M_AbilityOrb_Icon)
	UPROPERTY(EditAnywhere, Category = "Pickup|Look")
	TObjectPtr<class UMaterialInterface> IconMaterial;

	// 구슬 안에 보일 모드 그림 (흰색 실루엣 권장, 예: T_PetDial_Magnet)
	UPROPERTY(EditAnywhere, Category = "Pickup|Look")
	TObjectPtr<class UTexture2D> IconTexture;

	// 구슬 테두리·안쪽 빛·조명 색 (모드 색에 맞춰 BP에서 바꿈)
	UPROPERTY(EditAnywhere, Category = "Pickup|Look")
	FLinearColor GlowColor = FLinearColor(0.25f, 0.65f, 1.f);

	// 아이콘 그림 자체 색 (기본 흰색 → 가장 또렷하게 보임)
	UPROPERTY(EditAnywhere, Category = "Pickup|Look")
	FLinearColor IconColor = FLinearColor::White;

	// 아이콘 판 한 변 크기 (구슬 지름보다 조금 작게)
	UPROPERTY(EditAnywhere, Category = "Pickup|Look", meta = (ClampMin = "1.0", Units = "cm"))
	float IconSize = 32.f;

	// 구슬이 위아래로 둥실거리는 높이 / 속도 (0이면 정지)
	UPROPERTY(EditAnywhere, Category = "Pickup|Bob", meta = (ClampMin = "0.0", Units = "cm"))
	float BobHeight = 15.f;

	UPROPERTY(EditAnywhere, Category = "Pickup|Bob", meta = (ClampMin = "0.0"))
	float BobSpeed = 2.f;

	// 초당 회전 각도
	UPROPERTY(EditAnywhere, Category = "Pickup|Bob")
	float SpinDegreesPerSecond = 60.f;

private:
	UPROPERTY(Transient)
	TArray<FItemStack> AcquiredItems; // 실제로 얻은 것 (Notify에 전달)

	FTimerHandle FinishTimerHandle;
	FVector MeshBaseLocation = FVector::ZeroVector; // 둥실거림 기준 위치
	float BobTime = 0.f;
	bool bPicked = false; // 중복 획득 방지
};
