/// 작성자: 최요환
/// 작성일자: 2026-10-21
///
/// 공격 예고 범위를 바닥에 표시하는 데칼 액터입니다.
/// 아래로 투영하는 DecalComponent 하나로 구성되며, 머티리얼의 Color·Progress 파라미터로 색과 진행도를 표시합니다.
/// 사용법: SpawnActor 후 ShowCircleWarning(지점, 반경, 시간, 색) 또는 ShowWarning(지점, 절반 크기, Yaw, 시간, 색)
///         → 공격이 끝나면 HideWarning (bDestroyOnHide면 액터 소멸)
/// Duration > 0이면 Progress가 0 → 1로 자동 증가, 0이면 SetProgress로 직접 갱신합니다.
/// 원형·사각형 모양은 머티리얼이 결정하므로 모양별로 BP(머티리얼만 다름)를 만들어 씁니다.
/// 머티리얼 로컬 X(UV.x) = 액터 정면(Yaw 방향)이며, 사각형은 UV.x 0 쪽에서부터 진행도가 차오릅니다. 실제 크기는 SizeX·SizeY 파라미터로 넘깁니다.
/// 자성 코인 낙하 지점 표시에 먼저 사용하고, 보스 공격 GA·레이저 예고에도 재사용할 수 있습니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AttackWarningDecal.generated.h"

class UDecalComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class MYNICELITTLECAT_API AAttackWarningDecal : public AActor
{
	GENERATED_BODY()

public:
	AAttackWarningDecal();

	virtual void Tick(float DeltaTime) override;

	// 사각형 범위 표시 (HalfSize: 바닥 기준 X·Y 절반 크기, Yaw: 바닥 회전, Duration 0이면 진행도 수동)
	UFUNCTION(BlueprintCallable, Category = "AttackWarning")
	void ShowWarning(const FVector& Location, FVector2D HalfSize, float Yaw, float Duration, FLinearColor Color);

	// 원형 범위 표시 (머티리얼이 원형일 때)
	UFUNCTION(BlueprintCallable, Category = "AttackWarning")
	void ShowCircleWarning(const FVector& Location, float Radius, float Duration, FLinearColor Color);

	// 표시 종료 (bDestroyOnHide면 소멸, 아니면 숨김)
	UFUNCTION(BlueprintCallable, Category = "AttackWarning")
	void HideWarning();

	// 진행도 직접 지정 (0~1)
	UFUNCTION(BlueprintCallable, Category = "AttackWarning")
	void SetProgress(float NewProgress);

	UFUNCTION(BlueprintCallable, Category = "AttackWarning")
	void SetColor(FLinearColor NewColor);

	UFUNCTION(BlueprintPure, Category = "AttackWarning")
	float GetProgress() const { return Progress; }

	UFUNCTION(BlueprintPure, Category = "AttackWarning")
	bool IsShowing() const { return bShowing; }

protected:
	virtual void BeginPlay() override;

	UMaterialInstanceDynamic* GetOrCreateDynamicMaterial();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	// 아래(-Z)로 투영하는 데칼 (X = 투영 깊이, Y·Z = 바닥 크기)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDecalComponent> Decal;

	// Deferred Decal 머티리얼 (Color 벡터, Progress 스칼라 파라미터 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AttackWarning")
	TObjectPtr<UMaterialInterface> WarningMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AttackWarning")
	FName ColorParameterName = TEXT("Color");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AttackWarning")
	FName ProgressParameterName = TEXT("Progress");

	// 표시 범위의 실제 크기(cm, 로컬 X·Y 전체 길이)를 넘길 스칼라 파라미터 (사각형 테두리 두께를 cm로 맞출 때 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AttackWarning")
	FName SizeXParameterName = TEXT("SizeX");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AttackWarning")
	FName SizeYParameterName = TEXT("SizeY");

	// 위아래로 투영하는 거리 (바닥 높낮이가 있으면 늘림). 지점 기준 위아래 절반씩
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AttackWarning", meta = (ClampMin = "1.0"))
	float ProjectionDepth = 200.f;

	// 거리에 따라 데칼이 흐려지지 않게 (0이면 엔진 기본)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AttackWarning", meta = (ClampMin = "0.0"))
	float FadeScreenSize = 0.f;

	// 표시 중에 Progress가 1에 도달하면 자동으로 HideWarning
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AttackWarning")
	bool bHideOnComplete = false;

	// HideWarning 시 액터 소멸 (false면 숨기고 다시 Show 가능)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AttackWarning")
	bool bDestroyOnHide = true;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	bool bShowing = false;

	float Progress = 0.f;

	// 0 이하면 진행도 자동 증가 안 함
	float ShowDuration = 0.f;

	float ShowElapsed = 0.f;
};
