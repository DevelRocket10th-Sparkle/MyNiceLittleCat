/// 작성자: 최요환
/// 작성일자: 2026-10-20
///
/// 레이저 빔 컴포넌트입니다. 빔 하나의 발사·경로·피해를 처리하며, 보스 공격과 스테이지 함정에서 함께 씁니다.
/// - 보스: 발사 위치(소켓)에 붙이고, 보스 공격 GA가 StartLaser / SetTargetLocation / StopLaser로 조작
/// - 함정: 함정 액터에 붙이고 StartLaserForward로 컴포넌트 정면 발사, bFollowComponentRotation이면 액터가 돌 때 빔도 따라 돎
/// 발사 방향은 StartLaser 시점의 목표 지점 방향이며(bPlanarAim이면 수평), 발사 중 SetTargetLocation·SetFireDirection으로 바꿀 수 있습니다.
/// 켜져 있는 동안 매 틱 발사 위치에서 MaxRange까지 트레이스하고, 처음 막힌 액터에
/// 닿은 즉시 한 번, 계속 닿아 있으면 DamageInterval마다 DamagePerHit 만큼 피해를 줍니다.
/// - 플레이어(APlayerCharacter): ApplyPlayerHit 직접 호출 (피해·경직·무적, 빔 옆으로 밀림: 플레이어가 있는 쪽의 수직 방향)
/// - 그 외 액터: ApplyDamage(DamageTypeClass, 기본 UDamageType_BossLaser)
/// 연출: 빔 시작·끝·맞은 액터를 GetBeamStart/GetBeamEnd/GetHitActor로 조회 (BP 빔 연출용). 프로토타입은 bDrawDebugBeam 디버그 라인으로 표시합니다.
/// 반사판 반사는 담당자 별도 작업 (이 컴포넌트는 직선 빔만 처리).
/// 반사 구현용으로 맞은 면 법선(GetHitNormal)·트레이스 결과(GetHitResult)를 제공합니다.
/// 반사된 빔은 이 컴포넌트를 하나 더 써서 DamageTypeClass를 UDamageType_ReflectedLaser로 두면 30% 생성장치를 파괴할 수 있습니다.
/// 피해 타입은 인스턴스마다 바꿀 수 있습니다 (보스 기본 UDamageType_BossLaser, 함정은 필요하면 별도 타입 지정)
/// TODO(합의): 보스 GA에서 호출하는 방식(예고·추적 등)은 보스 담당과 확정 필요
/// TODO(합의): 플레이어가 TakeDamage → ApplyPlayerHit를 처리하게 되면 플레이어도 ApplyDamage로 통일 (지금은 직접 호출)

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "LaserBeamComponent.generated.h"

class ULaserBeamComponent;
class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLaserBeamStateChanged, ULaserBeamComponent*, Laser);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLaserBeamDamaged, ULaserBeamComponent*, Laser, AActor*, DamagedActor, float, Damage);


// 레이저의 직선 구간 하나
USTRUCT(BlueprintType)
struct MYNICELITTLECAT_API FLaserBeamSegment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LaserBeam")
	FVector Start = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "LaserBeam")
	FVector End = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "LaserBeam")
	FVector Direction = FVector::ForwardVector;

	// 이 구간 끝에서 발생한 충돌. 안 맞았으면 bBlockingHit == false
	UPROPERTY(BlueprintReadOnly, Category = "LaserBeam")
	FHitResult Hit;

	// 이 구간에 도달하기까지 반사된 횟수. 원본 구간은 0
	UPROPERTY(BlueprintReadOnly, Category = "LaserBeam")
	int32 ReflectionCount = 0;
};

