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
/// 연출: VisualSettings.BeamEffect(Niagara)가 있으면 빔 구간마다 이펙트를 하나씩 켜고 매 틱 시작·끝·굵기·색 User 파라미터를 갱신합니다.
/// ImpactEffect는 마지막 구간이 막힌 지점에 표시합니다. 코드로 만드는 레이저(격자 패턴 등)는 SetVisualSettings로 지정합니다.
/// 빔 시작·끝·맞은 액터는 GetBeamStart/GetBeamEnd/GetHitActor로도 조회할 수 있고, 디버그 라인(bDrawDebugBeam)은 필요할 때만 켭니다.
/// 반사판(IReflectiveSurfaceInterface)을 만났을 때 레이저 종류 (팀 합의 2026-10-27):
/// - 반사형 bEnableReflection: 반사판에서 반사되고, 반사된 구간은 ReflectedDamageTypeClass로 피해 → 30% 생성장치 파괴 가능.
///   스테이지 기믹·함정, 보스 단일 레이저용. bPlanarReflection(기본 true)이면 반사된 구간은 수평으로 진행
/// - 파괴형 bBreakReflectors: 반사하지 않고 반사판에 BreakByLaser를 호출해 부숨 (펫 반사판을 방패로 못 쓰게). 보스 격자 레이저용
/// - 둘 다 꺼짐: 반사판도 일반 물체처럼 빔을 막음. 둘 다 켜지면 반사가 우선
/// 맞은 면 법선(GetHitNormal)·트레이스 결과(GetHitResult)·구간 목록(GetBeamSegments)을 제공합니다.
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
class UNiagaraSystem;
class UNiagaraComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLaserBeamStateChanged, ULaserBeamComponent*, Laser);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLaserBeamDamaged, ULaserBeamComponent*, Laser, AActor*, DamagedActor, float, Damage);


// 레이저 빔 연출 설정 (Niagara). 파라미터 이름은 Niagara 시스템의 User 파라미터 이름과 맞춰야 합니다
USTRUCT(BlueprintType)
struct MYNICELITTLECAT_API FLaserBeamVisualSettings
{
	GENERATED_BODY()

	// 빔 구간마다 하나씩 켜는 이펙트 (비어 있으면 빔 연출 없음)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserBeam|Visual")
	TObjectPtr<UNiagaraSystem> BeamEffect;

	// 빔이 막힌 지점에 표시할 이펙트 (선택, 마지막 구간 기준, 법선 방향으로 회전)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserBeam|Visual")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	// 원본 구간 색
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserBeam|Visual")
	FLinearColor BeamColor = FLinearColor(1.f, 0.1f, 0.05f);

	// 한 번 이상 반사된 구간 색
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserBeam|Visual")
	FLinearColor ReflectedBeamColor = FLinearColor(0.1f, 0.6f, 1.f);

	// 보이는 굵기 = BeamRadius × 2 × 배율 (판정 굵기와 따로 조절, 격자처럼 판정이 넓은 레이저용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserBeam|Visual", meta = (ClampMin = "0.0"))
	float BeamWidthScale = 1.f;

	// 보이는 굵기 최솟값 (BeamRadius 0인 선 트레이스 레이저용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserBeam|Visual", meta = (ClampMin = "0.0", Units = "cm"))
	float MinBeamWidth = 4.f;

	// Niagara User 파라미터 이름 (Vector, 월드 좌표). 시스템에 없으면 무시됨 (시작점 = 이펙트 위치를 쓰는 시스템 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserBeam|Visual|Parameters")
	FName BeamStartParam = TEXT("BeamStart");

	// Niagara User 파라미터 이름 (Vector, 월드 좌표)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserBeam|Visual|Parameters")
	FName BeamEndParam = TEXT("BeamEnd");

	// Niagara User 파라미터 이름 (Float)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserBeam|Visual|Parameters")
	FName BeamWidthParam = TEXT("BeamWidth");

