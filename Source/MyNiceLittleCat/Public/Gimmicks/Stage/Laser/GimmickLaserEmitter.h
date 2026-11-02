/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 스테이지에 배치하는 레이저 발사기입니다 (스테이지 2 반사 퍼즐·함정).
/// 액터 정면(+X)으로 수평 레이저를 쏘며, 반사판·거울에서 반사됩니다 (ULaserBeamComponent 반사형).
/// 켜짐 상태 = 빔 발사 중이며, 동작은 EmitMode로 정합니다:
///   Always = 시작부터 계속 켜짐
///   Pulse  = OnTime 켜짐 / OffTime 꺼짐 반복 (함정용), 꺼지기 PulseWarningTime 전에 On Pulse Warning 이벤트
///   Signal = 다른 기믹의 신호로 켜고 끔 (수신기 A가 켜지면 발사기 B가 켜지는 연쇄 퍼즐 등, SignalRule·bInvertSignal 사용)
/// bDamagePlayer가 꺼져 있으면(퍼즐용 빛) 빔이 Pawn(플레이어·펫·몬스터)을 통과하고 피해를 주지 않습니다.
/// 켜져 있으면(함정) Pawn에 막히고 플레이어(bDamageEnemies면 몬스터도)에게 피해를 줍니다.
/// 빔 높이는 BeamHeight(기본 60cm, 스테이지 레이저 공통 높이)로 정하고, RotationSpeed로 제자리 회전할 수 있습니다.
/// Mesh(루트) = 받침, 레이저는 Laser 컴포넌트(발사 위치·방향)에서 나갑니다. 메시·빔 연출(Laser의 VisualSettings)은 BP에서 지정합니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/GimmickActorBase.h"
#include "GimmickLaserEmitter.generated.h"

class UStaticMeshComponent;
class ULaserBeamComponent;

UENUM(BlueprintType)
enum class EGimmickLaserEmitMode : uint8
{
	// 시작부터 계속 켜짐
	Always,
	// 켜짐·꺼짐 반복
	Pulse,
	// 다른 기믹의 신호로 켜고 끔
	Signal
};

UCLASS()
class MYNICELITTLECAT_API AGimmickLaserEmitter : public AGimmickActorBase
{
	GENERATED_BODY()

public:
	AGimmickLaserEmitter();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Gimmick|LaserEmitter")
	ULaserBeamComponent* GetLaser() const { return Laser; }

	UFUNCTION(BlueprintPure, Category = "Gimmick|LaserEmitter")
	EGimmickLaserEmitMode GetEmitMode() const { return EmitMode; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator) override;

	// 모드·피해 옵션을 베이스 설정과 레이저에 반영
	void ApplyEmitterSettings();

	// Pulse: 다음 단계(켜짐/꺼짐)로 전환하고 타이머 예약
	void AdvancePulse();

	void HandlePulseWarning();

	// Pulse: 꺼지기 직전 경고 (BP에서 깜빡임·소리 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|LaserEmitter", meta = (DisplayName = "On Pulse Warning"))
	void K2_OnPulseWarning();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// 빔이 나가는 위치·방향 (정면 +X)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULaserBeamComponent> Laser;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserEmitter")
	EGimmickLaserEmitMode EmitMode = EGimmickLaserEmitMode::Always;

	// 빔 높이 (액터 원점 = 바닥 기준). 펫 반사판·거울·수신기와 맞추기 위해 스테이지 공통 60cm
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserEmitter", meta = (Units = "cm"))
	float BeamHeight = 60.f;

	// Pulse: 켜져 있는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserEmitter|Pulse", meta = (ClampMin = "0.1", Units = "s", EditCondition = "EmitMode == EGimmickLaserEmitMode::Pulse", EditConditionHides))
	float PulseOnTime = 2.f;

	// Pulse: 꺼져 있는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserEmitter|Pulse", meta = (ClampMin = "0.1", Units = "s", EditCondition = "EmitMode == EGimmickLaserEmitMode::Pulse", EditConditionHides))
	float PulseOffTime = 1.5f;

	// Pulse: 시작 시점 밀기 (여러 발사기를 엇박자로 둘 때). 0 = 켜짐부터 시작
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserEmitter|Pulse", meta = (ClampMin = "0.0", Units = "s", EditCondition = "EmitMode == EGimmickLaserEmitMode::Pulse", EditConditionHides))
	float PulseStartOffset = 0.f;

	// Pulse: 꺼지기 몇 초 전에 경고 이벤트를 보낼지 (0이면 경고 없음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserEmitter|Pulse", meta = (ClampMin = "0.0", Units = "s", EditCondition = "EmitMode == EGimmickLaserEmitMode::Pulse", EditConditionHides))
	float PulseWarningTime = 0.5f;

	// true면 함정: Pawn에 막히고 플레이어에게 피해. false면 퍼즐용 빛: Pawn을 통과하고 피해 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserEmitter|Damage")
	bool bDamagePlayer = false;

	// 함정일 때 몬스터도 피해
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserEmitter|Damage", meta = (EditCondition = "bDamagePlayer"))
	bool bDamageEnemies = false;

	// 함정일 때 한 번 맞을 때 피해량 (플레이어 체력 3 기준 1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserEmitter|Damage", meta = (ClampMin = "0.0", EditCondition = "bDamagePlayer"))
	float DamagePerHit = 1.f;

	// 제자리 회전 속도 (도/초, 0이면 고정, 음수면 반대 방향)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserEmitter", meta = (Units = "deg"))
	float RotationSpeed = 0.f;

private:
	FTimerHandle PulseTimer;

	FTimerHandle PulseWarningTimer;
};
