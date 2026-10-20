/// 작성자: 최요환
/// 작성일자: 2026-10-20
///
/// 보스 레이저 컴포넌트입니다.
/// 보스의 발사 위치(소켓)에 붙이고, 보스 공격 GA가 StartLaser / SetTargetLocation / StopLaser로 조작합니다.
/// 발사 방향은 StartLaser 시점의 목표 지점 방향이며(bPlanarAim이면 수평), 발사 중 SetTargetLocation·SetFireDirection으로 바꿀 수 있습니다.
/// 켜져 있는 동안 매 틱 발사 위치에서 MaxRange까지 트레이스하고, 처음 막힌 액터에
/// DamageInterval마다 DamagePerSecond × DamageInterval 만큼 ApplyDamage(DamageTypeClass, 기본 UDamageType_BossLaser) 합니다.
/// 연출: 빔 시작·끝·맞은 액터를 GetBeamStart/GetBeamEnd/GetHitActor로 조회 (BP 빔 연출용). 프로토타입은 bDrawDebugBeam 디버그 라인으로 표시합니다.
/// 반사판 반사는 담당자 별도 작업 (이 컴포넌트는 직선 빔만 처리)
/// TODO(합의): 보스 GA에서 호출하는 방식(예고·추적 등)은 보스 담당과 확정 필요
/// TODO(합의): 플레이어 피해는 ApplyDamage 호출까지 (GAS HP 반영은 플레이어 담당)

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "BossLaserComponent.generated.h"

class UBossLaserComponent;
class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossLaserStateChanged, UBossLaserComponent*, Laser);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBossLaserDamaged, UBossLaserComponent*, Laser, AActor*, DamagedActor, float, Damage);

UCLASS(ClassGroup = (Gimmick), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UBossLaserComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UBossLaserComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 레이저 발사: 현재 컴포넌트 위치에서 TargetLocation 방향 (Duration 0 이하면 LaserDuration 사용, 그것도 0이면 StopLaser까지)
	UFUNCTION(BlueprintCallable, Category = "BossLaser")
	void StartLaser(const FVector& TargetLocation, float Duration = -1.f);

	// 레이저 발사: TargetActor의 현재 위치를 목표 지점으로 사용
	UFUNCTION(BlueprintCallable, Category = "BossLaser")
	void StartLaserAtActor(AActor* TargetActor, float Duration = -1.f);

	UFUNCTION(BlueprintCallable, Category = "BossLaser")
	void StopLaser();

	// 발사 중 조준 변경 (GA가 추적·회전 등에 사용, 발사 중이 아니면 다음 발사 전까지 방향만 저장)
	UFUNCTION(BlueprintCallable, Category = "BossLaser")
	void SetTargetLocation(const FVector& TargetLocation);

	UFUNCTION(BlueprintCallable, Category = "BossLaser")
	void SetFireDirection(const FVector& Direction);

	UFUNCTION(BlueprintPure, Category = "BossLaser")
	bool IsLaserActive() const { return bLaserActive; }

	UFUNCTION(BlueprintPure, Category = "BossLaser")
	FVector GetFireDirection() const { return FireDirection; }

	// 이번 틱 빔 시작·끝 위치 (발사 중이 아니면 마지막 값)
	UFUNCTION(BlueprintPure, Category = "BossLaser")
	FVector GetBeamStart() const { return BeamStart; }

	UFUNCTION(BlueprintPure, Category = "BossLaser")
	FVector GetBeamEnd() const { return BeamEnd; }

	// 빔에 막힌 액터 (사거리 끝까지 아무것도 없으면 null)
	UFUNCTION(BlueprintPure, Category = "BossLaser")
	AActor* GetHitActor() const { return HitActor.Get(); }

public:
	UPROPERTY(BlueprintAssignable, Category = "BossLaser")
	FOnBossLaserStateChanged OnLaserStarted;

	UPROPERTY(BlueprintAssignable, Category = "BossLaser")
	FOnBossLaserStateChanged OnLaserStopped;

	// 맞은 액터가 실제로 피해를 받았을 때 (피해량 0 초과)
	UPROPERTY(BlueprintAssignable, Category = "BossLaser")
	FOnBossLaserDamaged OnLaserDamaged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 발사 위치에서 빔 경로를 다시 계산
	void UpdateBeam();

	// 빔에 막힌 액터에 피해
	void ApplyLaserDamage(float DeltaTime);

	void DrawDebugBeam() const;

	// Classes가 비어 있으면 true, 아니면 그중 하나의 하위 클래스인지
	static bool IsActorOfClasses(const AActor* Actor, const TArray<TSubclassOf<AActor>>& Classes);

protected:
	// 발사 후 자동 종료까지의 시간 (0이면 StopLaser를 부를 때까지 유지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser", meta = (ClampMin = "0.0", Units = "s"))
	float LaserDuration = 0.f;

	// true면 목표 지점의 높이를 무시하고 발사 위치 높이에서 수평으로 발사 (쿼터뷰에서 바닥에 꽂히지 않게)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser")
	bool bPlanarAim = true;

	// 빔 최대 길이
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Trace", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxRange = 4000.f;

	// 빔 굵기 (0이면 라인 트레이스, 0보다 크면 구 스윕)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Trace", meta = (ClampMin = "0.0", Units = "cm"))
	float BeamRadius = 5.f;

	// 레이저를 막는 오브젝트 타입 (플레이어·벽·기믹)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Trace")
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

	// 초당 피해량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Damage", meta = (ClampMin = "0.0"))
	float DamagePerSecond = 20.f;

	// 피해를 주는 간격 (한 번에 DamagePerSecond × DamageInterval)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Damage", meta = (ClampMin = "0.05", Units = "s"))
	float DamageInterval = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Damage")
	TSubclassOf<UDamageType> DamageTypeClass;

	// 이 클래스(하위 포함) 액터에만 피해 (비어 있으면 모든 액터)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Damage")
	TArray<TSubclassOf<AActor>> DamageTargetClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossLaser|Debug")
	bool bDrawDebugBeam = true;

private:
	FVector FireDirection = FVector::ForwardVector;

	FVector BeamStart = FVector::ZeroVector;

	FVector BeamEnd = FVector::ZeroVector;

	TWeakObjectPtr<AActor> HitActor;

	bool bLaserActive = false;

	// 0 이하면 시간 제한 없음
	float RemainingTime = 0.f;

	float DamageTimer = 0.f;
};