UCLASS(ClassGroup = (Gimmick), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API ULaserBeamComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	ULaserBeamComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 레이저 발사: 현재 컴포넌트 위치에서 TargetLocation 방향 (Duration 0 이하면 LaserDuration 사용, 그것도 0이면 StopLaser까지)
	UFUNCTION(BlueprintCallable, Category = "LaserBeam")
	void StartLaser(const FVector& TargetLocation, float Duration = -1.f);

	// 레이저 발사: TargetActor의 현재 위치를 목표 지점으로 사용
	UFUNCTION(BlueprintCallable, Category = "LaserBeam")
	void StartLaserAtActor(AActor* TargetActor, float Duration = -1.f);

	// 레이저 발사: 컴포넌트 정면 방향 (함정 등 목표가 없는 레이저)
	UFUNCTION(BlueprintCallable, Category = "LaserBeam")
	void StartLaserForward(float Duration = -1.f);

	UFUNCTION(BlueprintCallable, Category = "LaserBeam")
	void StopLaser();

	// 발사 중 조준 변경 (GA가 추적·회전 등에 사용, 발사 중이 아니면 다음 발사 전까지 방향만 저장)
	UFUNCTION(BlueprintCallable, Category = "LaserBeam")
	void SetTargetLocation(const FVector& TargetLocation);

	UFUNCTION(BlueprintCallable, Category = "LaserBeam")
	void SetFireDirection(const FVector& Direction);

	UFUNCTION(BlueprintPure, Category = "LaserBeam")
	bool IsLaserActive() const { return bLaserActive; }

	UFUNCTION(BlueprintPure, Category = "LaserBeam")
	FVector GetFireDirection() const { return FireDirection; }

	// 이번 틱 빔 시작·끝 위치 (발사 중이 아니면 마지막 값)
	UFUNCTION(BlueprintPure, Category = "LaserBeam")
	FVector GetBeamStart() const { return BeamStart; }

	UFUNCTION(BlueprintPure, Category = "LaserBeam")
	FVector GetBeamEnd() const { return BeamEnd; }

	// 빔에 막힌 액터 (사거리 끝까지 아무것도 없으면 null)
	UFUNCTION(BlueprintPure, Category = "LaserBeam")
	AActor* GetHitActor() const { return HitActor.Get(); }

	// 빔이 막힌 면의 법선 (반사 방향 계산용, 막힌 곳이 없으면 0 벡터)
	// 반사 예: R = D - 2(D·N)N  (D = GetFireDirection(), N = GetHitNormal())
	UFUNCTION(BlueprintPure, Category = "LaserBeam")
	FVector GetHitNormal() const { return LastHit.bBlockingHit ? FVector(LastHit.ImpactNormal) : FVector::ZeroVector; }

	// 이번 틱 빔 트레이스 결과 전체 (맞은 지점·컴포넌트 등, 막힌 곳이 없으면 bBlockingHit false)
	UFUNCTION(BlueprintPure, Category = "LaserBeam")
	FHitResult GetHitResult() const { return LastHit; }

	UFUNCTION(BlueprintPure, Category = "LaserBeam")
	TArray<FLaserBeamSegment> GetBeamSegments() const { return BeamSegments; }

public:
	UPROPERTY(BlueprintAssignable, Category = "LaserBeam")
	FOnLaserBeamStateChanged OnLaserStarted;

	UPROPERTY(BlueprintAssignable, Category = "LaserBeam")
	FOnLaserBeamStateChanged OnLaserStopped;

	// 맞은 액터에 피해를 줬을 때 (플레이어는 무적이면 실제 피해 없이 무시될 수 있음)
	UPROPERTY(BlueprintAssignable, Category = "LaserBeam")
	FOnLaserBeamDamaged OnLaserDamaged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 발사 위치에서 빔 경로를 다시 계산
	void UpdateBeam();

	// 입력 방향을 발사 방향으로 정리 (bPlanarAim이면 수평, 정규화)
	FVector MakeFireDirection(const FVector& Direction) const;

	// 빔에 막힌 액터에 피해
	void ApplyLaserDamage();

	void DrawDebugBeam() const;

	// Classes가 비어 있으면 true, 아니면 그중 하나의 하위 클래스인지
	static bool IsActorOfClasses(const AActor* Actor, const TArray<TSubclassOf<AActor>>& Classes);

protected:
	// 발사 후 자동 종료까지의 시간 (0이면 StopLaser를 부를 때까지 유지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam", meta = (ClampMin = "0.0", Units = "s"))
	float LaserDuration = 0.f;

	// true면 목표 지점의 높이를 무시하고 발사 위치 높이에서 수평으로 발사 (쿼터뷰에서 바닥에 꽂히지 않게)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam")
	bool bPlanarAim = true;

	// true면 발사 중 매 틱 컴포넌트 정면을 발사 방향으로 사용 (회전하는 함정 등, 켜져 있으면 SetTargetLocation·SetFireDirection은 무시됨)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam")
	bool bFollowComponentRotation = false;

	// 빔 최대 길이
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Trace", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxRange = 4000.f;

	// 빔 굵기 (0이면 라인 트레이스, 0보다 크면 구 스윕)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Trace", meta = (ClampMin = "0.0", Units = "cm"))
	float BeamRadius = 5.f;

	// 레이저를 막는 오브젝트 타입 (플레이어·벽·기믹)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Trace")
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

	// 한 번 맞을 때 피해량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Damage", meta = (ClampMin = "0.0"))
	float DamagePerHit = 1.f;

	// 계속 닿아 있을 때 다시 피해를 주는 간격 (기본 1초에 1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Damage", meta = (ClampMin = "0.05", Units = "s"))
	float DamageInterval = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Damage")
	TSubclassOf<UDamageType> DamageTypeClass;

	// 이 클래스(하위 포함) 액터에만 피해 (비어 있으면 모든 액터)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Damage")
	TArray<TSubclassOf<AActor>> DamageTargetClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Debug")
	bool bDrawDebugBeam = true;

	// 한 번 이상 반사된 레이저가 사용할 피해 타입
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Damage")
	TSubclassOf<UDamageType> ReflectedDamageTypeClass;

	// 이 레이저가 반사면에서 반사될 수 있는지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Reflection")
	bool bEnableReflection = false;

	// 한 번의 경로 계산에서 허용할 최대 반사 횟수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Reflection", 
		meta = (ClampMin = "0", EditCondition = "bEnableReflection"))
	int32 MaxReflectionCount = 8;

	// 반사 직후 같은 표면을 즉시 다시 맞는 것을 피하기 위한 여유 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Reflection",
		meta = (ClampMin = "0.1", Units = "cm", EditCondition = "bEnableReflection"))
	float ReflectionStartOffset = 1.f;

private:
	FVector FireDirection = FVector::ForwardVector;

	FVector BeamStart = FVector::ZeroVector;

	FVector BeamEnd = FVector::ZeroVector;

	TWeakObjectPtr<AActor> HitActor;

	// 이번 틱 트레이스 결과 (막힌 곳이 없으면 bBlockingHit false)
	FHitResult LastHit;

	bool bLaserActive = false;

	// 0 이하면 시간 제한 없음
	float RemainingTime = 0.f;

	// 마지막으로 피해를 준 대상과 시각 (대상이 바뀌거나 음수면 닿는 즉시 피해)
	TWeakObjectPtr<AActor> LastDamagedActor;

	float LastDamageTime = -1.f;

	// 레이저 모음 배열
	UPROPERTY(Transient)
	TArray<FLaserBeamSegment> BeamSegments;
};
