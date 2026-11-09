/// 극성 에너지장 기믹입니다.
/// FieldVolume 안에 들어온 자성 물체(UMagneticComponent)의 극성을 FieldPolarity로 바꿉니다.
/// - Imprint: 통과한 물체에 극성을 새김 (나가도 그대로). 색 게이트 등
/// - Suppress: 안에 있는 동안만 극성을 덮어쓰고, 나가거나 장이 꺼지면 원래 극성으로 되돌림. 절연장(None) 등
/// 켜짐 = 장 작동. 다른 기믹 신호로 켜고 끌 수 있습니다 (예: 발판을 밟으면 꺼지는 절연장 = bInvertSignal).
/// 펫이 들고 가던 물체의 극성이 펫과 같아지면 펫 자석이 그 물체를 내던지고, None이 되면 떨어뜨립니다.
/// 플레이어·펫 자체는 막지 않습니다 (콜리전 없음, 주기 검사).
/// FieldMesh(기본 큐브)는 FieldVolume 크기에 맞춰 늘어나며, FieldMaterial의 ColorParameterName에 극성 색을 넣습니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/GimmickActorBase.h"
#include "Gimmicks/Magnetic/MagneticTypes.h"
#include "GimmickPolarityField.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMagneticComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class AGimmickPolarityField;

UENUM(BlueprintType)
enum class EPolarityFieldMode : uint8
{
	// 통과하면 극성이 바뀐 채로 유지
	Imprint,
	// 안에 있는 동안만 극성을 덮어씀 (나가면 원래대로)
	Suppress
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPolarityFieldApplied, AGimmickPolarityField*, Field, AActor*, Target, EMagneticPolarity, OldPolarity);

UCLASS()
class MYNICELITTLECAT_API AGimmickPolarityField : public AGimmickActorBase
{
	GENERATED_BODY()

public:
	AGimmickPolarityField();

	UFUNCTION(BlueprintPure, Category = "Gimmick|PolarityField")
	EMagneticPolarity GetFieldPolarity() const { return FieldPolarity; }

public:
	// 물체의 극성을 바꿨을 때 (소리·이펙트)
	UPROPERTY(BlueprintAssignable, Category = "Gimmick|PolarityField")
	FOnPolarityFieldApplied OnFieldApplied;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator) override;

	void CheckField();
	void ReleaseAllSuppressed();
	void UpdateVisual();
	FLinearColor GetFieldColor() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	// 장 영역. 크기는 BoxExtent로 조절 (FieldMesh가 따라 늘어남)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> FieldVolume;

	// 장 표시 메시 (콜리전 없음)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FieldMesh;

	// 장이 새기는 극성 (None = 절연: 펫 자석이 잡을 수 없음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolarityField")
	EMagneticPolarity FieldPolarity = EMagneticPolarity::S;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolarityField")
	EPolarityFieldMode FieldMode = EPolarityFieldMode::Imprint;

	// 감지할 오브젝트 타입 (자성 박스 = PhysicsBody, 폭탄 = Pawn)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolarityField")
	TArray<TEnumAsByte<EObjectTypeQuery>> AffectedObjectTypes;

	// 검사 주기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolarityField", meta = (ClampMin = "0.02", Units = "s"))
	float CheckInterval = 0.05f;

	/* 표시 */

	// 장 머티리얼 (예: M_Barrier_Shield_v1). 비면 메시 기본 머티리얼
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolarityField|Visual")
	TObjectPtr<UMaterialInterface> FieldMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolarityField|Visual")
	FName ColorParameterName = TEXT("Color");

	// 극성별 장 색 (머티리얼이 언릿이면 1보다 크게 해서 빛나게)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolarityField|Visual")
	TMap<EMagneticPolarity, FLinearColor> FieldColors;

	// 꺼졌을 때 메시를 숨김
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolarityField|Visual")
	bool bHideWhenOff = true;

private:
	// Suppress: 덮어쓴 물체와 원래 극성
	TMap<TWeakObjectPtr<UMagneticComponent>, EMagneticPolarity> SuppressedOriginals;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FieldMID;

	FTimerHandle CheckTimer;
};