	// Niagara User 파라미터 이름 (Linear Color, 빔·충돌 이펙트 공통)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserBeam|Visual|Parameters")
	FName BeamColorParam = TEXT("BeamColor");
};


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

	// 이 구간 끝에서 설치 예정 반사판에 맞았는지
	UPROPERTY(BlueprintReadOnly, Category = "LaserBeam")
	bool bHitVirtualSurface = false;
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

	// 사거리 변경 (패턴이 영역 끝까지만 쏘게 할 때 등)
	UFUNCTION(BlueprintCallable, Category = "LaserBeam")
	void SetMaxRange(float NewMaxRange) { MaxRange = FMath::Max(NewMaxRange, 1.f); }

	// 빔 굵기 변경 (0이면 선 트레이스)
	UFUNCTION(BlueprintCallable, Category = "LaserBeam")
	void SetBeamRadius(float NewBeamRadius) { BeamRadius = FMath::Max(NewBeamRadius, 0.f); }

	UFUNCTION(BlueprintPure, Category = "LaserBeam")
	float GetBeamRadius() const { return BeamRadius; }

	// 한 번 맞을 때 피해량 변경 (보스 패턴 GA가 보스 공격력으로 계산한 값을 넣음)
	UFUNCTION(BlueprintCallable, Category = "LaserBeam")
	void SetDamagePerHit(float NewDamagePerHit) { DamagePerHit = FMath::Max(NewDamagePerHit, 0.f); }

	UFUNCTION(BlueprintPure, Category = "LaserBeam")
	float GetDamagePerHit() const { return DamagePerHit; }

	// 연출 설정 교체 (코드로 만든 레이저용, 발사 중이면 이펙트를 새 설정으로 다시 만듦)
	UFUNCTION(BlueprintCallable, Category = "LaserBeam|Visual")
	void SetVisualSettings(const FLaserBeamVisualSettings& NewSettings);

	UFUNCTION(BlueprintPure, Category = "LaserBeam|Visual")
	const FLaserBeamVisualSettings& GetVisualSettings() const { return VisualSettings; }

	UFUNCTION(BlueprintCallable, Category = "LaserBeam|Debug")
	void SetDrawDebugBeam(bool bDraw) { bDrawDebugBeam = bDraw; }

	// 반사판 파괴형으로 바꿈 (격자 패턴 등 코드로 만드는 레이저용)
	UFUNCTION(BlueprintCallable, Category = "LaserBeam|Reflection")
	void SetBreakReflectors(bool bBreak) { bBreakReflectors = bBreak; }

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

	// VirtualSurface가 없으면 기존 실제 경로만 계산
	void CalculateBeamPath(TArray<FLaserBeamSegment>& OutSegments,
		const struct FVirtualReflectiveSurface* VirtualSurface = nullptr) const;

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

	// 빔 구간 수만큼 이펙트를 켜고 위치·굵기·색 갱신, 충돌 이펙트 위치 갱신
	void UpdateBeamEffects();

	// 이펙트 끄기 (컴포넌트는 다음 발사 때 재사용)
	void DeactivateBeamEffects();

	// 이펙트 컴포넌트 제거 (설정 교체·EndPlay)
	void DestroyBeamEffects();

	UNiagaraComponent* CreateEffectComponent(UNiagaraSystem* System);

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Visual")
	FLaserBeamVisualSettings VisualSettings;

	// 디버그 라인 표시 (Development 빌드에서도 비용이 있으니 확인할 때만 켬)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Debug")
	bool bDrawDebugBeam = false;

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

	// true면 반사된 구간의 위아래 성분을 없애 수평으로 진행 (쿼터뷰에서 비스듬히 내려 쏜 레이저도 반사 후 바닥에 꽂히지 않게).
	// 반사 지점 높이에서 수평으로 나가며, 수직으로 들어오면(수평 성분 없음) 반사하지 않고 끝남
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Reflection", meta = (EditCondition = "bEnableReflection"))
	bool bPlanarReflection = true;

	// true면 반사판(IReflectiveSurfaceInterface)에 맞았을 때 반사하지 않고 BreakByLaser로 부숨 (반사판에는 피해 없음).
	// bEnableReflection이 켜져 있으면 반사가 우선이라 이 값은 무시됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Reflection", meta = (EditCondition = "!bEnableReflection"))
	bool bBreakReflectors = false;

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

	// 빔 구간별 이펙트 (구간 수보다 많으면 남는 것은 꺼 둠)
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> BeamEffectComponents;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ImpactEffectComponent;
};
