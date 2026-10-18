/// 작성자: 최요환
/// 작성일자: 2026-10-18
///
/// 자력을 받는 액터에 붙이는 자성 컴포넌트입니다.
/// 액터에 N/S 극성을 부여하고, 극성에 맞는 색을 메시의 동적 머티리얼에 표시합니다.
/// 매 틱 UMagneticSubsystem에 등록된 자력원을 조회해서, 범위 안이고 사이에 벽이 없으면
/// 같은 극성은 자력원 반대쪽으로 밀리고 다른 극성은 자력원 쪽으로 끌려옵니다.
/// 루트 컴포넌트가 Simulate Physics일 때 AddForce로 힘을 적용하고, OnMagneticForce로 계산된 힘을 알립니다.
/// 상호작용 인터페이스와는 별개라 코인, 기믹 등 어떤 액터에도 붙일 수 있습니다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Gimmicks/Magnetic/MagneticTypes.h"
#include "MagneticComponent.generated.h"

class UMagneticComponent;
class UMaterialInstanceDynamic;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMagneticPolarityChanged, UMagneticComponent*, MagneticComponent, EMagneticPolarity, NewPolarity, EMagneticPolarity, OldPolarity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMagneticForce, UMagneticComponent*, MagneticComponent, FVector, Force);

UCLASS(ClassGroup = (Gimmick), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UMagneticComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMagneticComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 극성 변경 (같은 극성이면 무시). 색 갱신 + 델리게이트 브로드캐스트
	UFUNCTION(BlueprintCallable, Category = "Magnetic")
	void SetPolarity(EMagneticPolarity NewPolarity);

	UFUNCTION(BlueprintPure, Category = "Magnetic")
	EMagneticPolarity GetPolarity() const { return Polarity; }

	UFUNCTION(BlueprintCallable, Category = "Magnetic")
	void SetMagnetEnabled(bool bEnabled) { bMagnetEnabled = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Magnetic")
	bool IsMagnetEnabled() const { return bMagnetEnabled; }

	// 자력원 하나가 Location의 대상에 주는 힘 계산 (범위 밖·가림·극성 None이면 false)
	UFUNCTION(BlueprintCallable, Category = "Magnetic")
	bool CalculateForceFrom(AActor* SourceActor, const FMagneticSourceInfo& Info, const FVector& Location, FVector& OutForce) const;

public:
	UPROPERTY(BlueprintAssignable, Category = "Magnetic")
	FOnMagneticPolarityChanged OnPolarityChanged;

	// 힘을 받는 틱마다 브로드캐스트 (물리 미사용 액터의 이동 처리, 연출 등)
	UPROPERTY(BlueprintAssignable, Category = "Magnetic")
	FOnMagneticForce OnMagneticForce;

protected:
	virtual void BeginPlay() override;

	// 범위 판정 (Delta: 자력원 → 대상, Aim: 정규화된 조준 방향)
	static bool IsInsideField(const FMagneticSourceInfo& Info, const FVector& Delta, const FVector& Aim);

	// 자력원과 대상 사이에 벽이 있는지
	bool IsLineOfSightBlocked(AActor* SourceActor, const FVector& From, const FVector& To) const;

	void ApplyMagneticForce(const FVector& Force);

	void CreateDynamicMaterials();
	void UpdatePolarityColor();

protected:
	// 극성 (코인 등은 스폰 시 SetPolarity로 지정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic")
	EMagneticPolarity Polarity = EMagneticPolarity::N;

	// false면 자력에 반응하지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic")
	bool bMagnetEnabled = true;

	// 받는 힘 배율 (자력원 Strength에 곱함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic", meta = (ClampMin = "0.0"))
	float ForceMultiplier = 1.f;

	// true면 질량과 관계없이 같은 가속도 (AddForce bAccelChange)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic")
	bool bIgnoreMass = true;

	// 끌려올 때 자력원과 이 거리 안이면 더 당기지 않음 (펫 위치에서 떨림 방지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic", meta = (ClampMin = "0.0"))
	float PullStopDistance = 100.f;

	// 가림 판정에서 벽으로 취급할 오브젝트 타입
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic")
	TArray<TEnumAsByte<EObjectTypeQuery>> LineOfSightObjectTypes;

	// 극성 색을 메시 머티리얼에 표시
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic|Color")
	bool bApplyPolarityColor = true;

	// 극성별 색
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic|Color", meta = (EditCondition = "bApplyPolarityColor"))
	TMap<EMagneticPolarity, FLinearColor> PolarityColors;

	// 머티리얼의 벡터 파라미터 이름 (엔진 BasicShapeMaterial은 "Color")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic|Color", meta = (EditCondition = "bApplyPolarityColor"))
	FName ColorParameterName = TEXT("Color");

	// 이 태그가 있는 메시에만 색 적용 (None이면 액터의 모든 메시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic|Color", meta = (EditCondition = "bApplyPolarityColor"))
	FName ColorTargetTag;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;
};
